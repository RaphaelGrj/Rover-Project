#include <unity.h>
#include <string>
#include "LinkAuth.h"
#include "RoverProtocol.h"
#include "FakeStream.h"

void setUp(void) {}
void tearDown(void) {}

// Builds a RoverFrame the way the firmware really gets one: by encoding
// a frame through RoverProtocol::send (checksum included) and parsing
// it back. Hand-filling a RoverFrame would skip the parser, and the
// parser is exactly what bounds a field to MAX_VALUE_LEN -- the bound
// LinkAuth's fixed-length comparison relies on.
static RoverFrame parse(const char* type, const char* fields) {
    FakeStream sender;
    RoverProtocol outgoing(sender);
    outgoing.send(type, fields);

    FakeStream receiver;
    receiver.feed(sender.sent[0].c_str());
    RoverProtocol incoming(receiver);

    RoverFrame captured;
    incoming.onFrame([&captured](const RoverFrame& f) { captured = f; });
    incoming.poll();
    return captured;
}

// The USB cable case, which is still how the robot runs today: nothing
// is required, nothing is rejected, the firmware behaves exactly as it
// did before this class existed.
void test_disabled_auth_lets_every_frame_through(void) {
    LinkAuth auth;  // requireAuth defaults to false

    TEST_ASSERT_TRUE(auth.isAuthenticated());
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Allow,
                      auth.evaluate(parse("MOVE", "velocity=0.25 rotation=0.00")));
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Allow, auth.evaluate(parse("HEARTBEAT", "")));
}

// The central requirement of ARCHITECTURE_AND_ROADMAP.md §6.2 "5 bis":
// on a network transport, NOTHING moves the robot before the peer has
// proven itself -- not even a heartbeat, which is what would otherwise
// keep an unauthenticated connection alive indefinitely.
void test_network_link_rejects_commands_before_auth(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);

    TEST_ASSERT_FALSE(auth.isAuthenticated());
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected,
                      auth.evaluate(parse("MOVE", "velocity=1.00 rotation=0.00")));
    TEST_ASSERT_EQUAL_STRING("unauthenticated", auth.reason());
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected, auth.evaluate(parse("HEARTBEAT", "")));
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected,
                      auth.evaluate(parse("SYSTEM", "action=resume")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());
}

void test_correct_secret_authenticates_and_is_consumed(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);

    // Accepted, not Allow: AUTH is consumed here and must never reach
    // the frame handler as if it were a command.
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Accepted,
                      auth.evaluate(parse("AUTH", "secret=correct-horse")));
    TEST_ASSERT_TRUE(auth.isAuthenticated());

    // And now ordinary traffic flows.
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Allow,
                      auth.evaluate(parse("MOVE", "velocity=0.25 rotation=0.00")));
}

void test_wrong_secret_is_rejected(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);

    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected,
                      auth.evaluate(parse("AUTH", "secret=correct-horsf")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());
    TEST_ASSERT_EQUAL_STRING("unauthenticated", auth.reason());
}

// A prefix of the real secret must not pass. Worth its own test
// because a length-first comparison and a byte-wise one fail this
// differently, and the fixed-length walk in constantTimeEquals relies
// on both buffers being zero-padded for it to hold.
void test_secret_prefix_is_rejected(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);

    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected, auth.evaluate(parse("AUTH", "secret=correct")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());

    // ...and so must a superstring of it.
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected,
                      auth.evaluate(parse("AUTH", "secret=correct-horse-battery")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());
}

void test_auth_frame_without_a_secret_field_is_rejected(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);

    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected, auth.evaluate(parse("AUTH", "")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());
}

// Fail closed: no stored secret on a network link means locked, never
// open. Same stance as StandaloneControl (refuses to raise an AP
// without a password) and RoverOTA (refuses to flash without one).
// The distinct reason code matters: "wrong secret" is fixed at the Pi,
// "no secret stored" is fixed at the robot, and from the outside they
// are otherwise indistinguishable.
void test_no_stored_secret_refuses_everything(void) {
    LinkAuth auth;
    auth.requireAuth(true);

    TEST_ASSERT_FALSE(auth.hasSecret());
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected, auth.evaluate(parse("AUTH", "secret=anything")));
    TEST_ASSERT_EQUAL_STRING("link_secret_not_set", auth.reason());
    TEST_ASSERT_FALSE(auth.isAuthenticated());

    // An empty secret must not be a skeleton key either.
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected, auth.evaluate(parse("MOVE", "velocity=1.00")));
    TEST_ASSERT_FALSE(auth.isAuthenticated());
}

// Authentication is per-connection and never remembered: a link that
// drops and comes back is a new peer, which is the whole point over a
// WiFi socket where drops are routine (pi/rover_esp32/link.py
// reconnects on its own).
void test_reset_revokes_authentication(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Accepted,
                      auth.evaluate(parse("AUTH", "secret=correct-horse")));

    auth.reset();

    TEST_ASSERT_FALSE(auth.isAuthenticated());
    TEST_ASSERT_EQUAL(LinkAuth::Decision::Rejected,
                      auth.evaluate(parse("MOVE", "velocity=0.25 rotation=0.00")));
}

// A Pi that reconnected without the ESP32 noticing the drop re-sends
// AUTH on a link this side still considers authenticated. Consumed,
// not treated as an unknown command.
void test_repeat_auth_on_an_authenticated_link_is_consumed(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);
    auth.evaluate(parse("AUTH", "secret=correct-horse"));

    TEST_ASSERT_EQUAL(LinkAuth::Decision::Accepted,
                      auth.evaluate(parse("AUTH", "secret=correct-horse")));
    TEST_ASSERT_TRUE(auth.isAuthenticated());
}

// Going back to the cable must not leave a stale "already
// authenticated" flag behind -- and must not lock the cable out either.
void test_requiring_auth_again_revokes_a_previous_session(void) {
    LinkAuth auth;
    auth.setSecret("correct-horse");
    auth.requireAuth(true);
    auth.evaluate(parse("AUTH", "secret=correct-horse"));
    TEST_ASSERT_TRUE(auth.isAuthenticated());

    auth.requireAuth(false);
    TEST_ASSERT_TRUE(auth.isAuthenticated());  // cable: nothing required

    auth.requireAuth(true);
    TEST_ASSERT_FALSE(auth.isAuthenticated());  // network again: prove it
}

// A secret longer than a frame field would be truncated by the parser
// and could then never match, which is a miserable thing to debug. The
// constant here is what the provisioning portal enforces up front.
void test_max_secret_length_matches_the_frame_field_bound(void) {
    TEST_ASSERT_EQUAL_size_t(RoverFrame::MAX_VALUE_LEN - 1, LinkAuth::MAX_SECRET_LEN);

    LinkAuth auth;
    std::string longest(LinkAuth::MAX_SECRET_LEN, 'k');
    auth.setSecret(longest.c_str());
    auth.requireAuth(true);

    TEST_ASSERT_EQUAL(LinkAuth::Decision::Accepted,
                      auth.evaluate(parse("AUTH", ("secret=" + longest).c_str())));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_auth_lets_every_frame_through);
    RUN_TEST(test_network_link_rejects_commands_before_auth);
    RUN_TEST(test_correct_secret_authenticates_and_is_consumed);
    RUN_TEST(test_wrong_secret_is_rejected);
    RUN_TEST(test_secret_prefix_is_rejected);
    RUN_TEST(test_auth_frame_without_a_secret_field_is_rejected);
    RUN_TEST(test_no_stored_secret_refuses_everything);
    RUN_TEST(test_reset_revokes_authentication);
    RUN_TEST(test_repeat_auth_on_an_authenticated_link_is_consumed);
    RUN_TEST(test_requiring_auth_again_revokes_a_previous_session);
    RUN_TEST(test_max_secret_length_matches_the_frame_field_bound);
    return UNITY_END();
}
