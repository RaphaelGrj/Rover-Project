#pragma once

// What to actually draw this frame -- computed by DisplayEngine from
// the current Emotion plus live blink/wander/animation timers, and
// consumed by EyeRenderer. Kept as a plain struct so rendering stays a
// pure function of state: no hidden timers inside the renderer itself.
struct EyeState {
    float leftLookX = 0.0f;   // -1..1, iris offset within the eye
    float leftLookY = 0.0f;
    float rightLookX = 0.0f;
    float rightLookY = 0.0f;
    float openness = 1.0f;         // 0 = fully closed (blink), 1 = normal, >1 = wide open
    float glitchIntensity = 0.0f;  // 0 = clean, 1 = max RGB-split offset
};
