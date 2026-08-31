#include "EnvironmentSensor.h"
#include "sensors_config.h"
#include "I2CProbe.h"

void EnvironmentSensor::begin() {
    // Ping before the library's own begin() -- see I2CProbe.h.
    if (i2cDevicePresent(ROVER_BME688_ADDRESS)) {
        _ok = _bme.begin(ROVER_BME688_ADDRESS);
        if (_ok) {
            // Standard oversampling/filter settings from Adafruit's own
            // BME680 example -- not tuned for Rover specifically, good
            // enough to confirm the sensor reads sane values at this stage.
            _bme.setTemperatureOversampling(BME680_OS_8X);
            _bme.setHumidityOversampling(BME680_OS_2X);
            _bme.setPressureOversampling(BME680_OS_4X);
            _bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
            _bme.setGasHeater(320, 150);  // 320C for 150ms, per the sensor's datasheet example
        }
    } else {
        _ok = false;
    }
    _lastRetryMs = millis();
    _readingInProgress = false;
}

void EnvironmentSensor::update() {
    unsigned long now = millis();

    if (!_ok) {
        if (now - _lastRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) begin();
        return;
    }

    if (_readingInProgress) {
        if ((long)(now - _readingReadyMs) < 0) return;  // conversion still running, don't block
        if (!_bme.endReading()) {
            _ok = false;
            _lastRetryMs = now;
            _readingInProgress = false;
            return;
        }
        _temperatureC = _bme.temperature;
        _humidityPct = _bme.humidity;
        _pressureHpa = _bme.pressure / 100.0f;
        _gasKohm = _bme.gas_resistance / 1000.0f;
        _readingInProgress = false;
        return;
    }

    if (now - _lastUpdateMs < ROVER_SENSOR_UPDATE_PERIOD_MS) return;
    _lastUpdateMs = now;

    // Kicks off the sensor's own conversion (heater + ADC) and returns
    // immediately with the time it'll be ready; endReading() above picks
    // up the result on a later update() call once that time has passed.
    unsigned long readyAt = _bme.beginReading();
    if (readyAt == 0) {
        _ok = false;
        _lastRetryMs = now;
        return;
    }
    _readingReadyMs = readyAt;
    _readingInProgress = true;
}

void EnvironmentSensor::buildTelemetryFields(char* out, size_t outLen) const {
    snprintf(out, outLen, "temperature=%.1f humidity=%.1f pressure=%.1f gas_kohm=%.1f",
             _temperatureC, _humidityPct, _pressureHpa, _gasKohm);
}
