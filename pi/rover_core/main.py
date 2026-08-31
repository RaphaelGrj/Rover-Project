"""Entrypoint: wires the ESP32 link, RoverCore and the control web
server together, and runs them until interrupted.

    python -m rover_core.main --port /dev/ttyUSB0
    python -m rover_core.main --port rfc2217://localhost:4000   # Wokwi

See pi/README.md for setup, the Wokwi testing workflow, and how to set
ROVER_CONTROL_TOKEN (rover_control.auth) before exposing this to a
shared network.
"""
from __future__ import annotations

import argparse
import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path

import asyncio

from aiohttp import web

from rover_esp32.link import RoverLink
from rover_control.auth import resolve_token
from rover_control.camera import CameraStream
from rover_control.server import create_app

from .config import load_config
from .core import RoverCore

logger = logging.getLogger("rover_core")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Rover Pi core: ESP32 bridge + control server")
    parser.add_argument(
        "--config",
        type=Path,
        default=None,
        help="Path to a JSON config file (default: pi/config.json if present -- "
        "see pi/config.example.json). Any flag given below overrides the config file.",
    )
    parser.add_argument(
        "--port",
        default=None,
        help='Serial port to the ESP32 (Rover Protocol UART), e.g. "/dev/ttyUSB0" '
        'on the real robot or "rfc2217://localhost:4000" against a Wokwi simulation.',
    )
    parser.add_argument("--baudrate", type=int, default=None)
    parser.add_argument("--http-host", default=None)
    parser.add_argument("--http-port", type=int, default=None)
    parser.add_argument("--log-level", default=None)
    parser.add_argument(
        "--log-dir", default=None, help="Directory for rotating log files (default: console only)"
    )
    return parser.parse_args()


def resolve_settings(args: argparse.Namespace) -> dict:
    config = load_config(args.config)
    # An explicitly-given CLI flag always wins over the config file --
    # `or` works here because none of these settings' real values are
    # falsy in a way that could be confused with "not given" (an empty
    # port string or a 0 baudrate/http_port are not valid values anyway).
    return {
        "port": args.port or config["port"],
        "baudrate": args.baudrate or config["baudrate"],
        "http_host": args.http_host or config["http_host"],
        "http_port": args.http_port or config["http_port"],
        "log_level": args.log_level or config["log_level"],
        "log_dir": args.log_dir or config["log_dir"],
    }


def configure_logging(log_level: str, log_dir: str | None) -> None:
    handlers: list[logging.Handler] = [logging.StreamHandler()]
    if log_dir:
        Path(log_dir).mkdir(parents=True, exist_ok=True)
        # 1MB x 5 backups: bounded disk use, no manual log rotation upkeep.
        handlers.append(
            RotatingFileHandler(
                Path(log_dir) / "rover_core.log", maxBytes=1_000_000, backupCount=5, encoding="utf-8"
            )
        )
    logging.basicConfig(
        level=log_level, format="%(asctime)s %(levelname)s %(name)s: %(message)s", handlers=handlers
    )


async def async_main(settings: dict) -> None:
    link = RoverLink(settings["port"], settings["baudrate"])
    core = RoverCore(link)  # must be built inside the running loop, see core.py
    link.on_frame = core.on_frame
    link.start()
    logger.info("connected to ESP32 on %s @ %d baud", settings["port"], settings["baudrate"])

    token = resolve_token()
    camera = CameraStream()
    app = create_app(core, token, camera)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, settings["http_host"], settings["http_port"])
    await site.start()
    logger.info(
        "control server listening on http://%s:%d (append ?token=... from the log line above)",
        settings["http_host"], settings["http_port"],
    )

    try:
        await asyncio.Event().wait()  # run until cancelled (Ctrl+C -> KeyboardInterrupt)
    finally:
        await runner.cleanup()
        link.stop()


def main() -> None:
    args = parse_args()
    settings = resolve_settings(args)
    configure_logging(settings["log_level"], settings["log_dir"])
    try:
        asyncio.run(async_main(settings))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
