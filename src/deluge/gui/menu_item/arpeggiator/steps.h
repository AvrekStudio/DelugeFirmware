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

#include "definitions_cxx.hpp"
#include "gui/colour/colour.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/two_row_menu.h"

// The synth ARP on one two-row screen: MODE GATE SYNC RATE / OCTA REPE LEN RHYT. With RHYT selected the grid is the
// arp's step sequence (Digitone style): row 4 the steps' notes, row 8 their ON / OFF.

namespace deluge::gui::menu_item::arpeggiator {

/// OCTA, -5 to +5: octaves added above (+) or below (-) the notes held
class OctaveSpan final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override;
	[[nodiscard]] int32_t getMaxValue() const override;
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }
	void getColumnLabel(StringBuf& label) override { label.append("OCTA"); }
};

/// LEN, 1 to 16: the number of steps of the step sequence
class StepCount final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override { return 1; }
	[[nodiscard]] int32_t getMaxValue() const override { return 16; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }
	void getColumnLabel(StringBuf& label) override { label.append("LEN"); }
};

/// RHYT: no value, only a place for the focus; while it has it the grid edits the steps
class StepsCell final : public MenuItem {
public:
	using MenuItem::MenuItem;
	void renderInHorizontalMenu(const SlotPosition& slot) override;
	void getColumnLabel(StringBuf& label) override { label.append("RHYT"); }
	[[nodiscard]] bool showNotification() const override { return false; }
	bool selectEncoderActionIsPermitted() override { return false; }
};

/// The ARP screen; with its StepsCell selected, the grid shows and edits the steps
class ArpScreen final : public TwoRowMenu {
public:
	ArpScreen(l10n::String newName, l10n::String newTitle, std::initializer_list<MenuItem*> newItems,
	          StepsCell& stepsCell)
	    : TwoRowMenu(newName, newTitle, newItems), stepsCell_(stepsCell) {}

	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void endSession() override;
	ActionResult timerCallback() override;

	/// The grid is the step editor (RHYT selected, over a clip's grid)
	[[nodiscard]] bool editingSteps() const;
	bool padAction(int32_t x, int32_t y, bool on);
	bool stepEncoderAction(int32_t offset);
	void renderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);
	void renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

protected:
	void focusChanged() override;

private:
	StepsCell& stepsCell_;
	int32_t heldStep_ = -1;  // the step (column) held, -1 = none
	int32_t view_ = 0;       // the note offset shown on row 4 (scrolled by the vertical encoder)
	int32_t drawnStep_ = -1; // the step playing when the grid was last drawn
	bool timerRunning_ = false;
	void startOrStopPlayhead();
	bool sidebarPadAction(int32_t x, int32_t y, bool on);
	void showSteps() const;
};

/// The ARP screen open in the sound editor, if any
ArpScreen* openArpScreen();

} // namespace deluge::gui::menu_item::arpeggiator
