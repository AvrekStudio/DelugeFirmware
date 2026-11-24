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

// The VARI screen, after the old firmware's GENERATOR "robot" screen: "VARI" written down a thin white strip, a
// little robot (its eyes show how much the notes vary, its mouth the chance they play; its feet step and its antenna
// sways while a value turns) with the setting's name under it, and the settings as 2 x 4 cells: the value, and the
// setting's short name under it.

namespace deluge::gui::menu_item::vari {

class Screen final : public TwoRowMenu {
public:
	using TwoRowMenu::TwoRowMenu;
	void renderOLED() override;
	void selectEncoderAction(int32_t offset) override;
	ActionResult buttonAction(hid::Button b, bool on, bool inCardRoutine) override;
	ActionResult timerCallback() override;
	void endSession() override;

private:
	int8_t step_ = 0; // robot animation: 0 = at rest, +1 / -1 = which foot is up
};

} // namespace deluge::gui::menu_item::vari
