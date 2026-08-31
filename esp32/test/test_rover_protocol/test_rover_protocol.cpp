#include <unity.h>
#include <string>
#include "RoverProtocol.h"
#include "FakeStream.h"

void setUp(void) {}
void tearDown(void) {}

// Cross-checked against ROVER_PROTOCOL.md section 3.1 and
// esp32/tools/rover_frame.py -- this exact example was independently
// verified during the Wokwi Phase 1 session (PROGRESS.md 2026-08-25)
// after the *first* version of this doc example turned out to be wrong.
void test_send_appends_the_documented_checksum(void) {
    FakeStream stream;
    RoverProtocol protocol(stream);

    protocol.send("MOVE", "velocity=0.25 rotation=-0.10");

    TEST_ASSERT_EQUAL_size_t(1, stream.sent.size());
    TEST_ASSERT_EQUAL_STRING("MOVE velocity=0.25 rotation=-0.10 *39\n", stream.sent[0].c_str());
}

void test_send_with_no_fields_omits_trailing_space(void) {
    FakeStream stream;
    RoverProtocol protocol(stream);

    protocol.send("HEARTBEAT");

    // Content is just "HEARTBEAT" -- checksum is the XOR of those 9
    // bytes, independently computed here rather than copy-pasted, so
    // this test can't share a copy-paste mistake with the code under test.
    uint8_t expected = 0;
    for (char c : std::string("HEARTBEAT")) expected ^= (uint8_t)c;
    char want[32];
    snprintf(want, sizeof(want), "HEARTBEAT *%02X\n", expected);
    TEST_ASSERT_EQUAL_STRING(want, stream.sent[0].c_str());
}

void test_round_trip_send_then_parse(void) {
    FakeStream sender;
    RoverProtocol outgoing(sender);
    outgoing.send("MOVE", "velocity=0.25 rotation=-0.10");

    FakeStream receiver;
    receiver.feed(sender.sent[0].c_str());
    RoverProtocol incoming(receiver);

    bool called = false;
    incoming.onFrame([&](const RoverFrame& frame) {
        called = true;
        TEST_ASSERT_EQUAL_STRING("MOVE", frame.type);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, frame.getFloat("velocity"));
        TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.10f, frame.getFloat("rotation"));
    });
    incoming.poll();

    TEST_ASSERT_TRUE(called);
    TEST_ASSERT_TRUE(receiver.sent.empty());  // a valid frame never gets an ERROR reply
}

void test_invalid_checksum_is_rejected_and_reported(void) {
    FakeStream stream;
    RoverProtocol protocol(stream);
    stream.feed("MOVE velocity=0.25 rotation=-0.10 *00\n");  // *39 is the real checksum

    bool called = false;
    protocol.onFrame([&](const RoverFrame&) { called = true; });
    protocol.poll();

    TEST_ASSERT_FALSE(called);
    TEST_ASSERT_EQUAL_size_t(1, stream.sent.size());
    TEST_ASSERT_EQUAL_STRING("ERROR code=checksum_invalid *6B\n", stream.sent[0].c_str());
}

void test_oversized_line_is_rejected_as_frame_too_long(void) {
    FakeStream stream;
    RoverProtocol protocol(stream);
    // ROVER_MAX_FRAME_LEN is 128 (board_config.h); well past that.
    std::string huge(200, 'A');
    stream.feed((huge + "\n").c_str());

    bool called = false;
    protocol.onFrame([&](const RoverFrame&) { called = true; });
    protocol.poll();

    TEST_ASSERT_FALSE(called);
    TEST_ASSERT_EQUAL_size_t(1, stream.sent.size());
    TEST_ASSERT_EQUAL_STRING("ERROR code=frame_too_long *4B\n", stream.sent[0].c_str());
}

void test_carriage_return_is_tolerated(void) {
    uint8_t expected = 0;
    for (char c : std::string("HEARTBEAT")) expected ^= (uint8_t)c;
    char line[32];
    snprintf(line, sizeof(line), "HEARTBEAT *%02X\r\n", expected);

    FakeStream stream;
    RoverProtocol protocol(stream);
    stream.feed(line);

    bool called = false;
    protocol.onFrame([&](const RoverFrame& f) {
        called = true;
        TEST_ASSERT_EQUAL_STRING("HEARTBEAT", f.type);
    });
    protocol.poll();

    TEST_ASSERT_TRUE(called);
    TEST_ASSERT_TRUE(stream.sent.empty());
}

void test_frame_get_field_helpers(void) {
    RoverFrame frame;
    strcpy(frame.type, "MOVE");
    strcpy(frame.fields[0].key, "velocity");
    strcpy(frame.fields[0].value, "0.25");
    strcpy(frame.fields[1].key, "rotation");
    strcpy(frame.fields[1].value, "-10");
    frame.fieldCount = 2;

    TEST_ASSERT_TRUE(frame.hasField("velocity"));
    TEST_ASSERT_FALSE(frame.hasField("pitch"));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, frame.getFloat("velocity"));
    TEST_ASSERT_EQUAL_INT(-10, frame.getInt("rotation"));
    // Missing field falls back to the caller-supplied default, not 0.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, frame.getFloat("missing", 42.0f));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_send_appends_the_documented_checksum);
    RUN_TEST(test_send_with_no_fields_omits_trailing_space);
    RUN_TEST(test_round_trip_send_then_parse);
    RUN_TEST(test_invalid_checksum_is_rejected_and_reported);
    RUN_TEST(test_oversized_line_is_rejected_as_frame_too_long);
    RUN_TEST(test_carriage_return_is_tolerated);
    RUN_TEST(test_frame_get_field_helpers);
    return UNITY_END();
}
