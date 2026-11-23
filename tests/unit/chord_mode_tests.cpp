#include "CppUTest/TestHarness.h"
#include "util/chord_mode_calc.h"
#include <cstring>

using namespace deluge::chordmode;

namespace {
constexpr Scale kCMajor{0b101010110101, 0};
Chord chordOf(Mode mode, int32_t type, int32_t key, int8_t bass = 0) {
	Settings s;
	s.mode = mode;
	s.setType(type);
	s.bass = bass;
	return build(s, key, kCMajor);
}
} // namespace

TEST_GROUP(ChordModeTests){};

TEST(ChordModeTests, offPlaysTheKeyAlone) {
	Chord c = chordOf(Mode::OFF, 0, 60);
	CHECK_EQUAL(1, c.count);
	CHECK_EQUAL(60, c.notes[0]);
}

TEST(ChordModeTests, freeChordsFromTheKey) {
	Chord maj = chordOf(Mode::FREE, 0, 61);
	CHECK_EQUAL(3, maj.count);
	CHECK_EQUAL(61, maj.notes[0]);
	CHECK_EQUAL(65, maj.notes[1]);
	CHECK_EQUAL(68, maj.notes[2]);
	STRCMP_EQUAL("MAJ", qualityName(maj));
	Chord dom7 = chordOf(Mode::FREE, 5, 60);
	CHECK_EQUAL(4, dom7.count);
	CHECK_EQUAL(70, dom7.notes[3]);
	STRCMP_EQUAL("7", qualityName(dom7));
	STRCMP_EQUAL("5", qualityName(chordOf(Mode::FREE, 2, 60)));
}

TEST(ChordModeTests, rootTriadsAreDiatonic) {
	Chord d = chordOf(Mode::ROOT, 1, 62); // D in C major: D F A
	CHECK_EQUAL(62, d.notes[0]);
	CHECK_EQUAL(65, d.notes[1]);
	CHECK_EQUAL(69, d.notes[2]);
	STRCMP_EQUAL("MIN", qualityName(d));
	STRCMP_EQUAL("DIM", qualityName(chordOf(Mode::ROOT, 1, 71))); // B D F
	STRCMP_EQUAL("7", qualityName(chordOf(Mode::ROOT, 4, 67)));   // G B D F
	STRCMP_EQUAL("MAJ7", qualityName(chordOf(Mode::ROOT, 4, 60)));
}

TEST(ChordModeTests, rootMovesAKeyOffTheScale) {
	Chord c = chordOf(Mode::ROOT, 1, 61); // C# -> C (lower on a tie)
	CHECK_EQUAL(60, c.root);
	CHECK_EQUAL(60, c.notes[0]);
	CHECK_EQUAL(65, snapToScale(66, kCMajor)); // F# -> F
}

TEST(ChordModeTests, bassAddsTheRootOctavesAway) {
	Chord c = chordOf(Mode::FREE, 0, 60, -2);
	CHECK_EQUAL(4, c.count);
	CHECK_EQUAL(36, c.notes[3]);
	STRCMP_EQUAL("MAJ", qualityName(c)); // the bass doesn't change the name
	Chord up = chordOf(Mode::FREE, 0, 60, 1);
	CHECK_EQUAL(72, up.notes[3]);
}

TEST(ChordModeTests, typesFollowTheMode) {
	Settings s;
	s.mode = Mode::ROOT;
	CHECK_EQUAL(5, s.typeCount());
	s.setType(4);
	STRCMP_EQUAL("7TH", s.typeName());
	s.mode = Mode::FREE;
	CHECK_EQUAL(8, s.typeCount());
	STRCMP_EQUAL("MAJ", s.typeName());
	s.setType(9); // ignored
	CHECK_EQUAL(0, s.type());
}

TEST(ChordModeTests, notesOutsideMidiAreDropped) {
	Chord c = chordOf(Mode::FREE, 4, 125); // MAJ7 at the top: only the notes that fit
	CHECK_EQUAL(1, c.count);
}
