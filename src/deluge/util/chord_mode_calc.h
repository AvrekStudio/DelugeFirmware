/*
 * Copyright © 2026 AvrekStudio
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <cstdint>

/// CHORD MODE: one key played = a chord, built before the arpeggiator.
/// ROOT: chords of the song's scale (a key off the scale moves to the closest scale note, the lower one on a tie);
/// FREE: the key played is the chord's root, any quality.
namespace deluge::chordmode {

enum class Mode : uint8_t { OFF, ROOT, FREE, NUM };

constexpr int32_t kNumRootTypes = 5;  // 5TH TRI SUS2 SUS4 7TH
constexpr int32_t kNumFreeTypes = 8;  // MAJ MIN 5TH MIN7 MAJ7 DOM7 SUS4 SUS2
constexpr int32_t kMaxChordNotes = 5; // up to 4 chord notes + the bass
constexpr int32_t kStrumMaxMs = 500;
constexpr int32_t kBassMin = -3;
constexpr int32_t kBassMax = 3;

constexpr std::array<const char*, 3> kModeNames = {"OFF", "ROOT", "FREE"};
constexpr std::array<const char*, kNumRootTypes> kRootTypeNames = {"5TH", "TRI", "SUS2", "SUS4", "7TH"};
constexpr std::array<const char*, kNumFreeTypes> kFreeTypeNames = {"MAJ",  "MIN",  "5TH",  "MIN7",
                                                                   "MAJ7", "DOM7", "SUS4", "SUS2"};

struct Settings {
	Mode mode = Mode::OFF;
	uint8_t rootType = 1; // TRI
	uint8_t freeType = 0; // MAJ
	uint16_t strumMs = 0; // 0 = OFF
	int8_t bass = 0;      // octaves, 0 = OFF

	[[nodiscard]] int32_t typeCount() const { return mode == Mode::ROOT ? kNumRootTypes : kNumFreeTypes; }
	[[nodiscard]] int32_t type() const { return mode == Mode::ROOT ? rootType : freeType; }
	void setType(int32_t t) {
		if (t < 0 || t >= typeCount()) {
			return;
		}
		(mode == Mode::ROOT ? rootType : freeType) = static_cast<uint8_t>(t);
	}
	[[nodiscard]] const char* typeName() const {
		return mode == Mode::ROOT ? kRootTypeNames[rootType % kNumRootTypes] : kFreeTypeNames[freeType % kNumFreeTypes];
	}
};

/// The song's scale: a bit per semitone above its root (bit 0 = the root), and the root's pitch class
struct Scale {
	uint16_t bits = 0xFFF;
	int32_t root = 0;

	[[nodiscard]] bool contains(int32_t note) const {
		const int32_t pc = ((note - root) % 12 + 12) % 12;
		return (bits >> pc) & 1;
	}
	[[nodiscard]] int32_t size() const {
		int32_t n = 0;
		for (int32_t i = 0; i < 12; i++) {
			n += (bits >> i) & 1;
		}
		return n;
	}
};

/// The closest scale note (the lower one on a tie)
inline int32_t snapToScale(int32_t note, const Scale& scale) {
	if (scale.bits == 0 || scale.contains(note)) {
		return note;
	}
	for (int32_t d = 1; d < 12; d++) {
		if (scale.contains(note - d)) {
			return note - d;
		}
		if (scale.contains(note + d)) {
			return note + d;
		}
	}
	return note;
}

/// The scale note `degrees` scale steps above `note` (which is on the scale)
inline int32_t scaleStepUp(int32_t note, int32_t degrees, const Scale& scale) {
	int32_t n = note;
	for (int32_t found = 0; found < degrees;) {
		n++;
		if (scale.contains(n)) {
			found++;
		}
		if (n - note > 48) {
			break; // a scale with no notes
		}
	}
	return n;
}

struct Chord {
	std::array<int16_t, kMaxChordNotes> notes{};
	int32_t count = 0;
	int32_t root = 0; // the chord's root (the key played, or the scale note it moved to)

	void add(int32_t n) {
		if (count < kMaxChordNotes && n >= 0 && n <= 127) {
			notes[count++] = static_cast<int16_t>(n);
		}
	}
};

/// The notes played for a key: the root first, then the rest of the chord, then the bass
inline Chord build(const Settings& s, int32_t key, const Scale& scale) {
	Chord c;
	if (s.mode == Mode::OFF) {
		c.root = key;
		c.add(key);
		return c;
	}
	if (s.mode == Mode::ROOT && scale.size() >= 5) {
		const int32_t r = snapToScale(key, scale);
		c.root = r;
		c.add(r);
		switch (s.rootType) {
		case 0: // 5TH
			c.add(r + 7);
			break;
		case 2: // SUS2
			c.add(r + 2);
			c.add(r + 7);
			break;
		case 3: // SUS4
			c.add(r + 5);
			c.add(r + 7);
			break;
		case 4: // 7TH: diatonic
			c.add(scaleStepUp(r, 2, scale));
			c.add(scaleStepUp(r, 4, scale));
			c.add(scaleStepUp(r, 6, scale));
			break;
		default: // TRI: diatonic
			c.add(scaleStepUp(r, 2, scale));
			c.add(scaleStepUp(r, 4, scale));
			break;
		}
	}
	else {
		// FREE (or ROOT on a scale too small for diatonic chords: major-ish intervals from the key)
		static constexpr std::array<std::array<int8_t, 4>, kNumFreeTypes> kIntervals = {{
		    {0, 4, 7, -1},  // MAJ
		    {0, 3, 7, -1},  // MIN
		    {0, 7, -1, -1}, // 5TH
		    {0, 3, 7, 10},  // MIN7
		    {0, 4, 7, 11},  // MAJ7
		    {0, 4, 7, 10},  // DOM7
		    {0, 5, 7, -1},  // SUS4
		    {0, 2, 7, -1},  // SUS2
		}};
		c.root = key;
		const auto& iv = kIntervals[s.mode == Mode::FREE ? s.freeType % kNumFreeTypes : 0];
		for (int8_t i : iv) {
			if (i >= 0) {
				c.add(key + i);
			}
		}
	}
	if (s.bass != 0) {
		c.add(c.root + 12 * s.bass);
	}
	return c;
}

/// A chord quality from its pitch classes above the root (bit 0 = the root): "MAJ", "MIN", "5", "7", "69"...,
/// nullptr if it isn't one we know
inline const char* qualityFromMask(uint16_t mask) {
	struct Named {
		uint16_t mask;
		const char* name;
	};
	static constexpr std::array<Named, 24> kNames = {{
	    {0b000010010001, "MAJ"},   {0b000010001001, "MIN"},  {0b000001001001, "DIM"},   {0b000100010001, "AUG"},
	    {0b000010000001, "5"},     {0b000010100001, "SUS4"}, {0b000010000101, "SUS2"},  {0b100010010001, "MAJ7"},
	    {0b010010001001, "MIN7"},  {0b010010010001, "7"},    {0b010001001001, "M7B5"},  {0b001001001001, "DIM7"},
	    {0b100010001001, "MMAJ7"}, {0b001010010001, "6"},    {0b001010001001, "M6"},    {0b010010000101, "7SUS2"},
	    {0b010010100001, "7SUS4"}, {0b000010010101, "ADD9"}, {0b000010001101, "MADD9"}, {0b010010010101, "9"},
	    {0b100010010101, "MAJ9"},  {0b010010001101, "MIN9"}, {0b001010010101, "69"},    {0b001010001101, "M69"},
	}};
	for (const Named& n : kNames) {
		if (n.mask == mask) {
			return n.name;
		}
	}
	return nullptr;
}

/// The chord's quality, from its notes above the root: "MAJ", "MIN", "5", "7", "SUS4"...
inline const char* qualityName(const Chord& c) {
	uint16_t mask = 0;
	for (int32_t i = 0; i < c.count; i++) {
		const int32_t iv = c.notes[i] - c.root;
		if (iv >= 0 && iv < 12) {
			mask |= 1 << iv;
		}
	}
	const char* name = qualityFromMask(mask);
	return name != nullptr ? name : "?";
}

/// Names a chord played as notes (keyboard Chord / Chord Library layouts): its root and quality. Inversions are
/// recognised; when several roots fit, the lowest note wins if it makes a known chord.
struct Recognised {
	int32_t root = -1; // a note of the chord (-1: no notes)
	const char* quality = "";
};

inline Recognised recognise(const int16_t* notes, int32_t count) {
	Recognised r;
	if (count <= 0) {
		return r;
	}
	int32_t lowest = notes[0];
	for (int32_t i = 1; i < count; i++) {
		lowest = notes[i] < lowest ? notes[i] : lowest;
	}
	auto maskAbove = [&](int32_t root) {
		uint16_t mask = 0;
		for (int32_t i = 0; i < count; i++) {
			mask |= 1 << (((notes[i] - root) % 12 + 12) % 12);
		}
		return mask;
	};
	// the lowest note first, then the others from the bottom up
	if (const char* q = qualityFromMask(maskAbove(lowest))) {
		return {lowest, q};
	}
	int32_t best = -1;
	const char* bestName = nullptr;
	for (int32_t i = 0; i < count; i++) {
		if (notes[i] == lowest || (best >= 0 && notes[i] >= best)) {
			continue;
		}
		if (const char* q = qualityFromMask(maskAbove(notes[i]))) {
			best = notes[i];
			bestName = q;
		}
	}
	if (best >= 0) {
		return {best, bestName};
	}
	return {lowest, ""};
}

} // namespace deluge::chordmode
