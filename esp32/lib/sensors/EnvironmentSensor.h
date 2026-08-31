#pragma once

#include <Arduino.h>
#include <Adafruit_BME680.h>

// Wraps the BME688 via its BME680-compatible core registers
// (temperature/humidity/pressure/gas resistance) -- the extra BME688
// gas-classification features aren't used here. Independent health
// tracking, same reasoning as DistanceSensor/ImuSensor.
class EnvironmentSensor {
public:
    void begin();

    // Non-blocking, call every loop() iteration. Uses the sensor's
    // beginReading()/endReading() pair instead of the blocking
    // performReading() -- a BME680 conversion takes a couple hundred ms
    // (heater + ADC), which would otherwise stall the shared loop() that
    // also drives PID/protocol/display.
    void update();

    bool ok() const { return _ok; }

    // Fills "temperature=... humidity=... pressure=... gas_kohm=..."
    // (Celsius, %RH, hPa, kOhm) for a STATE frame.
    void buildTelemetryFields(char* out, size_t outLen) const;

private:
    Adafruit_BME680 _bme;
    bool _ok = false;
    bool _readingInProgress = false;
    unsigned long _readingReadyMs = 0;
    unsigned long _lastUpdateMs = 0;
    unsigned long _lastRetryMs = 0;
    float _temperatureC = 0.0f, _humidityPct = 0.0f, _pressureHpa = 0.0f, _gasKohm = 0.0f;
};
