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

#include "model/note_fx/note_fx_params.h"
#include <array>
#include <cstdint>

namespace deluge::note_fx {

/// Elektron-style p-locks: a NOTE FX module parameter given its own value on one note placed on the grid (the note
/// at `pos` on the row playing `note`). While that note plays, the module uses that value.
struct NoteLock {
	int32_t pos = -1; ///< -1 = free entry
	uint8_t note = 0;
	uint8_t type = 0;
	uint8_t param = 0;
	int16_t value = 0;
};

class NoteLocks {
public:
	static constexpr int32_t kCapacity = 64;

	/// The lock on (pos, note) for that parameter, or nullptr.
	[[nodiscard]] const NoteLock* find(int32_t pos, int32_t note, Type type, int32_t param) const {
		for (const NoteLock& l : locks_) {
			if (l.pos == pos && l.note == note && l.type == static_cast<uint8_t>(type) && l.param == param) {
				return &l;
			}
		}
		return nullptr;
	}
	[[nodiscard]] bool any(int32_t pos, int32_t note) const {
		for (const NoteLock& l : locks_) {
			if (l.pos == pos && l.note == note) {
				return true;
			}
		}
		return false;
	}
	/// Returns false if full.
	bool set(int32_t pos, int32_t note, Type type, int32_t param, int32_t value) {
		NoteLock* free = nullptr;
		for (NoteLock& l : locks_) {
			if (l.pos == pos && l.note == note && l.type == static_cast<uint8_t>(type) && l.param == param) {
				l.value = static_cast<int16_t>(value);
				return true;
			}
			if (l.pos < 0 && free == nullptr) {
				free = &l;
			}
		}
		if (free == nullptr) {
			return false;
		}
		*free = {pos, static_cast<uint8_t>(note), static_cast<uint8_t>(type), static_cast<uint8_t>(param),
		         static_cast<int16_t>(value)};
		return true;
	}
	void clear(int32_t pos, int32_t note, Type type, int32_t param) {
		for (NoteLock& l : locks_) {
			if (l.pos == pos && l.note == note && l.type == static_cast<uint8_t>(type) && l.param == param) {
				l = NoteLock{};
			}
		}
	}
	/// Copies the locks of (pos, note) onto `values` (a copy of the track's values). Returns true if there were any.
	bool applyTo(int32_t pos, int32_t note, Values& values) const {
		bool found = false;
		for (const NoteLock& l : locks_) {
			if (l.pos == pos && l.note == note && l.type < static_cast<uint8_t>(Type::NUM_TYPES)) {
				values.set(static_cast<Type>(l.type), l.param, l.value);
				found = true;
			}
		}
		return found;
	}
	[[nodiscard]] int32_t count() const {
		int32_t n = 0;
		for (const NoteLock& l : locks_) {
			n += l.pos >= 0;
		}
		return n;
	}

	// Saving: 9 bytes per lock in use
	static constexpr int32_t kBytesPerLock = 9;
	int32_t toBytes(uint8_t* b) const {
		int32_t i = 0;
		for (const NoteLock& l : locks_) {
			if (l.pos < 0) {
				continue;
			}
			b[i++] = l.pos & 0xFF;
			b[i++] = (l.pos >> 8) & 0xFF;
			b[i++] = (l.pos >> 16) & 0xFF;
			b[i++] = (l.pos >> 24) & 0xFF;
			b[i++] = l.note;
			b[i++] = l.type;
			b[i++] = l.param;
			b[i++] = l.value & 0xFF;
			b[i++] = (l.value >> 8) & 0xFF;
		}
		return i;
	}
	void fromBytes(const uint8_t* b, int32_t len) {
		*this = NoteLocks{};
		for (int32_t i = 0; i + kBytesPerLock <= len; i += kBytesPerLock) {
			int32_t pos = b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24);
			int32_t type = b[i + 5];
			int32_t count;
			paramSpecs(static_cast<Type>(type), &count);
			if (pos < 0 || b[i + 4] > 127 || type >= static_cast<int32_t>(Type::NUM_TYPES) || b[i + 6] >= count) {
				continue;
			}
			set(pos, b[i + 4], static_cast<Type>(type), b[i + 6], static_cast<int16_t>(b[i + 7] | (b[i + 8] << 8)));
		}
	}

private:
	std::array<NoteLock, kCapacity> locks_{};
};

} // namespace deluge::note_fx
