#include "CppUTest/TestHarness.h"
#include "modulation/arp_steps.h"
#include <array>

using deluge::modulation::ArpSteps;
using deluge::modulation::OctaveSpan;

TEST_GROUP(ArpStepsTests){};

TEST(ArpStepsTests, startsWithSixteenStepsOnAndNoOffset) {
	ArpSteps steps;
	CHECK_EQUAL(16, steps.len);
	for (int32_t s = 0; s < ArpSteps::kMaxSteps; s++) {
		CHECK(steps.isOn(s));
		CHECK_EQUAL(0, steps.offset[s]);
	}
	CHECK_FALSE(steps.isOn(-1));
	CHECK_FALSE(steps.isOn(16));
}

TEST(ArpStepsTests, stepsLoopOverLen) {
	ArpSteps steps;
	steps.setLen(4);
	CHECK_EQUAL(0, steps.stepAt(0));
	CHECK_EQUAL(3, steps.stepAt(3));
	CHECK_EQUAL(0, steps.stepAt(4));
	CHECK_EQUAL(1, steps.stepAt(9));
	steps.setLen(0);
	CHECK_EQUAL(1, steps.len);
	steps.setLen(40);
	CHECK_EQUAL(16, steps.len);
}

TEST(ArpStepsTests, toggleAndOffsetsAreClamped) {
	ArpSteps steps;
	steps.toggle(2);
	CHECK_FALSE(steps.isOn(2));
	steps.toggle(2);
	CHECK(steps.isOn(2));
	steps.toggle(99); // ignored
	steps.setOffset(5, 30);
	CHECK_EQUAL(24, steps.offset[5]);
	steps.setOffset(5, -30);
	CHECK_EQUAL(-24, steps.offset[5]);
	steps.setOffset(16, 3); // ignored
}

TEST(ArpStepsTests, bytesRoundTrip) {
	ArpSteps steps;
	steps.toggle(0);
	steps.toggle(15);
	steps.setOffset(3, -7);
	steps.setOffset(15, 12);
	std::array<uint8_t, ArpSteps::kNumBytes> bytes{};
	steps.toBytes(bytes.data());
	ArpSteps back;
	back.fromBytes(bytes.data());
	CHECK_EQUAL(steps.on, back.on);
	for (int32_t s = 0; s < ArpSteps::kMaxSteps; s++) {
		CHECK_EQUAL(steps.offset[s], back.offset[s]);
	}
}

TEST(ArpStepsTests, octaveSpanMapsToNumOctavesAndDirection) {
	uint8_t numOctaves = 0;
	bool below = true;
	OctaveSpan::toSettings(0, numOctaves, below);
	CHECK_EQUAL(1, numOctaves);
	CHECK_FALSE(below);
	OctaveSpan::toSettings(3, numOctaves, below);
	CHECK_EQUAL(4, numOctaves);
	CHECK_FALSE(below);
	OctaveSpan::toSettings(-2, numOctaves, below);
	CHECK_EQUAL(3, numOctaves);
	CHECK(below);
	CHECK_EQUAL(-2, OctaveSpan::fromSettings(numOctaves, below));
	OctaveSpan::toSettings(-9, numOctaves, below);
	CHECK_EQUAL(-5, OctaveSpan::fromSettings(numOctaves, below));
	CHECK_EQUAL(5, OctaveSpan::fromSettings(8, false)); // an old 8-octave setting shows as +5
}

TEST(ArpStepsTests, octavesGoBelowTheNoteWhenNegative) {
	// +2: octaves 0, 1, 2 above the note
	CHECK_EQUAL(0, OctaveSpan::octaveOf(0, 3, false));
	CHECK_EQUAL(2, OctaveSpan::octaveOf(2, 3, false));
	// -2: octaves -2, -1, 0 (the note held is the top one)
	CHECK_EQUAL(-2, OctaveSpan::octaveOf(0, 3, true));
	CHECK_EQUAL(0, OctaveSpan::octaveOf(2, 3, true));
}

TEST(ArpStepsTests, rhythmOffPlaysEveryNoteUnmoved) {
	ArpSteps steps;
	steps.toggle(1);
	steps.setOffset(1, 5);
	steps.setOffset(2, -3);
	CHECK_FALSE(steps.plays(1));
	CHECK_EQUAL(-3, steps.shift(2));
	steps.enabled = false;
	CHECK(steps.plays(1));
	CHECK_EQUAL(0, steps.shift(1));
	CHECK_EQUAL(0, steps.shift(2));
	CHECK_FALSE(steps.isOn(1)); // kept
	CHECK_EQUAL(5, steps.offset[1]);
}
