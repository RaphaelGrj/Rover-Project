#pragma once

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME680.h>

// Wraps whichever BME280/BME680/688 is actually plugged in, auto-
// detected by chip-id register (0xD0) at begin() -- see PROGRESS.md
// 2026-09-02: the board on hand turned out to be a BME280 (chip-id
// 0x60, no gas sensor), not the BME688 (0x61) originally planned in
// ARCHITECTURE_AND_ROADMAP.md Phase 4. Auto-detection means swapping in
// a real BME688 later is plug-and-play -- no code change, just power
// down, swap the board, power back up (or wait for the next
// ROVER_SENSOR_RETRY_PERIOD_MS retry). gas_kohm stays 0 on a BME280;
// only a detected BME680/688 fills it in.
class EnvironmentSensor {
public:
    void begin();

    // Non-blocking, call every loop() iteration. The BME280 path reads
    // synchronously (fast, no heater); the BME680/688 path uses
    // beginReading()/endReading() instead of blocking performReading()
    // since its gas-heater conversion takes a couple hundred ms, which
    // would otherwise stall the shared loop() that also drives
    // PID/protocol/display.
    void update();

    bool ok() const { return _ok; }

    // Fills "temperature=... humidity=... pressure=... gas_kohm=..."
    // (Celsius, %RH, hPa, kOhm) for a STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const;

private:
    enum class Variant { NONE, BME280, BME680 };

    Adafruit_BME280 _bme280;
    Adafruit_BME680 _bme680;
    Variant _variant = Variant::NONE;
    bool _ok = false;
    bool _readingInProgress = false;  // BME680 path only
    unsigned long _readingReadyMs = 0;
    unsigned long _lastUpdateMs = 0;
    unsigned long _lastRetryMs = 0;
    float _temperatureC = 0.0f, _humidityPct = 0.0f, _pressureHpa = 0.0f, _gasKohm = 0.0f;
};
