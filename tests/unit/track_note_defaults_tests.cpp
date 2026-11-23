#include "CppUTest/TestHarness.h"
#include "model/instrument/track_note_defaults.h"

using namespace deluge::track_defaults;

TEST_GROUP(TrackNoteDefaultsTests){};

TEST(TrackNoteDefaultsTests, defaults) {
	NoteDefaults d;
	CHECK_EQUAL(4, d.lenQuarters); // 1 step
	CHECK_EQUAL(60, d.note);       // C3
	CHECK_EQUAL(100, d.chance);
}

TEST(TrackNoteDefaultsTests, lenIndexWalksQuartersThenWholeSteps) {
	CHECK_EQUAL(1, lenQuartersFromIndex(0));   // 0.25
	CHECK_EQUAL(16, lenQuartersFromIndex(15)); // 4
	CHECK_EQUAL(20, lenQuartersFromIndex(16)); // 5
	CHECK_EQUAL(256, lenQuartersFromIndex(kLenIndexMax));
	CHECK_EQUAL(256, lenQuartersFromIndex(999));
	for (int32_t i = 0; i <= kLenIndexMax; i++) {
		CHECK_EQUAL(i, lenIndexFromQuarters(lenQuartersFromIndex(i)));
	}
	CHECK_EQUAL(16, lenIndexFromQuarters(17)); // 4.25 rounds up to 5
}

TEST(TrackNoteDefaultsTests, formatsLen) {
	char b[8];
	formatLen(1, b, sizeof(b));
	STRCMP_EQUAL("0.25", b);
	formatLen(2, b, sizeof(b));
	STRCMP_EQUAL("0.5", b);
	formatLen(7, b, sizeof(b));
	STRCMP_EQUAL("1.75", b);
	formatLen(4, b, sizeof(b));
	STRCMP_EQUAL("1", b);
	formatLen(256, b, sizeof(b));
	STRCMP_EQUAL("64", b);
}

TEST(TrackNoteDefaultsTests, lenTicksScalesTheSquare) {
	CHECK_EQUAL(48u, lenTicks(4, 48));
	CHECK_EQUAL(12u, lenTicks(1, 48));
	CHECK_EQUAL(3072u, lenTicks(256, 48));
	CHECK_EQUAL(1u, lenTicks(1, 2)); // never 0
}

TEST(TrackNoteDefaultsTests, chance) {
	for (int32_t r = 0; r < 256; r++) {
		CHECK(chancePasses(100, r));
		CHECK_FALSE(chancePasses(0, r));
	}
	int32_t passed = 0;
	for (int32_t r = 0; r < 256; r++) {
		passed += chancePasses(50, r) ? 1 : 0;
	}
	CHECK_EQUAL(128, passed);
}
