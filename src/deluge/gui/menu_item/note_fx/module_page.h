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

#include "gui/menu_item/note_fx/rack_menu.h"
#include "model/note_fx/note_fx_params.h"

namespace deluge::gui::menu_item::note_fx {

/// A module's EDIT page (ARP, EUCL, MONO): its parameters on two rows of four, as on the MASTER page. SYNTH / KIT /
/// MIDI / CV pick the column, SYNC-SCALING / TRIPLETS VIEW the row, turning SELECT changes the value.
/// P-lock: hold a note on the grid - the page then shows and edits that note's own values (marked with a square);
/// clicking a locked value removes its lock. ARP: clicking RATE shows SYNC in its place (and back).
class ModulePage final : public RackScreen {
public:
	ModulePage(l10n::String name, Type type) : RackScreen(name), type_(type) {}

	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void renderOLED() override;
	void drawName() override;
	void selectEncoderAction(int32_t offset) override;
	MenuItem* selectButtonPress() override;
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;

	/// Grid pads while this page is open: holding a note selects it for p-locking. Returns true if dealt with.
	bool padAction(int32_t x, int32_t y, bool on);

private:
	/// The parameter shown in a cell, or -1 for an empty cell.
	int32_t cellParam(int32_t cell) const;
	int32_t value(int32_t param, bool* locked) const;
	void setValue(int32_t param, int32_t value);

	Type type_;
	int32_t selected_ = 0;
	bool showSync_ = false;
};

extern ModulePage noteFxArpPage;
extern ModulePage noteFxEuclPage;
extern ModulePage noteFxMonoPage;

/// Called by the sound editor for grid pads: routes them to a module page if one is open.
bool modulePagePadAction(int32_t x, int32_t y, bool on);

} // namespace deluge::gui::menu_item::note_fx
