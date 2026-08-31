#pragma once

// Matches ARCHITECTURE_AND_ROADMAP.md section 15's emotion list
// exactly -- this is the vocabulary the Pi is allowed to speak
// (FACE emotion=...); how each one actually looks is entirely
// DisplayEngine's decision (section 15/16: "émotion logique" vs
// "rendu physique de l'émotion").
enum class Emotion {
    IDLE,
    HAPPY,
    CURIOUS,
    SLEEPY,
    CONFUSED,
    ALERT,
    SAD,
    EXCITED
};

// Parses a FACE command's `emotion=` field value (e.g. "happy",
// case-sensitive, lowercase like the protocol examples) into an
// Emotion. Returns false for an unrecognized name, leaving `out`
// untouched -- mirrors how an unrecognized SYSTEM action is silently
// ignored rather than erroring (main.cpp).
bool parseEmotion(const char* name, Emotion& out);
