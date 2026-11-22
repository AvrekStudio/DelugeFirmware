#include "CppUTest/TestHarness.h"
#include "model/instrument/track_note_defaults.h"
#include <cstring>

using namespace deluge::track_defaults;

namespace {
constexpr uint16_t kMajor = 0b101010110101; // C D E F G A B on the root
}

TEST_GROUP(TrackNoteDefaultsTests){};

TEST(TrackNoteDefaultsTests, formatsLen) {
	char b[8];
	formatLen(0, b, sizeof(b));
	STRCMP_EQUAL("INF", b);
	formatLen(1, b, sizeof(b));
	STRCMP_EQUAL("0.125", b);
	formatLen(4, b, sizeof(b));
	STRCMP_EQUAL("0.5", b);
	formatLen(8, b, sizeof(b));
	STRCMP_EQUAL("1", b);
	formatLen(10, b, sizeof(b));
	STRCMP_EQUAL("1.25", b);
	formatLen(1024, b, sizeof(b));
	STRCMP_EQUAL("128", b);
}

TEST(TrackNoteDefaultsTests, lenTicksScalesTheSquareAndNeverReachesZero) {
	CHECK_EQUAL(48u, lenTicks(8, 48));
	CHECK_EQUAL(6u, lenTicks(1, 48));
	CHECK_EQUAL(96u, lenTicks(16, 48));
	CHECK_EQUAL(1u, lenTicks(1, 3));
}

TEST(TrackNoteDefaultsTests, snapsToTheClosestScaleNoteLowerOnTie) {
	CHECK_EQUAL(60, snapToScale(61, kMajor, 0)); // C# -> C (tie C/D: lower)
	CHECK_EQUAL(64, snapToScale(64, kMajor, 0));
	CHECK_EQUAL(65, snapToScale(66, kMajor, 0)); // F# -> F (tie F/G: lower)
}

TEST(TrackNoteDefaultsTests, stepsThroughTheScaleOrSemitones) {
	CHECK_EQUAL(62, stepNote(60, 1, true, kMajor, 0));  // C -> D
	CHECK_EQUAL(65, stepNote(64, 1, true, kMajor, 0));  // E -> F
	CHECK_EQUAL(59, stepNote(60, -1, true, kMajor, 0)); // C -> B
	CHECK_EQUAL(61, stepNote(60, 1, false, kMajor, 0)); // SCL off: semitone
	CHECK_EQUAL(127, stepNote(126, 5, false, kMajor, 0));
	CHECK_EQUAL(0, stepNote(1, -5, false, kMajor, 0));
	// D major (root 2): C# is in it
	CHECK_EQUAL(61, stepNote(59, 1, true, kMajor, 2));
}
