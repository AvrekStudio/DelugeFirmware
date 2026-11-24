#include "CppUTest/TestHarness.h"
#include "model/instrument/vari_settings.h"

using namespace deluge::vari;
using deluge::chordmode::Scale;

namespace {
constexpr Scale kCMajor{0b101010110101, 0};
}

TEST_GROUP(VariTests){};

TEST(VariTests, nothingVariesAtZero) {
	Settings s;
	CHECK_FALSE(s.anyVariation());
	for (uint32_t seed = 1; seed < 50; seed++) {
		Variation v = vary(s, seed, 60, 100, kCMajor);
		CHECK_EQUAL(60, v.note);
		CHECK_EQUAL(100, v.velocity);
		CHECK_EQUAL(1000, v.gatePermille);
		CHECK_EQUAL(1, v.ratchets);
		CHECK_EQUAL(0, v.delayPermille);
	}
}

TEST(VariTests, sameSeedSameVariation) {
	Settings s;
	s.amount = {50, 50, 50, 50, 50, 50, 0};
	Variation a = vary(s, 1234, 60, 100, kCMajor);
	Variation b = vary(s, 1234, 60, 100, kCMajor);
	CHECK_EQUAL(a.note, b.note);
	CHECK_EQUAL(a.velocity, b.velocity);
	CHECK_EQUAL(a.gatePermille, b.gatePermille);
	CHECK_EQUAL(a.ratchets, b.ratchets);
	CHECK_EQUAL(a.delayPermille, b.delayPermille);
}

TEST(VariTests, rangesHold) {
	Settings s;
	s.amount = {100, 100, 100, 100, 100, 100, 0};
	bool moved = false;
	for (uint32_t seed = 1; seed < 2000; seed++) {
		Variation v = vary(s, seed, 60, 100, kCMajor);
		CHECK(v.velocity >= 36 && v.velocity <= 127);
		CHECK(v.gatePermille >= 250 && v.gatePermille <= 2000);
		CHECK(v.ratchets >= 1 && v.ratchets <= 4);
		CHECK(v.delayPermille >= 0 && v.delayPermille <= 1000);
		CHECK(v.note >= 60 - 12 - 5 && v.note <= 60 + 12 + 5);
		// NOTE moves on the scale: without OCT the note is a C major note
		CHECK(kCMajor.contains(v.note));
		moved |= v.note != 60;
	}
	CHECK(moved);
}

TEST(VariTests, noteChanceIsAPercentage) {
	Settings s;
	s.amount[NOTE] = 25;
	int32_t moved = 0;
	for (uint32_t seed = 1; seed <= 4000; seed++) {
		moved += vary(s, seed, 64, 100, kCMajor).note != 64 ? 1 : 0;
	}
	CHECK(moved > 800 && moved < 1200); // about 25 %
}

TEST(VariTests, scaleSteps) {
	CHECK_EQUAL(62, scaleStep(60, 1, kCMajor));
	CHECK_EQUAL(65, scaleStep(60, 3, kCMajor));
	CHECK_EQUAL(59, scaleStep(60, -1, kCMajor));
	CHECK_EQUAL(57, scaleStep(60, -2, kCMajor));
	CHECK_EQUAL(63, scaleStep(60, 3, Scale{0, 0}));
}

TEST(VariTests, mutateRedraws) {
	Settings s;
	CHECK_FALSE(redraws(s, 0));
	s.amount[MUT] = 100;
	CHECK(redraws(s, 99));
	s.amount[MUT] = 50;
	CHECK(redraws(s, 49));
	CHECK_FALSE(redraws(s, 50));
}
