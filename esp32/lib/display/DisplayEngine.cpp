#include "DisplayEngine.h"
#include <SPI.h>
#include <string.h>
#include "EyeRenderer.h"

namespace {

struct EmotionProfile {
    float baseOpenness;
    bool blinkEnabled;
    unsigned long blinkMinMs;
    unsigned long blinkMaxMs;
    float lookBiasX;
    float lookBiasY;
    bool wander;      // idle random look-around added on top of the bias
    bool jitter;      // small per-frame noise added to look position (EXCITED)
    bool asymmetric;  // eyes look in independent directions (CONFUSED)
    float glitchIntensity;
};

// One profile per Emotion (ARCHITECTURE_AND_ROADMAP.md section 15).
// This is a first, parametric pass reusing a single base eye shape --
// no per-emotion artwork exists yet -- easy to retune once there's real
// design input, without touching FACE dispatch or the protocol.
EmotionProfile profileFor(Emotion emotion) {
    switch (emotion) {
        case Emotion::HAPPY:
            return {0.55f, true, 1200, 2500, 0.0f, 0.0f, true, false, false, 0.0f};
        case Emotion::CURIOUS:
            return {1.15f, true, 3000, 6000, 0.3f, -0.3f, false, false, false, 0.0f};
        case Emotion::SLEEPY:
            return {0.30f, true, 2500, 5000, 0.0f, 0.2f, false, false, false, 0.0f};
        case Emotion::CONFUSED:
            return {0.9f, true, 2000, 4000, 0.0f, 0.0f, true, false, true, 0.0f};
        case Emotion::ALERT:
            return {1.15f, false, 0, 0, 0.0f, 0.0f, false, false, false, 0.6f};
        case Emotion::SAD:
            return {0.5f, true, 2500, 5000, 0.0f, 0.4f, false, false, false, 0.0f};
        case Emotion::EXCITED:
            return {1.0f, true, 900, 1800, 0.0f, 0.0f, false, true, false, 0.0f};
        case Emotion::IDLE:
        default:
            return {1.0f, true, 2000, 5000, 0.0f, 0.0f, true, false, false, 0.0f};
    }
}

constexpr unsigned long BLINK_DURATION_MS = 120;
constexpr unsigned long GLITCH_ANIMATION_DURATION_MS = 900;

// Roughly uniform in [-1, 1].
float randomUnit() {
    return random(-1000, 1001) / 1000.0f;
}

}  // namespace

void DisplayEngine::begin() {
    // Explicit pins, MISO=-1 (unused -- a write-only display never
    // needs it): without this, SPI.begin() with no args would claim the
    // default VSPI MISO pin (GPIO19), which is wired to the yaw servo
    // instead (head_config.h). Matches the approach already proven on
    // the sibling Lumi project.
    SPI.begin(ROVER_PIN_DISPLAY_SCLK, -1, ROVER_PIN_DISPLAY_MOSI, ROVER_PIN_DISPLAY_CS);
    _tft.init(ROVER_DISPLAY_WIDTH, ROVER_DISPLAY_HEIGHT);
    _tft.setSPISpeed(ROVER_DISPLAY_SPI_HZ);

    _lastUpdateMs = millis();
    _nextBlinkMs = millis() + random(profileFor(_emotion).blinkMinMs, profileFor(_emotion).blinkMaxMs + 1);
    render();
}

void DisplayEngine::setEmotion(Emotion emotion) {
    _emotion = emotion;
    // Re-roll blink timing immediately so a new emotion's cadence takes
    // effect right away instead of waiting out the previous one's.
    EmotionProfile profile = profileFor(_emotion);
    _blinking = false;
    if (profile.blinkEnabled) {
        _nextBlinkMs = millis() + random(profile.blinkMinMs, profile.blinkMaxMs + 1);
    }
}

void DisplayEngine::playAnimation(const char* name) {
    if (strcmp(name, "GLITCH") == 0) {
        _animation = PlayingAnimation::GLITCH;
        _animationStartMs = millis();
    }
    // Unrecognized name: silently ignored (see header comment).
}

void DisplayEngine::updateIdleMotion(unsigned long nowMs, float dtSeconds) {
    EmotionProfile profile = profileFor(_emotion);

    if (profile.blinkEnabled) {
        if (_blinking) {
            if (nowMs >= _blinkEndMs) {
                _blinking = false;
                _nextBlinkMs = nowMs + random(profile.blinkMinMs, profile.blinkMaxMs + 1);
            }
        } else if (nowMs >= _nextBlinkMs) {
            _blinking = true;
            _blinkEndMs = nowMs + BLINK_DURATION_MS;
        }
    } else {
        _blinking = false;  // e.g. ALERT: wide-eyed, never blinks
    }
    _openness = _blinking ? 0.05f : profile.baseOpenness;

    // Idle wander: ease toward a random target within the eye, then
    // pick a new one once its time is up.
    if (profile.wander) {
        if (nowMs >= _nextWanderMs) {
            _wanderTargetX = randomUnit() * 0.6f;
            _wanderTargetY = randomUnit() * 0.6f;
            _nextWanderMs = nowMs + random(1500, 3501);
        }
    } else {
        _wanderTargetX = 0.0f;
        _wanderTargetY = 0.0f;
    }

    float easeRate = constrain(3.0f * dtSeconds, 0.0f, 1.0f);  // ~95% there in ~1s
    _lookX += (profile.lookBiasX + _wanderTargetX - _lookX) * easeRate;
    _lookY += (profile.lookBiasY + _wanderTargetY - _lookY) * easeRate;

    if (profile.jitter) {
        _lookX = constrain(_lookX + randomUnit() * 0.15f, -1.0f, 1.0f);
        _lookY = constrain(_lookY + randomUnit() * 0.15f, -1.0f, 1.0f);
    }
}

void DisplayEngine::render() {
    EmotionProfile profile = profileFor(_emotion);
    EyeState state;
    state.openness = _openness;
    state.glitchIntensity =
        (_animation == PlayingAnimation::GLITCH) ? 1.0f : profile.glitchIntensity;

    if (profile.asymmetric) {
        // CONFUSED: eyes drift apart instead of looking the same way.
        state.leftLookX = constrain(_lookX - 0.4f, -1.0f, 1.0f);
        state.leftLookY = _lookY;
        state.rightLookX = constrain(_lookX + 0.4f, -1.0f, 1.0f);
        state.rightLookY = _lookY;
    } else {
        state.leftLookX = state.rightLookX = _lookX;
        state.leftLookY = state.rightLookY = _lookY;
    }

    EyeRenderer::draw(_tft, state);
}

void DisplayEngine::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_DISPLAY_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    if (_animation == PlayingAnimation::GLITCH &&
        now - _animationStartMs >= GLITCH_ANIMATION_DURATION_MS) {
        _animation = PlayingAnimation::NONE;
    }

    updateIdleMotion(now, dtSeconds);
    render();
}
