"""Tests for RoverLink's transport supervision (rover_esp32/link.py).

These drive a real TCP server in-process and talk to it through
pyserial's "socket://" URL handler -- the exact transport the deported
Pi uses (ARCHITECTURE_AND_ROADMAP.md §6.2), so the reconnection
behaviour is exercised for real rather than against a mock that would
just re-encode this file's own assumptions.

No ESP32 and no serial port involved: the server here plays the role of
the firmware's future WiFiServer.
"""
from __future__ import annotations

import socket
import threading
import time

from rover_esp32.link import RoverLink, resolve_link_secret
from rover_esp32.protocol import decode_frame, encode_frame

# Generous enough not to flake on a loaded CI box, short enough that a
# genuine failure doesn't hang the suite. RoverLink's own backoff starts
# at 0.5s, so anything waiting on a *reconnection* needs to clear that.
CONNECT_TIMEOUT_S = 5.0
POLL_S = 0.02


def _wait_until(predicate, timeout=CONNECT_TIMEOUT_S) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(POLL_S)
    return False


class FakeEsp32:
    """Minimal stand-in for the firmware's TCP listener: accepts one
    client at a time and lets a test send frames to it or hang up."""

    def __init__(self) -> None:
        self._server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind(("127.0.0.1", 0))
        self._server.listen(1)
        self.port = self._server.getsockname()[1]
        self.connections = 0
        # Every frame this "robot" received, decoded, in order -- so a
        # test can assert on what the Pi actually put on the wire
        # (notably the AUTH frame) rather than only on what came back.
        self.received: list[tuple[str, dict[str, str]]] = []
        self._client: socket.socket | None = None
        self._lock = threading.Lock()
        self._stopping = threading.Event()
        self._thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._thread.start()

    def _accept_loop(self) -> None:
        while not self._stopping.is_set():
            try:
                client, _ = self._server.accept()
            except OSError:
                return
            with self._lock:
                self._client = client
                self.connections += 1
            threading.Thread(target=self._read_loop, args=(client,), daemon=True).start()

    def _read_loop(self, client: socket.socket) -> None:
        """Collects whatever the Pi sends. Without this the socket's
        receive buffer just fills silently and nothing can be asserted
        about outgoing frames."""
        buffer = b""
        while not self._stopping.is_set():
            try:
                chunk = client.recv(4096)
            except OSError:
                return
            if not chunk:
                return
            buffer += chunk
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                try:
                    frame = decode_frame(line.decode("ascii").strip())
                except Exception:  # noqa: BLE001 -- a malformed line is a test failure, not a crash
                    continue
                with self._lock:
                    self.received.append(frame)

    def send_frame(self, frame_type: str, fields: dict[str, str] | None = None) -> None:
        with self._lock:
            client = self._client
        assert client is not None, "no client connected"
        client.sendall((encode_frame(frame_type, fields) + "\n").encode("ascii"))

    def hang_up(self) -> None:
        """Drops the current client, simulating a WiFi/TCP failure."""
        with self._lock:
            client, self._client = self._client, None
        if client is not None:
            client.close()

    def close(self) -> None:
        self._stopping.set()
        self.hang_up()
        self._server.close()

    @property
    def url(self) -> str:
        return f"socket://127.0.0.1:{self.port}"


def test_connects_and_decodes_incoming_frames():
    server = FakeEsp32()
    link = RoverLink(server.url)
    received: list[tuple[str, dict[str, str]]] = []
    link.on_frame = lambda frame_type, fields: received.append((frame_type, fields))
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected), "link never came up"

        server.send_frame("STATE", {"distance_left": "420"})
        assert _wait_until(lambda: received), "frame never arrived"
        assert received[0] == ("STATE", {"distance_left": "420"})
    finally:
        link.stop()
        server.close()


def test_start_does_not_raise_when_nothing_is_listening():
    """The control server must still come up when the robot is simply
    switched off -- over WiFi that is an ordinary startup condition, not
    a fatal one (it used to raise out of RoverLink.__init__)."""
    # Bind and immediately release a port to get one nothing listens on.
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.bind(("127.0.0.1", 0))
    dead_port = probe.getsockname()[1]
    probe.close()

    link = RoverLink(f"socket://127.0.0.1:{dead_port}")
    try:
        link.start()  # must not raise
        time.sleep(0.2)
        assert not link.is_connected
        # And a command sent into the void is dropped, not an exception.
        assert link.send("MOVE", {"velocity": "0.20"}) is False
    finally:
        link.stop()


def test_reconnects_after_the_link_drops():
    """The regression that matters most for the deported Pi: a dropped
    socket (AP roaming, ESP32 reboot after an OTA flash) must heal on
    its own, without restarting the Pi process."""
    server = FakeEsp32()
    link = RoverLink(server.url)
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected), "link never came up"
        assert server.connections == 1

        server.hang_up()
        assert _wait_until(lambda: not link.is_connected), "link never noticed the drop"
        assert _wait_until(lambda: link.is_connected), "link never reconnected"
        assert server.connections == 2
    finally:
        link.stop()
        server.close()


def test_frames_flow_again_after_a_reconnection():
    """Reconnecting is only useful if the new connection is actually
    wired up to on_frame -- guards against reconnecting to a socket
    nobody reads."""
    server = FakeEsp32()
    link = RoverLink(server.url)
    received: list[tuple[str, dict[str, str]]] = []
    link.on_frame = lambda frame_type, fields: received.append((frame_type, fields))
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected)
        server.hang_up()
        assert _wait_until(lambda: not link.is_connected)
        assert _wait_until(lambda: link.is_connected), "link never reconnected"

        server.send_frame("EVENT", {"name": "obstacle_detected"})
        assert _wait_until(lambda: received), "no frame after reconnection"
        assert received[-1] == ("EVENT", {"name": "obstacle_detected"})
    finally:
        link.stop()
        server.close()


def test_send_reports_failure_instead_of_raising_while_down():
    """RoverCore's heartbeat task only catches CancelledError: a raising
    send() would kill it permanently, and it would never resume
    heartbeating even once the network came back. Silence is the
    contract -- the ESP32's own heartbeat timeout is what stops the
    motors (ROVER_PROTOCOL.md §6)."""
    server = FakeEsp32()
    link = RoverLink(server.url)
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected)
        assert link.send("HEARTBEAT") is True

        server.hang_up()
        assert _wait_until(lambda: not link.is_connected)
        # Must return False rather than raise, however often it's called.
        for _ in range(3):
            assert link.send("HEARTBEAT") is False
    finally:
        link.stop()
        server.close()


def test_on_frame_exception_does_not_kill_the_link():
    """Regression guard, same class as the 2026-09-10 non-ASCII bug (see
    test_protocol.py): pyserial's ReaderThread treats an exception from
    data_received as a fatal connection error, so a bug in the consumer
    used to take the whole link down silently."""
    server = FakeEsp32()
    link = RoverLink(server.url)
    seen: list[str] = []

    def exploding_on_frame(frame_type: str, fields: dict[str, str]) -> None:
        seen.append(frame_type)
        raise RuntimeError("consumer bug")

    link.on_frame = exploding_on_frame
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected)

        server.send_frame("STATE", {"a": "1"})
        assert _wait_until(lambda: len(seen) == 1)
        # The link must survive and keep delivering.
        server.send_frame("STATE", {"a": "2"})
        assert _wait_until(lambda: len(seen) == 2), "link died on a consumer exception"
        assert link.is_connected
    finally:
        link.stop()
        server.close()


def test_stop_is_idempotent_and_leaves_no_thread_running():
    server = FakeEsp32()
    link = RoverLink(server.url)
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected)
        link.stop()
        link.stop()  # must not raise
        assert not link.is_connected
        assert not any(t.name == "rover-link" for t in threading.enumerate())
    finally:
        server.close()


def test_repeated_start_stop_cycles_leak_no_threads():
    """Guards a race found while writing these tests: stop() arriving
    while the supervisor was mid-connect used to leave a reader thread
    nobody would close, with the supervisor blocked in join() forever.
    Cycling start/stop with no settling time is what exposes it."""
    server = FakeEsp32()
    try:
        for _ in range(6):
            link = RoverLink(server.url)
            link.start()
            # Deliberately no wait: stop() should land at an arbitrary
            # point of the connect sequence, including mid-handshake.
            link.stop()
            assert not link.is_connected
        assert not any(t.name == "rover-link" for t in threading.enumerate())
    finally:
        server.close()


def test_can_be_restarted_after_stop():
    server = FakeEsp32()
    link = RoverLink(server.url)
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected)
        link.stop()
        assert not link.is_connected

        link.start()
        assert _wait_until(lambda: link.is_connected), "link did not come back after restart"
    finally:
        link.stop()
        server.close()


# --- Link authentication (ARCHITECTURE_AND_ROADMAP.md §6.2 "5 bis") ---
#
# The ESP32 refuses every command on a network link until a shared
# secret has been presented (esp32/lib/communication/LinkAuth.h). These
# check the Pi's half of that contract, against the same in-process TCP
# server as everything above.


def test_authenticates_on_connection_before_anything_else():
    server = FakeEsp32()
    link = RoverLink(server.url, secret="correct-horse")
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected), "link never came up"
        assert _wait_until(lambda: server.received), "nothing was sent"

        # First frame on the wire, not merely present somewhere in it:
        # the robot rejects and reports anything that arrives before
        # AUTH, so an AUTH sent second would already have cost a frame.
        assert server.received[0] == ("AUTH", {"secret": "correct-horse"})
    finally:
        link.stop()
        server.close()


def test_reauthenticates_after_a_reconnection():
    """Authentication is per-connection on the robot: it forgets on
    every disconnection, so a link that comes back must prove itself
    again or every subsequent command is refused. Over WiFi a drop is
    routine, which makes this the common path rather than an edge case."""
    server = FakeEsp32()
    link = RoverLink(server.url, secret="correct-horse")
    try:
        link.start()
        assert _wait_until(lambda: server.connections == 1), "link never came up"
        assert _wait_until(lambda: len(server.received) == 1), "no initial AUTH"

        server.hang_up()
        assert _wait_until(lambda: server.connections == 2), "never reconnected"

        assert _wait_until(lambda: len(server.received) == 2), "did not re-authenticate"
        assert server.received[1] == ("AUTH", {"secret": "correct-horse"})
    finally:
        link.stop()
        server.close()


def test_no_secret_sends_no_auth_frame():
    """The USB cable case, still the default: physical access is the
    authentication there and the firmware requires none, so the Pi must
    not start emitting a frame the robot never asked for."""
    server = FakeEsp32()
    link = RoverLink(server.url)  # no secret
    try:
        link.start()
        assert _wait_until(lambda: link.is_connected), "link never came up"

        link.send("MOVE", {"velocity": "0.20"})
        assert _wait_until(lambda: server.received), "nothing was sent"
        assert server.received[0][0] == "MOVE"
        assert not any(frame_type == "AUTH" for frame_type, _ in server.received)
    finally:
        link.stop()
        server.close()


def test_resolve_link_secret_reads_the_environment(monkeypatch):
    monkeypatch.delenv("ROVER_LINK_SECRET", raising=False)
    assert resolve_link_secret() is None

    # Empty is treated as unset rather than as a real (empty) secret --
    # an empty secret would be refused by the robot anyway, and silently
    # sending one would obscure why.
    monkeypatch.setenv("ROVER_LINK_SECRET", "")
    assert resolve_link_secret() is None

    monkeypatch.setenv("ROVER_LINK_SECRET", "correct-horse")
    assert resolve_link_secret() == "correct-horse"
