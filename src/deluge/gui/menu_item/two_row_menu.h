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

#include "gui/menu_item/horizontal_menu.h"

namespace deluge::gui::menu_item {

/// A horizontal menu showing up to 8 items on two rows of four, with the page name written vertically on the left.
/// SYNTH / KIT / MIDI / CV pick the column, SYNC-SCALING / TRIPLETS VIEW (lit) the top / bottom row, turning SELECT
/// edits the selected item, LOAD (lit) does what clicking SELECT does. SHIFT + SCALE / CROSS SCREEN still go to the
/// previous / next menu of the chain.
class TwoRowMenu : public HorizontalMenu {
public:
	using HorizontalMenu::HorizontalMenu;

	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void endSession() override;
	void renderOLED() override;
	ActionResult buttonAction(hid::Button b, bool on, bool inCardRoutine) override;

	static constexpr int32_t kColumns = 4;
	static constexpr int32_t kMaxItems = 8;

protected:
	/// Called when another cell gets the focus
	virtual void focusChanged() {}

private:
	int32_t selectedIndex() const;
	void select(int32_t index);
	void lightButtons() const;
};

} // namespace deluge::gui::menu_item
