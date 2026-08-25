"""Owns the serial connection to the ESP32 and speaks Rover Protocol.

pyserial has no first-class asyncio support, so this runs its own
background reader thread (serial.threaded.ReaderThread); every decoded
frame is handed to `self.on_frame(frame_type, fields)` from *that*
thread. Callers that touch asyncio state from there must hop back onto
the event loop themselves -- see rover_core.core.RoverCore.on_frame,
which is the only intended consumer.
"""
from __future__ import annotations

import logging

import serial
import serial.threaded

from .protocol import FrameError, decode_frame, encode_frame

logger = logging.getLogger(__name__)


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
        self._link.on_frame(frame_type, fields)


class RoverLink:
    """port accepts anything pyserial's serial_for_url understands: a
    real device path (e.g. "/dev/ttyUSB0") for the physical robot, or a
    URL like "rfc2217://localhost:4000" to drive a Wokwi simulation
    instead -- see esp32/wokwi.toml's rfc2217ServerPort. Same code path
    either way, which is the point.
    """

    def __init__(self, port: str, baudrate: int = 115200) -> None:
        self._serial = serial.serial_for_url(port, baudrate=baudrate, timeout=1)
        self._reader = serial.threaded.ReaderThread(self._serial, lambda: _LineHandler(self))
        # Public and reassignable: the reader thread looks this up fresh
        # on every line, so the owner can set it any time (before or
        # after start()) without ordering games at construction time.
        self.on_frame = lambda frame_type, fields: None

    def start(self) -> None:
        self._reader.start()

    def stop(self) -> None:
        self._reader.close()

    def send(self, frame_type: str, fields: dict[str, str] | None = None) -> None:
        line = encode_frame(frame_type, fields)
        logger.debug("-> %s", line)
        self._serial.write((line + "\n").encode("ascii"))
