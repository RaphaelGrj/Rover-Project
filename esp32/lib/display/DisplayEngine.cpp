#include "DisplayEngine.h"
#include <SPI.h>
#include <math.h>
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
    bool jitter;      // extra per-target noise on top of wander (EXCITED)
    bool asymmetric;  // eyes look in independent directions (CONFUSED)
    float glitchIntensity;
};

// One profile per Emotion (ARCHITECTURE_AND_ROADMAP.md section 15).
// This is a first, parametric pass reusing a single base eye shape --
// no per-emotion artwork exists yet -- easy to retune once there's real
// design input, without touching FACE dispatch or the protocol. The
// cyan/magenta glitch is a baseline part of the look for every emotion
// (requested explicitly), not just an ALERT-only effect -- ALERT and
// the GLITCH animation just push it further.
EmotionProfile profileFor(Emotion emotion) {
    switch (emotion) {
        case Emotion::HAPPY:
            return {0.55f, true, 1200, 2500, 0.0f, 0.0f, true, false, false, 0.40f};
        case Emotion::CURIOUS:
            return {1.15f, true, 3000, 6000, 0.3f, -0.3f, false, false, false, 0.40f};
        case Emotion::SLEEPY:
            return {0.30f, true, 2500, 5000, 0.0f, 0.2f, false, false, false, 0.25f};
        case Emotion::CONFUSED:
            return {0.9f, true, 2000, 4000, 0.0f, 0.0f, true, false, true, 0.40f};
        case Emotion::ALERT:
            return {1.15f, false, 0, 0, 0.0f, 0.0f, false, false, false, 0.70f};
        case Emotion::SAD:
            return {0.5f, true, 2500, 5000, 0.0f, 0.4f, false, false, false, 0.30f};
        case Emotion::EXCITED:
            return {1.0f, true, 900, 1800, 0.0f, 0.0f, false, true, false, 0.50f};
        case Emotion::IDLE:
        default:
            return {1.0f, true, 2000, 5000, 0.0f, 0.0f, true, false, false, 0.40f};
    }
}

// A blink is a smooth close-then-open envelope over this duration, not
// an instant on/off toggle -- an abrupt cut reads as a display glitch,
// not a living eye. peaks (fully closed) at the midpoint.
constexpr unsigned long BLINK_DURATION_MS = 180;
constexpr float BLINK_MIN_OPENNESS = 0.05f;

constexpr unsigned long GLITCH_ANIMATION_DURATION_MS = 900;
constexpr unsigned long LOOK_AROUND_DURATION_MS = 1200;
constexpr unsigned long WAKE_UP_DURATION_MS = 600;

// Small continuous sway added on top of the eased look target, so
// nothing is ever perfectly still even when "wander" is off for that
// emotion (CURIOUS/SLEEPY/ALERT/SAD) -- two slightly-off sine periods
// so the motion doesn't look like a mechanical back-and-forth tick.
constexpr float BREATHE_AMOUNT = 0.05f;

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
    // init() takes the panel's native (pre-rotation) size; setRotation()
    // then turns it landscape, which is also what makes tft.width()/
    // height() match ROVER_DISPLAY_WIDTH/HEIGHT afterwards (display_config.h).
    _tft.init(ROVER_DISPLAY_PANEL_WIDTH, ROVER_DISPLAY_PANEL_HEIGHT);
    _tft.setRotation(ROVER_DISPLAY_ROTATION);
    _tft.setSPISpeed(ROVER_DISPLAY_SPI_HZ);
    // One full clear at boot; every frame after this only touches the
    // two eye cells (EyeRenderer::draw), not the whole panel.
    _tft.fillScreen(0x0000);

    unsigned long now = millis();
    _lastUpdateMs = now;
    EmotionProfile profile = profileFor(_emotion);
    _nextBlinkMs = now + random(profile.blinkMinMs, profile.blinkMaxMs + 1);
    render(now);
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
    } else if (strcmp(name, "LOOK_AROUND") == 0) {
        _animation = PlayingAnimation::LOOK_AROUND;
        _animationStartMs = millis();
    } else if (strcmp(name, "WAKE_UP") == 0) {
        _animation = PlayingAnimation::WAKE_UP;
        _animationStartMs = millis();
    }
    // Unrecognized name: silently ignored (see header comment).
}

void DisplayEngine::updateIdleMotion(unsigned long nowMs, float dtSeconds) {
    EmotionProfile profile = profileFor(_emotion);

    // Blink: a smooth envelope (0 -> 1 -> 0 openness reduction) over
    // BLINK_DURATION_MS, not a binary toggle. ALERT disables it
    // entirely (wide-eyed, never blinks).
    if (profile.blinkEnabled) {
        if (_blinking) {
            unsigned long elapsed = nowMs - _blinkStartMs;
            if (elapsed >= BLINK_DURATION_MS) {
                _blinking = false;
                _nextBlinkMs = nowMs + random(profile.blinkMinMs, profile.blinkMaxMs + 1);
                _openness = profile.baseOpenness;
            } else {
                float progress = (float)elapsed / (float)BLINK_DURATION_MS;
                float envelope = sinf(progress * PI);  // 0 at start/end, 1 at midpoint
                _openness = profile.baseOpenness - (profile.baseOpenness - BLINK_MIN_OPENNESS) * envelope;
            }
        } else if (nowMs >= _nextBlinkMs) {
            _blinking = true;
            _blinkStartMs = nowMs;
        } else {
            _openness = profile.baseOpenness;
        }
    } else {
        _blinking = false;
        _openness = profile.baseOpenness;
    }

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

void DisplayEngine::render(unsigned long nowMs) {
    EmotionProfile profile = profileFor(_emotion);

    // Continuous sway, recomputed fresh from the clock every frame (not
    // accumulated into _lookX/_lookY -- see the header comment on why):
    // keeps every emotion visibly alive even when "wander" is off.
    float breatheX = sinf(nowMs * 0.0011f) * BREATHE_AMOUNT;
    float breatheY = sinf(nowMs * 0.0017f + 1.7f) * BREATHE_AMOUNT * 0.7f;
    float renderLookX = constrain(_lookX + breatheX, -1.0f, 1.0f);
    float renderLookY = constrain(_lookY + breatheY, -1.0f, 1.0f);
    float renderOpenness = _openness;

    // LOOK_AROUND/WAKE_UP override the idle-computed look/openness for
    // their duration, same layering principle GLITCH already uses for
    // glitchIntensity below -- the underlying emotion keeps ticking
    // (blink timers etc.) underneath, this only overrides what gets
    // drawn while the animation is active.
    if (_animation == PlayingAnimation::LOOK_AROUND) {
        float progress = (float)(nowMs - _animationStartMs) / (float)LOOK_AROUND_DURATION_MS;
        // One full left-right-center sweep across the animation's
        // duration -- starts and ends at 0 so it blends cleanly back
        // into whatever the idle look direction was.
        renderLookX = sinf(progress * 2.0f * PI) * 0.8f;
    } else if (_animation == PlayingAnimation::WAKE_UP) {
        float progress = constrain(
            (float)(nowMs - _animationStartMs) / (float)WAKE_UP_DURATION_MS, 0.0f, 1.0f);
        // Wide-eyed spike (up to EyeRenderer's own MAX_OPENNESS=1.3) that
        // eases back down to this emotion's normal openness by the end.
        renderOpenness = 1.3f - (1.3f - profile.baseOpenness) * progress;
    }

    EyeState state;
    state.openness = renderOpenness;
    state.glitchIntensity =
        (_animation == PlayingAnimation::GLITCH) ? 1.0f : profile.glitchIntensity;

    if (profile.asymmetric) {
        // CONFUSED: eyes drift apart instead of looking the same way.
        state.leftLookX = constrain(renderLookX - 0.4f, -1.0f, 1.0f);
        state.leftLookY = renderLookY;
        state.rightLookX = constrain(renderLookX + 0.4f, -1.0f, 1.0f);
        state.rightLookY = renderLookY;
    } else {
        state.leftLookX = state.rightLookX = renderLookX;
        state.leftLookY = state.rightLookY = renderLookY;
    }

    EyeRenderer::draw(_tft, state);
}

void DisplayEngine::update() {
    unsigned long now = millis();
    if (now - _lastUpdateMs < ROVER_DISPLAY_UPDATE_PERIOD_MS) return;
    float dtSeconds = (now - _lastUpdateMs) / 1000.0f;
    _lastUpdateMs = now;

    unsigned long animationElapsed = now - _animationStartMs;
    if ((_animation == PlayingAnimation::GLITCH && animationElapsed >= GLITCH_ANIMATION_DURATION_MS) ||
        (_animation == PlayingAnimation::LOOK_AROUND && animationElapsed >= LOOK_AROUND_DURATION_MS) ||
        (_animation == PlayingAnimation::WAKE_UP && animationElapsed >= WAKE_UP_DURATION_MS)) {
        _animation = PlayingAnimation::NONE;
    }

    updateIdleMotion(now, dtSeconds);
    render(now);
}
