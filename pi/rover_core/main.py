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
import ssl
from logging.handlers import RotatingFileHandler
from pathlib import Path

import asyncio

from aiohttp import web

from rover_esp32.link import RoverLink
from rover_control.auth import resolve_token
from rover_control.camera import CameraStream
from rover_control.server import create_app
from rover_mqtt.publisher import MqttPublisher

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
    parser.add_argument(
        "--tls-cert",
        default=None,
        help="Path to a TLS certificate (PEM). Set alongside --tls-key to serve HTTPS/WSS "
        "instead of plain HTTP/WS -- see pi/README.md for how to generate a self-signed one. "
        "Never commit a real cert/key to the repo.",
    )
    parser.add_argument("--tls-key", default=None, help="Path to the TLS private key (PEM) matching --tls-cert.")
    parser.add_argument(
        "--mqtt-host",
        default=None,
        help="MQTT broker host. Publishes Rover's state periodically if set -- "
        "see pi/README.md 'MQTT / Home Assistant'. Unset by default (no publishing).",
    )
    parser.add_argument("--mqtt-port", type=int, default=None)
    parser.add_argument("--mqtt-topic-prefix", default=None)
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
        "tls_cert": args.tls_cert or config["tls_cert"],
        "tls_key": args.tls_key or config["tls_key"],
        "mqtt_host": args.mqtt_host or config["mqtt_host"],
        "mqtt_port": args.mqtt_port or config["mqtt_port"],
        "mqtt_topic_prefix": args.mqtt_topic_prefix or config["mqtt_topic_prefix"],
        "mqtt_publish_period_s": config["mqtt_publish_period_s"],
    }


def build_ssl_context(tls_cert: str | None, tls_key: str | None) -> ssl.SSLContext | None:
    """Returns None (plain HTTP/WS) unless both a cert and key were
    given and actually exist -- TLS is opt-in, and a half-configured
    pair (typo'd path, etc.) must fail loudly rather than silently
    falling back to unencrypted, which could go unnoticed on a shared
    network."""
    if not tls_cert and not tls_key:
        return None
    if not tls_cert or not tls_key:
        raise SystemExit("--tls-cert and --tls-key must both be set to enable HTTPS/WSS (or neither).")
    if not Path(tls_cert).is_file() or not Path(tls_key).is_file():
        raise SystemExit(f"TLS cert/key not found: {tls_cert} / {tls_key}")

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=tls_cert, keyfile=tls_key)
    return context


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


async def _mqtt_publish_loop(core: RoverCore, mqtt: MqttPublisher, period_s: float) -> None:
    """Periodic, decoupled from the raw per-field STATE cadence (the
    ESP32 sends distance/IMU/environment/wheel-speed as separate lines
    every 200-500ms, ROVER_PROTOCOL.md §7.1) -- publishing a full
    snapshot that often would be excessive MQTT traffic for what is
    meant to be a coarse "what's Rover up to" signal for Home
    Assistant, not a high-rate telemetry feed."""
    try:
        while True:
            mqtt.publish_state({"behavior_state": core.state.name, **core.last_state})
            await asyncio.sleep(period_s)
    except asyncio.CancelledError:
        pass


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

    ssl_context = build_ssl_context(settings["tls_cert"], settings["tls_key"])
    site = web.TCPSite(runner, settings["http_host"], settings["http_port"], ssl_context=ssl_context)
    await site.start()
    scheme = "https" if ssl_context else "http"
    logger.info(
        "control server listening on %s://%s:%d (append ?token=... from the log line above)%s",
        scheme, settings["http_host"], settings["http_port"],
        "" if ssl_context else " -- PLAIN HTTP: the token above travels unencrypted on this network",
    )

    mqtt = MqttPublisher(settings["mqtt_host"], settings["mqtt_port"], settings["mqtt_topic_prefix"])
    mqtt_task = None
    if mqtt.available:
        mqtt_task = asyncio.create_task(
            _mqtt_publish_loop(core, mqtt, settings["mqtt_publish_period_s"])
        )

    try:
        await asyncio.Event().wait()  # run until cancelled (Ctrl+C -> KeyboardInterrupt)
    finally:
        if mqtt_task is not None:
            mqtt_task.cancel()
        mqtt.close()
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
