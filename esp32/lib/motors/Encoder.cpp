#include "Encoder.h"

void Encoder::begin(uint8_t pinA, uint8_t pinB) {
    _pinA = pinA;
    _pinB = pinB;
    // GPIO34-39 (used here, see motion_config.h) are input-only on the
    // ESP32 and have no internal pull-up/down; the encoder module is
    // expected to drive both signals itself (open-drain + external pull
    // or push-pull, depending on the exact N20 encoder board).
    pinMode(_pinA, INPUT);
    pinMode(_pinB, INPUT);
    attachInterruptArg(digitalPinToInterrupt(_pinA), onPinAChange, this, CHANGE);
}

void IRAM_ATTR Encoder::onPinAChange(void* arg) {
    Encoder* self = static_cast<Encoder*>(arg);
    bool a = digitalRead(self->_pinA);
    bool b = digitalRead(self->_pinB);
    // A leads B for one direction of rotation, lags for the other.
    portENTER_CRITICAL_ISR(&self->_mux);
    self->_ticks += (a == b) ? 1 : -1;
    portEXIT_CRITICAL_ISR(&self->_mux);
}

long Encoder::readAndResetTicks() {
    portENTER_CRITICAL(&_mux);
    long ticks = _ticks;
    _ticks = 0;
    portEXIT_CRITICAL(&_mux);
    return ticks;
}
