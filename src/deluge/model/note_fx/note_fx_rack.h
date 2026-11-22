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

#include <array>
#include <cstdint>

namespace deluge::note_fx {

/// The modules that can go in a NOTE FX RACK slot. Append new ones at the end (the value is saved).
enum class Type : uint8_t {
	NONE = 0,
	DIR = 1,  ///< Play direction of the clip
	ARP = 2,  ///< Arpeggiator
	EUCL = 3, ///< Euclidean rhythm generator
	MONO = 4, ///< Polyphonic to monophonic
	NUM_TYPES,
};

constexpr int32_t kNumSlots = 8;
constexpr int32_t kSlotsPerRow = 4;

/// Short name drawn in a slot's frame.
constexpr const char* shortName(Type t) {
	switch (t) {
	case Type::DIR:
		return "DIR";
	case Type::ARP:
		return "ARP";
	case Type::EUCL:
		return "EUCL";
	case Type::MONO:
		return "MONO";
	default:
		return "";
	}
}

/// Name shown in the "+" list.
constexpr const char* longName(Type t) {
	switch (t) {
	case Type::DIR:
		return "PLAY DIR";
	case Type::ARP:
		return "ARPEGGIATOR";
	case Type::EUCL:
		return "EUCLID";
	case Type::MONO:
		return "MONO";
	default:
		return "";
	}
}

struct Slot {
	Type type = Type::NONE;
	bool on = true;
	[[nodiscard]] bool empty() const { return type == Type::NONE; }
};

/// A clip's chain of note effects: notes go through the slots in order 0 -> 7 (top row left to right, then bottom
/// row). Pure data + editing logic, no dependency on the rest of the firmware (unit tested).
struct Rack {
	std::array<Slot, kNumSlots> slots{};

	// DIR module's setting: 0 forward, 1 reversed, 2 ping-pong (= SequenceDirection's first three values)
	uint8_t dirMode = 0;

	[[nodiscard]] bool contains(Type t) const {
		for (const Slot& s : slots) {
			if (s.type == t) {
				return true;
			}
		}
		return false;
	}

	/// The slot holding `t`, or -1.
	[[nodiscard]] int32_t find(Type t) const {
		for (int32_t i = 0; i < kNumSlots; i++) {
			if (slots[i].type == t) {
				return i;
			}
		}
		return -1;
	}

	/// True if `t` is in the rack and switched on.
	[[nodiscard]] bool active(Type t) const {
		int32_t i = find(t);
		return i >= 0 && slots[i].on;
	}

	/// Modules not yet in the rack, in list order. Returns how many were written to `out`.
	int32_t available(Type* out, int32_t max) const {
		int32_t n = 0;
		for (uint8_t t = 1; t < static_cast<uint8_t>(Type::NUM_TYPES) && n < max; t++) {
			if (!contains(static_cast<Type>(t))) {
				out[n++] = static_cast<Type>(t);
			}
		}
		return n;
	}

	/// Puts `t` (switched on) in an empty slot. A module can only be in the rack once.
	bool add(int32_t slot, Type t) {
		if (slot < 0 || slot >= kNumSlots || !slots[slot].empty() || t == Type::NONE || t >= Type::NUM_TYPES
		    || contains(t)) {
			return false;
		}
		slots[slot] = Slot{t, true};
		return true;
	}

	bool remove(int32_t slot) {
		if (slot < 0 || slot >= kNumSlots || slots[slot].empty()) {
			return false;
		}
		slots[slot] = Slot{};
		return true;
	}

	/// Swaps the slot with its neighbour (direction -1 or +1; slot 3 -> 4 goes to the bottom row). Returns the new
	/// position of the module, or -1 if it can't move.
	int32_t move(int32_t slot, int32_t direction) {
		int32_t to = slot + direction;
		if (slot < 0 || slot >= kNumSlots || slots[slot].empty() || to < 0 || to >= kNumSlots) {
			return -1;
		}
		Slot tmp = slots[to];
		slots[to] = slots[slot];
		slots[slot] = tmp;
		return to;
	}

	bool toggle(int32_t slot) {
		if (slot < 0 || slot >= kNumSlots || slots[slot].empty()) {
			return false;
		}
		slots[slot].on = !slots[slot].on;
		return true;
	}

	// Saving: 2 bytes per slot (type, on) then dirMode.
	static constexpr int32_t kNumSaveBytes = kNumSlots * 2 + 1;
	void toBytes(uint8_t* b) const {
		for (int32_t i = 0; i < kNumSlots; i++) {
			b[i * 2] = static_cast<uint8_t>(slots[i].type);
			b[i * 2 + 1] = slots[i].on;
		}
		b[kNumSlots * 2] = dirMode;
	}
	void fromBytes(const uint8_t* b, int32_t len) {
		*this = Rack{};
		for (int32_t i = 0; i < kNumSlots && i * 2 + 1 < len; i++) {
			Type t = static_cast<Type>(b[i * 2]);
			if (t > Type::NONE && t < Type::NUM_TYPES && !contains(t)) {
				slots[i] = Slot{t, b[i * 2 + 1] != 0};
			}
		}
		if (len > kNumSlots * 2 && b[kNumSlots * 2] <= 2) {
			dirMode = b[kNumSlots * 2];
		}
	}
};

} // namespace deluge::note_fx
