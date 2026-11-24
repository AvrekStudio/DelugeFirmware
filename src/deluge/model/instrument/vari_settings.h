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

#include "util/chord_mode_calc.h"
#include <algorithm>
#include <array>
#include <cstdint>

/// VARI (SOUND menu): live, non-destructive variations of the grid's notes as they play (grid -> VARI -> CHORD ->
/// ARP). Every amount is 0-100 %. CHNC is the track's RAND of the MASTER menu (the same value).
namespace deluge::vari {

enum Param : uint8_t { VELO, NOTE, OCT, GATE, RATC, HUMN, MUT, NUM_PARAMS };

constexpr std::array<const char*, NUM_PARAMS> kLabels = {"VELO", "NOTE", "OCT", "GATE", "RATC", "HUMN", "MUT"};
constexpr std::array<const char*, NUM_PARAMS> kSaveNames = {"variVelo",    "variNote",  "variOct",   "variGate",
                                                            "variRatchet", "variHuman", "variMutate"};
constexpr int32_t kMax = 100;

/// Set while VARI PRINT asks for its undo action (from the sound editor)
inline bool printing = false;

struct Settings {
	std::array<uint8_t, NUM_PARAMS> amount{}; // MUT 0 % = the variation drawn is kept, pass after pass

	[[nodiscard]] bool anyVariation() const {
		for (int32_t p = 0; p < MUT; p++) {
			if (amount[p] != 0) {
				return true;
			}
		}
		return false;
	}
};

/// A note's variation: drawn from its seed, so the same seed always gives the same variation (MUT keeps or redraws
/// the seed). Every draw is made whatever the settings, so turning one setting doesn't change what the others drew.
struct Variation {
	int32_t note = 0;
	int32_t velocity = 0;
	int32_t gatePermille = 1000; // length, in thousandths of the note's
	int32_t ratchets = 1;        // 1 = played once
	int32_t delayPermille = 0;   // HUMN: how late, in thousandths of kMaxHumanDelayMs
};

constexpr int32_t kMaxHumanDelayMs = 30;

/// xorshift32: the seed's sequence of draws
struct Draws {
	uint32_t state;
	explicit Draws(uint32_t seed) : state(seed != 0 ? seed : 0x9E3779B9u) {}
	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	/// 0 .. n - 1
	int32_t below(int32_t n) { return n > 0 ? static_cast<int32_t>(next() % static_cast<uint32_t>(n)) : 0; }
};

/// The scale note `degrees` scale steps away (up if positive) - semitones if the scale has no notes
inline int32_t scaleStep(int32_t note, int32_t degrees, const chordmode::Scale& scale) {
	if (scale.bits == 0) {
		return note + degrees;
	}
	const int32_t dir = degrees > 0 ? 1 : -1;
	int32_t n = note;
	for (int32_t found = 0; found < degrees * dir;) {
		n += dir;
		if (scale.contains(n)) {
			found++;
		}
		if ((n - note) * dir > 48) {
			break;
		}
	}
	return n;
}

inline Variation vary(const Settings& s, uint32_t seed, int32_t note, int32_t velocity, const chordmode::Scale& scale) {
	Draws d(seed);
	Variation v;
	v.note = note;
	v.velocity = velocity;

	// VELO: up to +-64 x VELO %
	const int32_t veloDraw = d.below(2001) - 1000; // -1000 .. 1000
	// NOTE: NOTE % of the notes move 1 to 3 scale degrees (further as NOTE grows)
	const int32_t noteChance = d.below(100);
	const int32_t noteJump = d.below(3);
	const bool noteUp = d.below(2) == 0;
	// OCT: OCT % of the notes jump an octave
	const int32_t octChance = d.below(100);
	const bool octUp = d.below(2) == 0;
	// GATE: from 1/4 to 2x the length, the range opening with GATE %
	const int32_t gateDraw = d.below(1001); // 0 .. 1000
	// RATC: RATC % of the notes play 2 to 4 times
	const int32_t ratchetChance = d.below(100);
	const int32_t ratchetCount = d.below(3);
	// HUMN: up to kMaxHumanDelayMs x HUMN % late
	const int32_t humanDraw = d.below(1001);

	const auto& a = s.amount;
	v.velocity = std::clamp<int32_t>(velocity + veloDraw * 64 * a[VELO] / 100 / 1000, 1, 127);
	if (noteChance < a[NOTE]) {
		const int32_t maxJump = 1 + a[NOTE] * 2 / 100; // 1 .. 3
		const int32_t jump = 1 + noteJump % maxJump;
		v.note = scaleStep(v.note, noteUp ? jump : -jump, scale);
	}
	if (octChance < a[OCT]) {
		v.note += octUp ? 12 : -12;
	}
	v.note = std::clamp<int32_t>(v.note, 0, 127);
	if (a[GATE] > 0) {
		const int32_t lo = 1000 - 750 * a[GATE] / 100;
		const int32_t hi = 1000 + 1000 * a[GATE] / 100;
		v.gatePermille = lo + gateDraw * (hi - lo) / 1000;
	}
	if (ratchetChance < a[RATC]) {
		const int32_t maxExtra = 1 + a[RATC] * 2 / 100; // 1 .. 3
		v.ratchets = 2 + ratchetCount % maxExtra;
	}
	v.delayPermille = humanDraw * a[HUMN] / 100;
	return v;
}

/// MUT: whether a note draws a new variation this time (0 % = never, it keeps its own)
inline bool redraws(const Settings& s, uint32_t random100) {
	return static_cast<int32_t>(random100 % 100) < s.amount[MUT];
}

} // namespace deluge::vari
