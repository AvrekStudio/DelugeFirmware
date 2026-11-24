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

#include "gui/menu_item/two_row_menu.h"

// The VARI screen: "VARI" written down a thin white strip, and the settings as 2 x 4 cells (CHNC VELO NOTE OCT /
// GATE RATC HUMN MUT): the value, and the setting's short name under it.

namespace deluge::gui::menu_item::vari {

class Screen final : public TwoRowMenu {
public:
	using TwoRowMenu::TwoRowMenu;
	void renderOLED() override;
	void selectEncoderAction(int32_t offset) override;
	ActionResult buttonAction(hid::Button b, bool on, bool inCardRoutine) override;
	void endSession() override;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	/// SELECT on any setting: asks "PRINT NOTES?" - NO / YES with SELECT turned, SELECT again to confirm
	void selectPressed();

private:
	bool confirming_ = false; // the PRINT NOTES question is up
	bool yes_ = false;
	void drawPrintQuestion();
};

/// The VARI screen open in the sound editor, if any
Screen* openScreen();

} // namespace deluge::gui::menu_item::vari
