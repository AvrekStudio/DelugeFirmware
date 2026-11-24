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

#include "gui/menu_item/track_defaults/track_defaults_menus.h"
#include "model/instrument/vari_settings.h"

/// VARI (SOUND menu): CHNC VELO NOTE OCT / GATE RATC HUMN MUT, all 0-100 %. CHNC = the MASTER menu's RAND.
namespace deluge::gui::menu_item::vari {

using track_defaults::TextInteger;

/// One VARI amount, 0-100 %
class Amount final : public TextInteger {
public:
	Amount(l10n::String name, deluge::vari::Param param) : TextInteger(name), param_(param) {}
	void readCurrentValue() override {
		Instrument* instrument = getCurrentInstrument();
		setValue(instrument != nullptr ? instrument->vari.amount[param_] : 0);
	}
	void writeCurrentValue() override {
		if (Instrument* instrument = getCurrentInstrument()) {
			instrument->vari.amount[param_] = static_cast<uint8_t>(getValue());
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return deluge::vari::kMax; }
	void getColumnLabel(StringBuf& label) override { label.append(deluge::vari::kLabels[param_]); }

protected:
	void format(int32_t value, char* out, size_t size) override {
		if (value == 0 && param_ != deluge::vari::MUT) {
			snprintf(out, size, "OFF");
		}
		else {
			snprintf(out, size, "%d%%", static_cast<int>(value));
		}
	}

private:
	deluge::vari::Param param_;
};

/// CHNC: the chance that each note plays - the same value as the MASTER menu's RAND
class Chance final : public TextInteger {
public:
	using TextInteger::TextInteger;
	void readCurrentValue() override {
		Instrument* instrument = getCurrentInstrument();
		setValue(instrument != nullptr ? instrument->noteDefaults.chance : deluge::track_defaults::kChanceMax);
	}
	void writeCurrentValue() override {
		if (Instrument* instrument = getCurrentInstrument()) {
			instrument->noteDefaults.chance = static_cast<uint8_t>(getValue());
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return deluge::track_defaults::kChanceMax; }
	void getColumnLabel(StringBuf& label) override { label.append("CHNC"); }

protected:
	void format(int32_t value, char* out, size_t size) override {
		snprintf(out, size, "%d%%", static_cast<int>(value));
	}
};

} // namespace deluge::gui::menu_item::vari
