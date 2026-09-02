#include "EnvironmentSensor.h"
#include "sensors_config.h"
#include "I2CProbe.h"

namespace {

constexpr uint8_t kChipIdReg = 0xD0;
constexpr uint8_t kChipIdBme280 = 0x60;
constexpr uint8_t kChipIdBme680 = 0x61;

// Raw register read, ahead of either library's own begin() -- this is
// what picks which driver gets handed the sensor. Returns 0xFF (not a
// real Bosch chip-id) on any bus error.
uint8_t readChipId(uint8_t address) {
    Wire.beginTransmission(address);
    Wire.write(kChipIdReg);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    Wire.requestFrom(address, (uint8_t)1);
    if (!Wire.available()) return 0xFF;
    return Wire.read();
}

}  // namespace

void EnvironmentSensor::begin() {
    // Ping before either library's own begin() -- see I2CProbe.h.
    if (!i2cDevicePresent(ROVER_ENV_SENSOR_ADDRESS)) {
        _ok = false;
        _variant = Variant::NONE;
        _lastRetryMs = millis();
        return;
    }

    uint8_t chipId = readChipId(ROVER_ENV_SENSOR_ADDRESS);
    if (chipId == kChipIdBme680) {
        _variant = Variant::BME680;
        _ok = _bme680.begin(ROVER_ENV_SENSOR_ADDRESS);
        if (_ok) {
            // Standard oversampling/filter settings from Adafruit's own
            // BME680 example -- not tuned for Rover specifically, good
            // enough to confirm the sensor reads sane values at this stage.
            _bme680.setTemperatureOversampling(BME680_OS_8X);
            _bme680.setHumidityOversampling(BME680_OS_2X);
            _bme680.setPressureOversampling(BME680_OS_4X);
            _bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
            _bme680.setGasHeater(320, 150);  // 320C for 150ms, per the sensor's datasheet example
        }
    } else {
        // chipId==0x60 (BME280) or anything unrecognized defaults here --
        // if it's genuinely a dead/different part, begin() fails below
        // and the usual sensor_timeout/retry loop takes over regardless.
        _variant = Variant::BME280;
        _ok = _bme280.begin(ROVER_ENV_SENSOR_ADDRESS);
        if (_ok) {
            _bme280.setSampling(Adafruit_BME280::MODE_NORMAL,
                                 Adafruit_BME280::SAMPLING_X2,   // temperature
                                 Adafruit_BME280::SAMPLING_X16,  // pressure
                                 Adafruit_BME280::SAMPLING_X1,   // humidity
                                 Adafruit_BME280::FILTER_X16,
                                 Adafruit_BME280::STANDBY_MS_500);
        }
    }
    _readingInProgress = false;
    _lastRetryMs = millis();
}

void EnvironmentSensor::update() {
    unsigned long now = millis();

    if (!_ok) {
        if (now - _lastRetryMs >= ROVER_SENSOR_RETRY_PERIOD_MS) begin();
        return;
    }

    if (_variant == Variant::BME280) {
        if (now - _lastUpdateMs < ROVER_SENSOR_UPDATE_PERIOD_MS) return;
        _lastUpdateMs = now;
        // MODE_NORMAL keeps it converting continuously in the background
        // (per the standby time set in begin()); these just read back the
        // latest result over I2C, no conversion to wait on.
        _temperatureC = _bme280.readTemperature();
        _humidityPct = _bme280.readHumidity();
        _pressureHpa = _bme280.readPressure() / 100.0f;
        _gasKohm = 0.0f;
        return;
    }

    // BME680/688 path: async two-phase read, its gas heater makes a
    // conversion take a couple hundred ms.
    if (_readingInProgress) {
        if ((long)(now - _readingReadyMs) < 0) return;  // conversion still running, don't block
        if (!_bme680.endReading()) {
            _ok = false;
            _lastRetryMs = now;
            _readingInProgress = false;
            return;
        }
        _temperatureC = _bme680.temperature;
        _humidityPct = _bme680.humidity;
        _pressureHpa = _bme680.pressure / 100.0f;
        _gasKohm = _bme680.gas_resistance / 1000.0f;
        _readingInProgress = false;
        return;
    }

    if (now - _lastUpdateMs < ROVER_SENSOR_UPDATE_PERIOD_MS) return;
    _lastUpdateMs = now;

    unsigned long readyAt = _bme680.beginReading();
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
