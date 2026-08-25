#include "Emotion.h"
#include <string.h>

bool parseEmotion(const char* name, Emotion& out) {
    if (strcmp(name, "idle") == 0) { out = Emotion::IDLE; return true; }
    if (strcmp(name, "happy") == 0) { out = Emotion::HAPPY; return true; }
    if (strcmp(name, "curious") == 0) { out = Emotion::CURIOUS; return true; }
    if (strcmp(name, "sleepy") == 0) { out = Emotion::SLEEPY; return true; }
    if (strcmp(name, "confused") == 0) { out = Emotion::CONFUSED; return true; }
    if (strcmp(name, "alert") == 0) { out = Emotion::ALERT; return true; }
    if (strcmp(name, "sad") == 0) { out = Emotion::SAD; return true; }
    if (strcmp(name, "excited") == 0) { out = Emotion::EXCITED; return true; }
    return false;
}
