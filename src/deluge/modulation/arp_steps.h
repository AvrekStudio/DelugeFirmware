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

#include <algorithm>
#include <array>
#include <cstdint>

namespace deluge::modulation {

/// The arpeggiator's step sequence (Digitone style): LEN steps played in a loop, one per arp note. An OFF step is a
/// silence, but the arp still moves on to its next note. Each step moves its note by a number of semitones.
struct ArpSteps {
	static constexpr int32_t kMaxSteps = 16;
	static constexpr int32_t kMaxOffset = 24;
	static constexpr int32_t kNumBytes = 2 + kMaxSteps; // on bits + offsets

	uint8_t len = kMaxSteps;
	uint16_t on = 0xFFFF;
	std::array<int8_t, kMaxSteps> offset{};

	[[nodiscard]] bool isOn(int32_t step) const { return step >= 0 && step < kMaxSteps && ((on >> step) & 1); }
	void toggle(int32_t step) {
		if (step >= 0 && step < kMaxSteps) {
			on ^= static_cast<uint16_t>(1u << step);
		}
	}
	void setOffset(int32_t step, int32_t semitones) {
		if (step >= 0 && step < kMaxSteps) {
			offset[step] = static_cast<int8_t>(std::clamp<int32_t>(semitones, -kMaxOffset, kMaxOffset));
		}
	}
	void setLen(int32_t newLen) { len = static_cast<uint8_t>(std::clamp<int32_t>(newLen, 1, kMaxSteps)); }
	/// The step played for the arp's n-th note (counted from the arp's start)
	[[nodiscard]] int32_t stepAt(uint32_t n) const { return static_cast<int32_t>(n % (len ? len : 1)); }

	void toBytes(uint8_t* out) const {
		out[0] = on & 0xFF;
		out[1] = on >> 8;
		for (int32_t s = 0; s < kMaxSteps; s++) {
			out[2 + s] = static_cast<uint8_t>(offset[s]);
		}
	}
	void fromBytes(const uint8_t* in) {
		on = static_cast<uint16_t>(in[0] | (in[1] << 8));
		for (int32_t s = 0; s < kMaxSteps; s++) {
			setOffset(s, static_cast<int8_t>(in[2 + s]));
		}
	}
};

/// OCTA, -5 to +5: 0 = the notes held only, +N = N more octaves above them, -N = N more octaves below them.
/// Stored as the arp's number of octaves (|OCTA| + 1) and whether they go down.
struct OctaveSpan {
	static constexpr int32_t kMin = -5;
	static constexpr int32_t kMax = 5;

	static int32_t fromSettings(uint8_t numOctaves, bool below) {
		const int32_t n = std::clamp<int32_t>(numOctaves, 1, kMax + 1) - 1;
		return below ? -n : n;
	}
	static void toSettings(int32_t span, uint8_t& numOctaves, bool& below) {
		span = std::clamp<int32_t>(span, kMin, kMax);
		numOctaves = static_cast<uint8_t>((span < 0 ? -span : span) + 1);
		below = span < 0;
	}
	/// Octave (relative to the note held) of the arp's octave index 0 .. numOctaves - 1
	static int32_t octaveOf(int32_t index, uint8_t numOctaves, bool below) {
		return below ? index - (numOctaves - 1) : index;
	}
};

} // namespace deluge::modulation
