"""Tests for rover_mqtt.publisher -- both "disabled" paths (no host
configured; paho-mqtt not installed) must leave the rest of rover_core
working, same principle as rover_control.camera's hardware-optional
handling."""
from __future__ import annotations

from rover_mqtt.publisher import MqttPublisher


def test_unavailable_when_no_host_configured():
    publisher = MqttPublisher(host=None, port=1883, topic_prefix="rover")
    assert publisher.available is False


def test_publish_state_is_a_no_op_when_unavailable():
    publisher = MqttPublisher(host=None, port=1883, topic_prefix="rover")
    # Must not raise even though nothing is connected.
    publisher.publish_state({"behavior_state": "IDLE"})


def test_close_is_a_no_op_when_never_connected():
    publisher = MqttPublisher(host=None, port=1883, topic_prefix="rover")
    publisher.close()  # must not raise


def test_unavailable_when_paho_mqtt_not_installed(monkeypatch):
    import builtins

    real_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name == "paho.mqtt.client" or name.startswith("paho"):
            raise ImportError("simulated: paho-mqtt not installed")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)
    publisher = MqttPublisher(host="localhost", port=1883, topic_prefix="rover")
    assert publisher.available is False


def test_unavailable_when_broker_unreachable():
    # Port 1 is a real, always-refused port on any machine (privileged,
    # nothing listens there) -- a real connection attempt that reliably
    # fails fast, without needing an actual broker or a mock.
    publisher = MqttPublisher(host="127.0.0.1", port=1, topic_prefix="rover")
    assert publisher.available is False
