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

#include "gui/menu_item/integer.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/toggle.h"
#include "gui/menu_item/two_row_menu.h"

// The EUC page of a note row (hold its AUD pad, click the horizontal encoder), Elektron style:
// PL1 PL2 LEN EUC / RO1 RO2 TRO OP

namespace deluge::gui::menu_item::euclid {

enum class Field : uint8_t { PL1, PL2, LEN, RO1, RO2, TRO };

/// PL1 / PL2 (0 to LEN), LEN (steps of 1/16, the row's length), RO1 / RO2 / TRO (-LEN/2 to +LEN/2, wrapping)
class Param final : public Integer {
public:
	Param(Field field, const char* label) : Integer(l10n::String::EMPTY_STRING), field_(field), label_(label) {}
	void readCurrentValue() override;
	void writeCurrentValue() override;
	void selectEncoderAction(int32_t offset) override;
	[[nodiscard]] int32_t getMinValue() const override;
	[[nodiscard]] int32_t getMaxValue() const override;
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append(label_); }
	[[nodiscard]] std::string_view getName() const override { return label_; }

private:
	Field field_;
	const char* label_;
};

/// EUC: on, the row plays the pattern (its notes drawn before are kept aside and come back when off)
class Enabled final : public Toggle {
public:
	Enabled() : Toggle(l10n::String::EMPTY_STRING) {}
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append("EUC"); }
	[[nodiscard]] std::string_view getName() const override { return "EUC"; }
};

/// OP: how the two generators combine
class Operator final : public Selection {
public:
	Operator() : Selection(l10n::String::EMPTY_STRING) {}
	void readCurrentValue() override;
	void writeCurrentValue() override;
	deluge::vector<std::string_view> getOptions(OptType optType) override { return {"OR", "AND", "XOR"}; }
	bool wrapAround() override { return true; }
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append("OP"); }
	[[nodiscard]] std::string_view getName() const override { return "OP"; }
};

class Screen final : public TwoRowMenu {
public:
	using TwoRowMenu::TwoRowMenu;
	[[nodiscard]] std::string_view getTitle() const override { return "EUC"; }
	[[nodiscard]] std::string_view getName() const override { return "EUC"; }
	/// SELECT (or LOAD): EUC on / off
	void selectPressed();
};

extern Screen euclidScreen;

} // namespace deluge::gui::menu_item::euclid
