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

#include "model/note_fx/note_fx_rack.h"
#include <array>
#include <cstdint>
#include <cstdio>

/// The NOTE FX modules' parameters: what they are, their ranges, defaults and how they are shown.
/// Values are per track (Instrument); a clip can lock any of them to its own value (Hapax-style p-lock).
namespace deluge::note_fx {

constexpr int32_t kMaxParams = 10;

enum ArpParam : uint8_t {
	ARP_STYLE,
	ARP_RATE,
	ARP_GATE,
	ARP_HUMAN,
	ARP_OCTAVE,
	ARP_CHORD,
	ARP_RETRIG,
	ARP_REPEAT,
	ARP_SYNC,     ///< under RATE: clicking RATE shows it
	ARP_FREE_RATE ///< RATE while SYNC is OFF: 0..100%
};
enum EuclParam : uint8_t { EUCL_NOTE, EUCL_RATE, EUCL_STEPS, EUCL_PULSES, EUCL_ROTATE, EUCL_GATE };
enum MonoParam : uint8_t { MONO_MODE, MONO_LEGATO, MONO_RETRIG };

enum ArpSync : uint8_t { SYNC_OFF, SYNC_ON, SYNC_LOCKED };
enum MonoMode : uint8_t { MONO_FIRST, MONO_LAST, MONO_RANDOM, MONO_LOW, MONO_HIGH };

// ARP styles (Hapax order)
constexpr int32_t kNumArpStyles = 30;
constexpr const char* kArpStyleShort[kNumArpStyles] = {
    "ORDR", "UP",  "DOWN", "U/D", "D/U", "U+D", "D+U", "RAND", "VELU", "VELD", "CNVU", "CNVD", "DIVU", "DIVD", "CVDV",
    "DVCV", "PKU", "PKD",  "THU", "THD", "R2U", "R3U", "R4U",  "R2D",  "R3D",  "R4D",  "D12",  "S12",  "D13",  "S13",
};
enum ArpStyle : uint8_t {
	STYLE_ORDER,
	STYLE_UP,
	STYLE_DOWN,
	STYLE_UP_DOWN,
	STYLE_DOWN_UP,
	STYLE_UP_AND_DOWN,
	STYLE_DOWN_AND_UP,
	STYLE_RANDOM,
	STYLE_VELOCITY_UP,
	STYLE_VELOCITY_DOWN,
	STYLE_CONVERGE_UP,
	STYLE_CONVERGE_DOWN,
	STYLE_DIVERGE_UP,
	STYLE_DIVERGE_DOWN,
	STYLE_CONVERGE_DIVERGE,
	STYLE_DIVERGE_CONVERGE,
	STYLE_PINKY_UP,
	STYLE_PINKY_DOWN,
	STYLE_THUMB_UP,
	STYLE_THUMB_DOWN,
	STYLE_REGRESS2_UP,
	STYLE_REGRESS3_UP,
	STYLE_REGRESS4_UP,
	STYLE_REGRESS2_DOWN,
	STYLE_REGRESS3_DOWN,
	STYLE_REGRESS4_DOWN,
	STYLE_DOUBLE_1_2,
	STYLE_SILENCE_1_2,
	STYLE_DOUBLE_1_3,
	STYLE_SILENCE_1_3,
};

// Synced rates, as fractions of a bar
constexpr int32_t kNumRates = 12;
constexpr int32_t kRateDivisions[kNumRates] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64};
constexpr int32_t kRate16th = 7;

// Re-trig: off, on every added note, then every N bars / note values (in 1/192 of a bar)
constexpr int32_t kNumRetrigs = 10;
constexpr const char* kRetrigShort[kNumRetrigs] = {"--",   "NOTE", "8BAR", "4BAR", "2BAR",
                                                   "1BAR", "1/2",  "1/4",  "1/8",  "1/16"};
constexpr int32_t kRetrigBars192[kNumRetrigs] = {0, 0, 8 * 192, 4 * 192, 2 * 192, 192, 96, 48, 24, 12};

constexpr const char* kMonoModeShort[5] = {"FRST", "LAST", "RAND", "LOW", "HIGH"};

// EUCL NOTE: IN (the notes played), TRK (the track's default NOTE, MASTER menu), then a fixed note 0..127
constexpr int32_t kEuclNoteIn = -2;
constexpr int32_t kEuclNoteTrack = -1;
// EUCL GATE: TRK = the track's LEN (MASTER menu), then 0..200% of a step
constexpr int32_t kEuclGateTrack = -1;

struct ParamSpec {
	const char* label; ///< 4 letters max
	int16_t min;
	int16_t max;
	int16_t def;
};

inline const ParamSpec* paramSpecs(Type t, int32_t* count) {
	static constexpr ParamSpec kArp[] = {
	    {"STYL", 0, kNumArpStyles - 1, STYLE_UP},
	    {"RATE", 0, kNumRates - 1, kRate16th},
	    {"GATE", 1, 200, 50},
	    {"HUMN", 0, 100, 0},
	    {"OCT", -5, 5, 0},
	    {"CHRD", -2, 2, 0},
	    {"RTRG", 0, kNumRetrigs - 1, 0},
	    {"RPT", 0, 16, 0},
	    {"SYNC", 0, 2, SYNC_ON},
	    {"RATE", 0, 100, 50},
	};
	static constexpr ParamSpec kEucl[] = {
	    {"NOTE", kEuclNoteIn, 127, kEuclNoteTrack},
	    {"RATE", 0, kNumRates - 1, kRate16th},
	    {"STEP", 1, 32, 16},
	    {"PULS", 1, 32, 4},
	    {"ROT", 0, 31, 0},
	    {"GATE", kEuclGateTrack, 200, kEuclGateTrack},
	};
	static constexpr ParamSpec kMono[] = {
	    {"MODE", 0, 4, MONO_LAST},
	    {"LEGA", 0, 1, 0},
	    {"RTRG", 0, 1, 1},
	};
	switch (t) {
	case Type::ARP:
		*count = 10;
		return kArp;
	case Type::EUCL:
		*count = 6;
		return kEucl;
	case Type::MONO:
		*count = 3;
		return kMono;
	default:
		*count = 0;
		return nullptr;
	}
}

/// All the modules' values, indexed by module type then parameter.
struct Values {
	std::array<std::array<int16_t, kMaxParams>, static_cast<int32_t>(Type::NUM_TYPES)> v{};

	Values() { setDefaults(); }
	void setDefaults() {
		for (int32_t t = 0; t < static_cast<int32_t>(Type::NUM_TYPES); t++) {
			int32_t n;
			const ParamSpec* s = paramSpecs(static_cast<Type>(t), &n);
			for (int32_t p = 0; p < kMaxParams; p++) {
				v[t][p] = p < n ? s[p].def : 0;
			}
		}
	}
	[[nodiscard]] int16_t get(Type t, int32_t p) const { return v[static_cast<int32_t>(t)][p]; }
	void set(Type t, int32_t p, int32_t value) {
		int32_t n;
		const ParamSpec* s = paramSpecs(t, &n);
		if (p < 0 || p >= n) {
			return;
		}
		if (value < s[p].min) {
			value = s[p].min;
		}
		if (value > s[p].max) {
			value = s[p].max;
		}
		v[static_cast<int32_t>(t)][p] = static_cast<int16_t>(value);
	}

	static constexpr int32_t kNumSaveBytes = static_cast<int32_t>(Type::NUM_TYPES) * kMaxParams * 2;
	void toBytes(uint8_t* b) const {
		int32_t i = 0;
		for (auto& m : v) {
			for (int16_t x : m) {
				b[i++] = static_cast<uint8_t>(x & 0xFF);
				b[i++] = static_cast<uint8_t>((x >> 8) & 0xFF);
			}
		}
	}
	/// Reads what was saved, keeping each value in its range (unknown / missing ones keep their default).
	void fromBytes(const uint8_t* b, int32_t len) {
		setDefaults();
		for (int32_t t = 0; t < static_cast<int32_t>(Type::NUM_TYPES); t++) {
			int32_t n;
			paramSpecs(static_cast<Type>(t), &n);
			for (int32_t p = 0; p < n; p++) {
				int32_t i = (t * kMaxParams + p) * 2;
				if (i + 1 < len) {
					set(static_cast<Type>(t), p, static_cast<int16_t>(b[i] | (b[i + 1] << 8)));
				}
			}
		}
	}
};

/// Parameter value as shown in its cell (5 chars max). buf: 8 chars.
inline void formatParam(Type t, int32_t p, int32_t value, char* buf, size_t size) {
	auto signedOrDash = [&](int32_t x) {
		if (x == 0) {
			snprintf(buf, size, "--");
		}
		else {
			snprintf(buf, size, "%+d", static_cast<int>(x));
		}
	};
	switch (t) {
	case Type::ARP:
		switch (p) {
		case ARP_STYLE:
			snprintf(buf, size, "%s", kArpStyleShort[value]);
			return;
		case ARP_RATE:
			snprintf(buf, size, "1/%d", static_cast<int>(kRateDivisions[value]));
			return;
		case ARP_GATE:
		case ARP_HUMAN:
			snprintf(buf, size, "%d%%", static_cast<int>(value));
			return;
		case ARP_OCTAVE:
		case ARP_CHORD:
			signedOrDash(value);
			return;
		case ARP_RETRIG:
			snprintf(buf, size, "%s", kRetrigShort[value]);
			return;
		case ARP_REPEAT:
			if (value == 0) {
				snprintf(buf, size, "--");
			}
			else {
				snprintf(buf, size, "x%d", static_cast<int>(value));
			}
			return;
		case ARP_SYNC:
			snprintf(buf, size, "%s", value == SYNC_OFF ? "OFF" : (value == SYNC_ON ? "ON" : "LOCK"));
			return;
		case ARP_FREE_RATE:
			snprintf(buf, size, "%d%%", static_cast<int>(value));
			return;
		}
		break;
	case Type::EUCL:
		switch (p) {
		case EUCL_NOTE:
			if (value == kEuclNoteIn) {
				snprintf(buf, size, "IN");
			}
			else if (value == kEuclNoteTrack) {
				snprintf(buf, size, "TRK");
			}
			else {
				static constexpr const char* kNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
				                                           "F#", "G",  "G#", "A",  "A#", "B"};
				snprintf(buf, size, "%s%d", kNames[value % 12], static_cast<int>(value / 12 - 2));
			}
			return;
		case EUCL_RATE:
			snprintf(buf, size, "1/%d", static_cast<int>(kRateDivisions[value]));
			return;
		case EUCL_GATE:
			if (value == kEuclGateTrack) {
				snprintf(buf, size, "TRK");
			}
			else {
				snprintf(buf, size, "%d%%", static_cast<int>(value));
			}
			return;
		default:
			snprintf(buf, size, "%d", static_cast<int>(value));
			return;
		}
	case Type::MONO:
		if (p == MONO_MODE) {
			snprintf(buf, size, "%s", kMonoModeShort[value]);
		}
		else {
			snprintf(buf, size, "%s", value ? "ON" : "OFF");
		}
		return;
	default:
		break;
	}
	snprintf(buf, size, "%d", static_cast<int>(value));
}

} // namespace deluge::note_fx
