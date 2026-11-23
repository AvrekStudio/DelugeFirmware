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
#include <cstdint>
#include <cstdio>

/// MASTER menu, second row: the track's LEN / NOTE / RAND
namespace deluge::track_defaults {

/// LEN, in quarters of a step (a grid square): 0.25 to 64 steps
constexpr int32_t kLenQuartersMin = 1;
constexpr int32_t kLenQuartersMax = 256;
constexpr int32_t kLenQuartersDefault = 4; // 1 step
/// The menu walks LEN by quarters up to 4 steps, then by whole steps up to 64
constexpr int32_t kLenFineSteps = 16; // 0.25 .. 4
constexpr int32_t kLenIndexMax = kLenFineSteps - 1 + (64 - 4);

constexpr int32_t lenQuartersFromIndex(int32_t index) {
	index = std::clamp<int32_t>(index, 0, kLenIndexMax);
	return index < kLenFineSteps ? index + 1 : (index - kLenFineSteps + 5) * 4;
}

constexpr int32_t lenIndexFromQuarters(int32_t quarters) {
	quarters = std::clamp<int32_t>(quarters, kLenQuartersMin, kLenQuartersMax);
	if (quarters <= kLenFineSteps) {
		return quarters - 1;
	}
	return kLenFineSteps + (quarters + 3) / 4 - 5; // rounded up to a whole step
}

/// "0.25", "1.5", "64"
inline void formatLen(int32_t quarters, char* out, size_t size) {
	const int32_t whole = quarters / 4;
	switch (quarters % 4) {
	case 0:
		snprintf(out, size, "%d", static_cast<int>(whole));
		break;
	case 1:
		snprintf(out, size, "%d.25", static_cast<int>(whole));
		break;
	case 2:
		snprintf(out, size, "%d.5", static_cast<int>(whole));
		break;
	default:
		snprintf(out, size, "%d.75", static_cast<int>(whole));
		break;
	}
}

/// Ticks of a note LEN quarters of a square long (never 0)
constexpr uint32_t lenTicks(int32_t quarters, uint32_t squareTicks) {
	const uint64_t ticks = static_cast<uint64_t>(squareTicks) * static_cast<uint32_t>(quarters) / 4;
	return ticks ? static_cast<uint32_t>(ticks) : 1;
}

/// RAND: the chance, in %, that each note of the track plays
constexpr int32_t kChanceMax = 100;

/// Whether a note plays, for a random byte 0..255
constexpr bool chancePasses(int32_t chance, uint8_t random255) {
	return chance >= kChanceMax || (static_cast<int32_t>(random255) * kChanceMax >> 8) < chance;
}

struct NoteDefaults {
	int16_t lenQuarters = kLenQuartersDefault;
	uint8_t note = 60; // C3: the note the track's note generators play
	uint8_t chance = kChanceMax;
};

} // namespace deluge::track_defaults
