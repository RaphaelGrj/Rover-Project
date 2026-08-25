#pragma once

#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include "Emotion.h"
#include "display_config.h"

// Owns the ST7789 panel and renders Rover's face (eyes only for now --
// mouth/other expressions are a later iteration, see PROGRESS.md).
// Turns a FACE command into a persistent Emotion, and ANIMATION into a
// one-shot effect layered on top.
class DisplayEngine {
public:
    void begin();

    // Non-blocking, call every loop() iteration; internally rate-limited
    // to ROVER_DISPLAY_UPDATE_PERIOD_MS.
    void update();

    void setEmotion(Emotion emotion);

    // Matched against a small built-in table (see DisplayEngine.cpp);
    // an unrecognized name is silently ignored, same leniency as an
    // unrecognized SYSTEM action (main.cpp).
    void playAnimation(const char* name);

private:
    // Hardware SPI, bound to the global SPI object -- DisplayEngine::begin()
    // must call SPI.begin() with explicit pins (MISO unused, see .cpp)
    // before this is used, so GPIO19 stays free for the yaw servo instead
    // of being silently claimed as the default VSPI MISO pin.
    Adafruit_ST7789 _tft{ROVER_PIN_DISPLAY_CS, ROVER_PIN_DISPLAY_DC, ROVER_PIN_DISPLAY_RST};

    Emotion _emotion = Emotion::IDLE;

    // Idle blink/look-around motion, always running underneath whatever
    // emotion is active; each EmotionProfile (DisplayEngine.cpp) tunes
    // how it behaves rather than replacing it.
    float _lookX = 0.0f, _lookY = 0.0f;
    float _wanderTargetX = 0.0f, _wanderTargetY = 0.0f;
    unsigned long _nextWanderMs = 0;
    float _openness = 1.0f;
    bool _blinking = false;
    unsigned long _nextBlinkMs = 0;
    unsigned long _blinkEndMs = 0;

    // One-shot animation on top of the current emotion; NONE = not
    // playing, eyes render normally.
    enum class PlayingAnimation { NONE, GLITCH } _animation = PlayingAnimation::NONE;
    unsigned long _animationStartMs = 0;

    unsigned long _lastUpdateMs = 0;

    void updateIdleMotion(unsigned long nowMs, float dtSeconds);
    void render();
};
