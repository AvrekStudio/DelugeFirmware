/*
 * Copyright (c) 2026 Synthstrom Audible Limited
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

#include <cstdint>
#include <cstdio>

/// A track's default note (MASTER menu, second row): LEN, NOTE and SCL. Pure logic, unit tested.
namespace deluge::track_defaults {

/// LEN is stored in eighths of a grid square: 1 = 0.125x, 8 = 1x (a square, the Deluge's usual length), 1024 = 128x.
/// 0 = INF: the note lasts up to the next note in its row.
constexpr int32_t kLenInfinite = 0;
constexpr int32_t kLenNormal = 8;
constexpr int32_t kLenMax = 1024;

struct NoteDefaults {
	int16_t lenEighths = kLenNormal;
	uint8_t note = 60;     ///< NOTE: the note the track's note generators play (C3)
	bool scaleLock = true; ///< SCL: NOTE only steps through the song's scale
};

/// "0.125", "0.5", "1", "1.25", "128", "INF". buf must hold 8 chars.
inline void formatLen(int32_t eighths, char* buf, size_t size) {
	if (eighths == kLenInfinite) {
		snprintf(buf, size, "INF");
		return;
	}
	static constexpr const char* kFraction[8] = {"", ".125", ".25", ".375", ".5", ".625", ".75", ".875"};
	snprintf(buf, size, "%d%s", static_cast<int>(eighths / 8), kFraction[eighths % 8]);
}

/// Length in ticks of a note placed on a square `squareWidth` ticks wide (finite LEN only, never 0).
inline uint32_t lenTicks(int32_t eighths, uint32_t squareWidth) {
	uint64_t ticks = static_cast<uint64_t>(squareWidth) * static_cast<uint64_t>(eighths) / 8;
	return ticks < 1 ? 1 : static_cast<uint32_t>(ticks);
}

/// Is `note` in the scale given as `modeBits` (bit n = n semitones above the root) on `root`?
inline bool inScale(int32_t note, uint16_t modeBits, int32_t root) {
	int32_t interval = ((note - root) % 12 + 12) % 12;
	return (modeBits >> interval) & 1;
}

/// The closest note of the scale (the lower one on a tie).
inline int32_t snapToScale(int32_t note, uint16_t modeBits, int32_t root) {
	if ((modeBits & 0xFFF) == 0) {
		return note;
	}
	for (int32_t d = 0; d < 12; d++) {
		if (note - d >= 0 && inScale(note - d, modeBits, root)) {
			return note - d;
		}
		if (note + d <= 127 && inScale(note + d, modeBits, root)) {
			return note + d;
		}
	}
	return note;
}

/// NOTE turned by `offset` steps: semitones, or scale notes when `scaleLock`. Stays within 0..127.
inline int32_t stepNote(int32_t note, int32_t offset, bool scaleLock, uint16_t modeBits, int32_t root) {
	if (!scaleLock || (modeBits & 0xFFF) == 0) {
		int32_t n = note + offset;
		return n < 0 ? 0 : (n > 127 ? 127 : n);
	}
	int32_t dir = offset > 0 ? 1 : -1;
	int32_t n = note;
	for (int32_t steps = offset > 0 ? offset : -offset; steps > 0; steps--) {
		int32_t next = n + dir;
		while (next >= 0 && next <= 127 && !inScale(next, modeBits, root)) {
			next += dir;
		}
		if (next < 0 || next > 127) {
			break;
		}
		n = next;
	}
	return n;
}

} // namespace deluge::track_defaults
