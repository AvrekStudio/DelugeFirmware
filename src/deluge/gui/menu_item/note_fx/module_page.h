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

#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"
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

	void endSession() override;
	/// ARP: follows the step playing on the grid.
	ActionResult timerCallback() override;

	/// Grid pads while this page is open - ARP: its step editor; the others: holding a note selects it for p-locking.
	bool padAction(int32_t x, int32_t y, bool on);
	/// ARP: the step held in its editor + the vertical encoder = that step's note offset, semitone by semitone.
	bool stepEncoderAction(int32_t offset);
	/// ARP: draws its step editor on the grid. Returns false if this page doesn't use the grid.
	bool renderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

private:
	bool arpPadAction(int32_t x, int32_t y, bool on);
	void lightButtons() const;
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

/// Clip view: the notes the NOTE FX EUCL module plays (fixed / track note) shown in red where they fall on the grid.
void renderEuclidNotes(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth]);

/// Called by the sound editor: route grid pads / the vertical encoder / the grid's rendering to the open module page.
bool modulePagePadAction(int32_t x, int32_t y, bool on);
bool modulePageVerticalEncoderAction(int32_t offset);
/// Returns true if the open module page draws the grid (then it has drawn `image` if given).
bool modulePageRenderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                          uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

} // namespace deluge::gui::menu_item::note_fx
