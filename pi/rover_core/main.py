"""Entrypoint: wires the ESP32 link, RoverCore and the control web
server together, and runs them until interrupted.

    python -m rover_core.main --port /dev/ttyUSB0
    python -m rover_core.main --port rfc2217://localhost:4000   # Wokwi

See pi/README.md for setup and the Wokwi testing workflow.
"""
from __future__ import annotations

import argparse
import asyncio
import logging

from aiohttp import web

from rover_esp32.link import RoverLink
from rover_control.server import create_app

from .core import RoverCore

logger = logging.getLogger("rover_core")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Rover Pi core: ESP32 bridge + control server")
    parser.add_argument(
        "--port",
        default="/dev/ttyUSB0",
        help='Serial port to the ESP32 (Rover Protocol UART), e.g. "/dev/ttyUSB0" '
        'on the real robot or "rfc2217://localhost:4000" against a Wokwi simulation.',
    )
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--http-host", default="0.0.0.0")
    parser.add_argument("--http-port", type=int, default=8080)
    parser.add_argument("--log-level", default="INFO")
    return parser.parse_args()


async def async_main(args: argparse.Namespace) -> None:
    link = RoverLink(args.port, args.baudrate)
    core = RoverCore(link)  # must be built inside the running loop, see core.py
    link.on_frame = core.on_frame
    link.start()
    logger.info("connected to ESP32 on %s @ %d baud", args.port, args.baudrate)

    app = create_app(core)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, args.http_host, args.http_port)
    await site.start()
    logger.info("control server listening on http://%s:%d", args.http_host, args.http_port)

    try:
        await asyncio.Event().wait()  # run until cancelled (Ctrl+C -> KeyboardInterrupt)
    finally:
        await runner.cleanup()
        link.stop()


def main() -> None:
    args = parse_args()
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    try:
        asyncio.run(async_main(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
