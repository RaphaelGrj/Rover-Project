#include <unity.h>
#include "Emotion.h"

void setUp(void) {}
void tearDown(void) {}

// Must match ARCHITECTURE_AND_ROADMAP.md section 15's 8 emotions exactly
// -- this is the vocabulary the Pi is allowed to speak over FACE.
void test_parses_all_known_emotions(void) {
    struct { const char* name; Emotion expected; } cases[] = {
        {"idle", Emotion::IDLE},         {"happy", Emotion::HAPPY},
        {"curious", Emotion::CURIOUS},   {"sleepy", Emotion::SLEEPY},
        {"confused", Emotion::CONFUSED}, {"alert", Emotion::ALERT},
        {"sad", Emotion::SAD},           {"excited", Emotion::EXCITED},
    };
    for (auto& c : cases) {
        Emotion out;
        TEST_ASSERT_TRUE_MESSAGE(parseEmotion(c.name, out), c.name);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(c.expected), static_cast<int>(out));
    }
}

// main.cpp relies on this leaving `out` untouched and just returning
// false, the same tolerance as an unrecognized SYSTEM action.
void test_rejects_unknown_emotion_without_touching_output(void) {
    Emotion out = Emotion::HAPPY;
    TEST_ASSERT_FALSE(parseEmotion("bogus", out));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Emotion::HAPPY), static_cast<int>(out));
}

void test_is_case_sensitive(void) {
    Emotion out;
    TEST_ASSERT_FALSE(parseEmotion("HAPPY", out));
    TEST_ASSERT_FALSE(parseEmotion("Happy", out));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_all_known_emotions);
    RUN_TEST(test_rejects_unknown_emotion_without_touching_output);
    RUN_TEST(test_is_case_sensitive);
    return UNITY_END();
}
