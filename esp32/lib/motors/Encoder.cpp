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
    int8_t delta = (a == b) ? 1 : -1;
    self->_ticks += delta;
    self->_totalTicks += delta;
    self->_totalEdges++;
    portEXIT_CRITICAL_ISR(&self->_mux);
}

long Encoder::readAndResetTicks() {
    portENTER_CRITICAL(&_mux);
    long ticks = _ticks;
    _ticks = 0;
    portEXIT_CRITICAL(&_mux);
    return ticks;
}

long Encoder::totalTicks() {
    portENTER_CRITICAL(&_mux);
    long total = _totalTicks;
    portEXIT_CRITICAL(&_mux);
    return total;
}

unsigned long Encoder::totalEdges() {
    portENTER_CRITICAL(&_mux);
    unsigned long total = _totalEdges;
    portEXIT_CRITICAL(&_mux);
    return total;
}

bool Encoder::pollStorm(unsigned long nowMs) {
    if (_stormed) return false;
    if (_stormWindowStartMs == 0) {
        _stormWindowStartMs = nowMs;
        _stormWindowEdges = totalEdges();
        return false;
    }

    // Measured against the REAL elapsed time, not the nominal window:
    // loop() jitters (a full display redraw alone is ~27ms), and judging
    // a rate against a window that did not actually elapse would detach
    // a perfectly good encoder.
    unsigned long elapsedMs = nowMs - _stormWindowStartMs;
    if (elapsedMs < STORM_WINDOW_MS) return false;

    unsigned long edges = totalEdges();
    unsigned long inWindow = edges - _stormWindowEdges;
    _stormWindowStartMs = nowMs;
    _stormWindowEdges = edges;

    // Compared as a budget of edges rather than by computing a rate, so
    // nothing has to be multiplied by 1000 -- a genuine storm can push
    // the count high enough for that to overflow.
    unsigned long allowed = (MAX_PLAUSIBLE_EDGES_PER_S / 1000UL) * elapsedMs;
    if (inWindow <= allowed) return false;

    detachInterrupt(digitalPinToInterrupt(_pinA));
    _stormed = true;
    return true;
}

void Encoder::resetTotal() {
    portENTER_CRITICAL(&_mux);
    _totalTicks = 0;
    portEXIT_CRITICAL(&_mux);
    // Doubles as the "I have fixed the wiring, try again" command: a
    // storm-detached interrupt is re-armed here and nowhere else, so
    // recovery is always a deliberate act (SYSTEM action=reset_ticks).
    if (_stormed) {
        _stormed = false;
        _stormWindowStartMs = 0;
        attachInterruptArg(digitalPinToInterrupt(_pinA), onPinAChange, this, CHANGE);
    }
}
