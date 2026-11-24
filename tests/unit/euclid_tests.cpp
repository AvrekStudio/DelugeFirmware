#include "CppUTest/TestHarness.h"
#include "model/note/euclid.h"
#include <string>

using namespace deluge::euclid;

static std::string pattern(const Settings& s, int32_t steps) {
	std::string p;
	for (int32_t i = 0; i < steps; i++) {
		p += fires(s, i, steps) ? 'x' : '.';
	}
	return p;
}

TEST_GROUP(Euclid){};

TEST(Euclid, classicPatterns) {
	Settings s;
	s.pulses1 = 3;
	STRCMP_EQUAL("x..x..x.", pattern(s, 8).c_str());
	s.pulses1 = 4;
	STRCMP_EQUAL("x...x...x...x...", pattern(s, 16).c_str());
	s.pulses1 = 5;
	STRCMP_EQUAL("x...x..x..x..x..", pattern(s, 16).c_str());
	int32_t count = 0;
	for (char c : pattern(s, 16)) {
		count += c == 'x';
	}
	CHECK_EQUAL(5, count);
	s.pulses1 = 0;
	STRCMP_EQUAL("........", pattern(s, 8).c_str());
	s.pulses1 = 20; // more pulses than steps: every step
	STRCMP_EQUAL("xxxxxxxx", pattern(s, 8).c_str());
}

TEST(Euclid, rotations) {
	Settings s;
	s.pulses1 = 3;
	s.rotate1 = 1;
	STRCMP_EQUAL(".x..x..x", pattern(s, 8).c_str());
	s.rotate1 = -1;
	STRCMP_EQUAL("..x..x.x", pattern(s, 8).c_str());
	s.rotate1 = 0;
	s.trackRotate = 2;
	STRCMP_EQUAL("x.x..x..", pattern(s, 8).c_str());
}

TEST(Euclid, operators) {
	Settings s;
	s.pulses1 = 2; // x...x...
	s.pulses2 = 4; // x.x.x.x.
	s.op = Op::OR;
	STRCMP_EQUAL("x.x.x.x.", pattern(s, 8).c_str());
	s.op = Op::AND;
	STRCMP_EQUAL("x...x...", pattern(s, 8).c_str());
	s.op = Op::XOR;
	STRCMP_EQUAL("..x...x.", pattern(s, 8).c_str());
	s.pulses2 = 0;
	s.op = Op::OR;
	STRCMP_EQUAL("x...x...", pattern(s, 8).c_str());
}

TEST(Euclid, rotationRangeWraps) {
	CHECK_EQUAL(8, maxRotation(16));
	CHECK_EQUAL(-7, minRotation(16));
	CHECK_EQUAL(-7, wrapRotation(9, 16));
	CHECK_EQUAL(8, wrapRotation(-8, 16));
	CHECK_EQUAL(3, wrapRotation(3, 16));
	CHECK_EQUAL(0, wrapRotation(0, 1));
}

TEST(Euclid, fitAndSteps) {
	Settings s;
	s.pulses1 = 12;
	s.rotate1 = 7;
	s.trackRotate = -7;
	fitTo(s, 8);
	CHECK_EQUAL(8, s.pulses1);
	CHECK_EQUAL(4, s.rotate1);
	CHECK_EQUAL(-3, s.trackRotate);
	CHECK_EQUAL(16, stepsForLength(384, 24));
	CHECK_EQUAL(1, stepsForLength(5, 24));
	CHECK_EQUAL(kMaxSteps, stepsForLength(100000, 24));
	CHECK_EQUAL(3, stepsForLength(70, 24));
}
