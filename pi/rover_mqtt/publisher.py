"""Minimal MQTT publisher -- a first, small step towards Phase 10
(ARCHITECTURE_AND_ROADMAP.md, Home Assistant integration), not that
whole phase: outbound state only, no MQTT discovery, no inbound
commands. Intended for the "surveillance de la maison" use case already
mentioned in the architecture doc (§6, Phase 6).

Optional in two independent ways, same "hardware/feature optional"
principle as rover_control.camera/RoverOTA: no broker configured ->
never even tries to import paho-mqtt; paho-mqtt not installed (it's a
separate extra, see pi/requirements-mqtt.txt, not in the base
requirements.txt) -> logs once and stays inert. Either way,
`available` stays False and publish_state() becomes a no-op -- nothing
about the rest of rover_core depends on this working.
"""
from __future__ import annotations

import json
import logging

logger = logging.getLogger(__name__)


class MqttPublisher:
    def __init__(self, host: str | None, port: int, topic_prefix: str) -> None:
        self.available = False
        self._client = None
        self._topic_prefix = topic_prefix

        if not host:
            return

        try:
            import paho.mqtt.client as mqtt
        except ImportError as exc:
            logger.info(
                "MQTT publishing disabled: paho-mqtt not installed (%s) -- "
                "pip install -r requirements-mqtt.txt to enable it", exc,
            )
            return

        # Explicit callback API version -- paho-mqtt 2.x deprecates the
        # implicit v1 default (still works, just warns on every run).
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        try:
            client.connect(host, port, keepalive=30)
        except OSError as exc:
            logger.warning(
                "MQTT broker unreachable at %s:%d (%s) -- state won't be published", host, port, exc
            )
            return

        client.loop_start()
        self._client = client
        self.available = True
        logger.info("publishing state to MQTT %s:%d under topic prefix %r", host, port, topic_prefix)

    def publish_state(self, payload: dict) -> None:
        if not self.available:
            return
        # retain=True: a Home Assistant sensor (or anything else
        # subscribing) that starts up after Rover sees the last known
        # state immediately, instead of waiting for the next publish.
        self._client.publish(f"{self._topic_prefix}/state", json.dumps(payload), retain=True)

    def close(self) -> None:
        if self._client is not None:
            self._client.loop_stop()
            self._client.disconnect()
