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

/// VARI (SOUND menu): live, non-destructive variations of the grid's notes as they play (grid -> VARI -> CHORD ->
/// ARP). Every amount is 0-100 %. CHNC is the track's RAND of the MASTER menu (the same value).
namespace deluge::vari {

enum Param : uint8_t { VELO, NOTE, OCT, GATE, RATC, HUMN, MUT, NUM_PARAMS };

constexpr std::array<const char*, NUM_PARAMS> kLabels = {"VELO", "NOTE", "OCT", "GATE", "RATC", "HUMN", "MUT"};
constexpr std::array<const char*, NUM_PARAMS> kSaveNames = {"variVelo",    "variNote",  "variOct",   "variGate",
                                                            "variRatchet", "variHuman", "variMutate"};
constexpr int32_t kMax = 100;

struct Settings {
	std::array<uint8_t, NUM_PARAMS> amount{0, 0, 0, 0, 0, 0, 100}; // MUT 100 % = a new variation on every pass

	[[nodiscard]] bool anyVariation() const {
		for (int32_t p = 0; p < MUT; p++) {
			if (amount[p] != 0) {
				return true;
			}
		}
		return false;
	}
};

} // namespace deluge::vari
