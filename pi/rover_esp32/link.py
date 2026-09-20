"""Owns the link to the ESP32 and speaks Rover Protocol.

Transport-agnostic by design: `port` accepts anything pyserial's
serial_for_url understands, which is what made the 2026-09-15 move to a
deported Pi (ARCHITECTURE_AND_ROADMAP.md §6.2) nearly free on this side.

    /dev/ttyUSB0              real robot, historical USB cable
    socket://rover.local:3333 real robot, Pi deported over WiFi
    rfc2217://localhost:4000  Wokwi simulation (esp32/wokwi.toml)

pyserial has no first-class asyncio support, so this runs its own
background threads; every decoded frame is handed to
`self.on_frame(frame_type, fields)` from *that* thread. Callers that
touch asyncio state from there must hop back onto the event loop
themselves -- see rover_core.core.RoverCore.on_frame, which is the only
intended consumer.

Why this file reconnects at all
-------------------------------
Over the old USB cable, the port was either present at startup or not,
and a mid-run disconnection meant someone had physically unplugged the
robot. Over a WiFi socket, neither holds: the ESP32 may still be
booting when the Pi starts, it may reboot after an OTA flash, and the
link may drop and come back on its own (AP roaming, interference).

So connecting is a *supervised, retrying* operation rather than a
one-shot in __init__, and `send()` reports failure instead of raising.
That last point is deliberate: a raising send() would escape into
RoverCore's heartbeat task, which only catches CancelledError -- the
task would die permanently and never resume heartbeating even once the
network came back. The same class of bug (an exception escaping into a
pyserial thread and killing it silently) already bit this project once
on real hardware, see pi/tests/test_protocol.py.

Authenticating
--------------
A USB cable authenticated itself: to send a MOVE you had to be in the
room. A TCP socket does not, so the ESP32 requires a shared secret
before it will accept any command on a network link (see
esp32/lib/communication/LinkAuth.h and ARCHITECTURE_AND_ROADMAP.md
§6.2 "5 bis"). This class presents it on every (re)connection, because
the robot forgets it on every disconnection -- authentication is
per-connection there, so that a link which drops and comes back is
treated as a new peer.

The secret comes from the ROVER_LINK_SECRET environment variable,
never from config.json: that file is plain text on disk and its own
docstring rules out secrets, which is why the control token doesn't
live there either.

Dropping frames while the link is down is correct, not a workaround:
the ESP32 stops the motors on its own when HEARTBEAT stops arriving
(ROVER_PROTOCOL.md §6), and "la sécurité ne doit jamais dépendre du
Raspberry Pi". A Pi that cannot reach the robot must fail silent, not
queue up stale commands to replay later.
"""
from __future__ import annotations

import logging
import os
import threading

import serial
import serial.threaded

from .protocol import FrameError, decode_frame, encode_frame

logger = logging.getLogger(__name__)

# Reconnection backoff. Starts short so an ESP32 that is merely a few
# seconds behind the Pi at boot is picked up almost immediately, then
# backs off so a robot that is simply switched off doesn't fill the log
# with one attempt per second all day. Reset to the minimum after every
# successful connection.
RECONNECT_MIN_DELAY_S = 0.5
RECONNECT_MAX_DELAY_S = 5.0


def resolve_link_secret() -> str | None:
    """The shared secret the ESP32 expects on a network link, or None.

    Unlike the control server's token (rover_control/auth.py), this one
    cannot be generated on the fly when unset: it has to match what is
    stored on the robot, so there is nothing sensible to invent. None
    means "send no AUTH frame", which is exactly right over the USB
    cable -- the ESP32 requires none there.
    """
    secret = os.environ.get("ROVER_LINK_SECRET")
    return secret or None


class _LineHandler(serial.threaded.LineReader):
    TERMINATOR = b"\n"

    def __init__(self, link: "RoverLink") -> None:
        super().__init__()
        self._link = link

    def handle_line(self, line: str) -> None:
        try:
            frame_type, fields = decode_frame(line)
        except FrameError as exc:
            logger.warning("dropping invalid frame from ESP32: %s (line=%r)", exc, line)
            return
        try:
            self._link.on_frame(frame_type, fields)
        except Exception:  # noqa: BLE001 -- see comment below
            # Never let a consumer-side bug propagate: pyserial's
            # ReaderThread treats an exception from data_received as a
            # fatal connection error and exits, which would silently
            # take the whole link down (and, before this file
            # supervised itself, kept it down). Logged and swallowed so
            # one bad frame can't cost us the connection.
            logger.exception("on_frame callback failed for %s %s", frame_type, fields)

    def connection_lost(self, exc: BaseException | None) -> None:
        """pyserial's base Protocol re-raises `exc` here, which surfaces
        as an unhandled exception (and a full traceback on stderr) in
        the reader thread. Over USB that happened once, when someone
        unplugged the robot; over WiFi an ordinary drop is routine, so
        the default would spam a traceback on every hiccup. The
        supervisor already handles reconnection -- log and return."""
        if exc is not None:
            logger.debug("reader thread ended: %s", exc)


class RoverLink:
    """Connects lazily and keeps reconnecting until stop() is called.

    Public API is unchanged from the pre-2026-09-15 version (start,
    stop, send, on_frame) so RoverCore and rover_core.main needed no
    modification -- only `send()` gained a return value, which existing
    callers are free to ignore.
    """

    def __init__(self, port: str, baudrate: int = 115200, secret: str | None = None) -> None:
        self._port = port
        self._baudrate = baudrate
        # None = send no AUTH frame at all, which is what the USB cable
        # wants (the ESP32 requires none there). See resolve_link_secret.
        self._secret = secret
        # Guards the _serial/_reader pair only. Held just long enough to
        # read or swap the pointers, never across a blocking write, so a
        # write stalling on a half-dead socket can't block the
        # supervisor from tearing that same socket down.
        self._lock = threading.Lock()
        self._serial: serial.SerialBase | None = None
        self._reader: serial.threaded.ReaderThread | None = None
        self._stopping = threading.Event()
        self._supervisor: threading.Thread | None = None
        # Public and reassignable: the reader thread looks this up fresh
        # on every line, so the owner can set it any time (before or
        # after start()) without ordering games at construction time.
        self.on_frame = lambda frame_type, fields: None

    @property
    def is_connected(self) -> bool:
        with self._lock:
            return self._serial is not None

    def start(self) -> None:
        """Returns immediately -- it does NOT wait for, or require, a
        successful connection. The Pi's control server must come up and
        stay useful (web UI, camera, AI panels) even while the robot
        itself is unreachable, which over WiFi is an ordinary and
        temporary condition rather than a fatal one."""
        if self._supervisor is not None:
            return
        self._stopping.clear()
        self._supervisor = threading.Thread(
            target=self._supervise, name="rover-link", daemon=True
        )
        self._supervisor.start()

    def stop(self) -> None:
        # Both under the lock, so this can't interleave with the
        # supervisor's own check-and-publish (see _supervise).
        with self._lock:
            self._stopping.set()
            reader = self._reader
        if reader is not None:
            # Makes the reader thread exit its read loop, which in turn
            # unblocks the supervisor's join().
            reader.close()
        supervisor, self._supervisor = self._supervisor, None
        if supervisor is not None:
            supervisor.join(timeout=RECONNECT_MAX_DELAY_S + 1.0)
            if supervisor.is_alive():
                # Never expected; worth a loud line rather than a
                # silently leaked thread if it ever happens.
                logger.error("ESP32 link supervisor did not stop within timeout")

    def send(self, frame_type: str, fields: dict[str, str] | None = None) -> bool:
        """Best-effort. Returns False (and logs) instead of raising when
        the link is down -- see this module's docstring for why silence
        is the correct failure mode here."""
        line = encode_frame(frame_type, fields)
        with self._lock:
            port = self._serial
        if port is None:
            logger.debug("link down, dropping %s", frame_type)
            return False
        try:
            port.write((line + "\n").encode("ascii"))
        except (serial.SerialException, OSError) as exc:
            # The supervisor notices the same failure through the reader
            # thread and handles reconnection; nothing to do here but
            # report it. Not .exception(): on a flaky link this would be
            # every frame, and the stack trace adds nothing.
            logger.warning("link write failed (%s), dropping %s", exc, frame_type)
            return False
        logger.debug("-> %s", line)
        return True

    # --- internals -----------------------------------------------------

    def _supervise(self) -> None:
        """Connect, pump frames until the link dies, repeat."""
        delay = RECONNECT_MIN_DELAY_S
        while not self._stopping.is_set():
            port = self._open()
            if port is None:
                # Event.wait() rather than sleep() so stop() interrupts
                # a long backoff instead of having to wait it out.
                self._stopping.wait(delay)
                delay = min(delay * 2, RECONNECT_MAX_DELAY_S)
                continue

            delay = RECONNECT_MIN_DELAY_S
            reader = serial.threaded.ReaderThread(port, lambda: _LineHandler(self))
            # Checking _stopping and publishing _reader must be atomic
            # against stop(), which sets _stopping and reads _reader
            # under this same lock. Otherwise stop() can slip in between
            # the two and see _reader still None, leaving a reader
            # thread nobody will ever close -- and this supervisor
            # blocked forever in join() below, leaking past stop().
            with self._lock:
                if self._stopping.is_set():
                    port.close()
                    return
                self._serial = port
                self._reader = reader
            logger.info("ESP32 link up on %s", self._port)

            reader.start()
            # Only now, with the reader running, so the robot's reply
            # ("STATE link=authenticated", or ERROR code=unauthenticated
            # / link_secret_not_set) is actually seen and logged rather
            # than sitting unread in a buffer. Sent on every connection
            # because the ESP32 forgets it on every disconnection.
            self._authenticate()
            # Returns when the transport fails (pyserial's ReaderThread
            # exits its loop on SerialException) or when stop() closed
            # it deliberately.
            reader.join()

            with self._lock:
                self._serial = None
                self._reader = None
            if not self._stopping.is_set():
                logger.warning("ESP32 link lost on %s, reconnecting", self._port)
                # Settle before retrying. Without this, an ESP32 that
                # accepts a connection and immediately drops it (mid-
                # reboot, or one client slot already taken) would spin
                # this loop as fast as the network allows.
                self._stopping.wait(RECONNECT_MIN_DELAY_S)

    def _authenticate(self) -> None:
        """Best-effort, like send(): a failure here is reported and left
        to the supervisor, which will reconnect and try again. Nothing
        waits for the reply -- the robot simply refuses every subsequent
        frame if this did not land, and the ERROR it sends back says so
        in the log."""
        if self._secret is None:
            logger.debug("no ROVER_LINK_SECRET set, not authenticating (expected over USB)")
            return
        if self.send("AUTH", {"secret": self._secret}):
            logger.info("sent AUTH to the robot")
        else:
            logger.warning("could not send AUTH -- the robot will refuse every command")

    def _open(self) -> serial.SerialBase | None:
        try:
            return serial.serial_for_url(self._port, baudrate=self._baudrate, timeout=1)
        except (serial.SerialException, OSError) as exc:
            # Expected while the robot is off or still booting, so this
            # is not an error-level event -- it is the normal state of a
            # deported Pi whose robot isn't switched on yet.
            logger.warning("cannot reach ESP32 on %s (%s), retrying", self._port, exc)
            return None
