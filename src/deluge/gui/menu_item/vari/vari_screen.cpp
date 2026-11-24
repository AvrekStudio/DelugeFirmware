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

#include "gui/menu_item/vari/vari_screen.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/vertical_title.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include <array>
#include <cstdio>

namespace deluge::gui::menu_item::vari {

using deluge::hid::display::OLED;
using Canvas = deluge::hid::display::oled_canvas::Canvas;

namespace {

constexpr int32_t kStepMs = 350; // how long a foot stays up after a turn

// Cells: 4 columns of 20 px from x = 48, 2 rows of 24 px
constexpr int32_t kCellsX = 48;
constexpr int32_t kCellW = 20;
constexpr int32_t kCellH = 24;

// Robot
constexpr int32_t kHeadX0 = 16;
constexpr int32_t kHeadX1 = 39;
constexpr int32_t kHeadY0 = 9;
constexpr int32_t kHeadY1 = 26;
constexpr int32_t kRobotCentre = 28;

// The setting under each cell: index = the cell, in the screen's order
enum Sign : uint8_t { CHNC, VELO, NOTE, OCT, GATE, RATC, HUMN, MUT };
constexpr std::array<const char*, 8> kLabels = {"CHNC", "VELO", "NOTE", "OCT", "GATE", "RATC", "HUMN", "MUT"};
constexpr std::array<const char*, 8> kNames = {"CHANCE", "VELOCITY", "NOTE",  "OCTAVE",
                                               "GATE",   "RATCHET",  "HUMAN", "MUTATE"};

void fill(Canvas& c, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
	for (int32_t y = y0; y <= y1; y++) {
		for (int32_t x = x0; x <= x1; x++) {
			c.drawPixel(x, y);
		}
	}
}

void smallText(Canvas& c, const char* text, int32_t centreX, int32_t y) {
	const int32_t w = c.getStringWidthInPixels(text, kTextSmallSizeY);
	c.drawString(text, centreX - w / 2, y, kTextSmallSpacingX, kTextSmallSizeY);
}

void drawRobot(Canvas& c, int32_t variation, int32_t chance, int32_t step, const char* name) {
	// antenna, swaying while a value turns
	const int32_t sway = step;
	fill(c, 26 + sway, 3, 30 + sway, 5);
	c.drawPixel(kRobotCentre + sway, 6);
	c.drawPixel(kRobotCentre + (sway != 0 ? sway : 0), 7);
	c.drawPixel(kRobotCentre, 8);

	// head
	for (int32_t x = kHeadX0 + 1; x < kHeadX1; x++) {
		c.drawPixel(x, kHeadY0);
		c.drawPixel(x, kHeadY1);
	}
	for (int32_t y = kHeadY0 + 1; y < kHeadY1; y++) {
		c.drawPixel(kHeadX0, y);
		c.drawPixel(kHeadX1, y);
	}

	// eyes: how much the notes vary - dots, rings, then crosses
	for (const int32_t ex : {22, 32}) {
		if (variation < 34) {
			fill(c, ex, 16, ex + 2, 18);
		}
		else if (variation < 67) {
			fill(c, ex, 16, ex + 2, 16);
			fill(c, ex, 18, ex + 2, 18);
			c.drawPixel(ex, 17);
			c.drawPixel(ex + 2, 17);
		}
		else {
			c.drawPixel(ex, 16);
			c.drawPixel(ex + 2, 16);
			c.drawPixel(ex + 1, 17);
			c.drawPixel(ex, 18);
			c.drawPixel(ex + 2, 18);
		}
	}
	// cheeks and mouth: the wider, the more notes play
	c.drawPixel(19, 21);
	c.drawPixel(36, 21);
	const int32_t mouth = 5 + chance * 8 / 100;
	fill(c, kRobotCentre - mouth / 2, 22, kRobotCentre - mouth / 2 + mouth - 1, 23);

	// feet, stepping while a value turns
	const int32_t leftY = 30 + (step > 0 ? -2 : (step < 0 ? 1 : 0));
	const int32_t rightY = 30 + (step < 0 ? -2 : (step > 0 ? 1 : 0));
	fill(c, 19, leftY, 23, leftY + 1);
	fill(c, 32, rightY, 36, rightY + 1);

	smallText(c, name, kRobotCentre, 37);
}

} // namespace

void Screen::renderOLED() {
	auto& image = OLED::main;
	image.clear();
	drawVerticalStrip(getTitle(), 1, 8, 0, true);

	std::array<int32_t, 8> values{};
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < 8; i++) {
		auto* item = static_cast<Integer*>(items[i]);
		item->readCurrentValue();
		values[i] = item->getValue();
	}
	int32_t variation = 0;
	for (int32_t i = VELO; i <= HUMN; i++) {
		variation = std::max(variation, values[i]);
	}

	const MenuItem* current = getCurrentItem();
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < 8; i++) {
		if (items[i] == current) {
			currentIndex = i;
		}
	}
	drawRobot(image, variation, values[CHNC], step_, kNames[currentIndex]);

	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < 8; i++) {
		const int32_t x0 = kCellsX + (i % 4) * kCellW;
		const int32_t y0 = (i / 4) * kCellH;
		char text[8];
		if (values[i] == 0 && i != CHNC && i != MUT) {
			snprintf(text, sizeof(text), "OFF");
		}
		else {
			snprintf(text, sizeof(text), "%d%%", static_cast<int>(values[i]));
		}
		// the value above, the setting's name under it
		smallText(image, text, x0 + kCellW / 2, y0 + 6);
		smallText(image, kLabels[i], x0 + kCellW / 2, y0 + 14);
		if (i == currentIndex) {
			image.invertAreaRounded(x0, kCellW, y0, y0 + kCellH - 1);
		}
	}
	OLED::markChanged();
}

void Screen::selectEncoderAction(int32_t offset) {
	TwoRowMenu::selectEncoderAction(offset);
	if (display->hasPopup()) {
		display->cancelPopup();
	}
	// the robot steps (one foot then the other) and sways its antenna while the value turns
	step_ = step_ > 0 ? -1 : 1;
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kStepMs);
	renderUIsForOled();
}

ActionResult Screen::buttonAction(hid::Button b, bool on, bool inCardRoutine) {
	const ActionResult result = TwoRowMenu::buttonAction(b, on, inCardRoutine);
	if (display->hasPopup()) {
		display->cancelPopup();
	}
	return result;
}

ActionResult Screen::timerCallback() {
	step_ = 0; // back at rest
	renderUIsForOled();
	return ActionResult::DEALT_WITH;
}

void Screen::endSession() {
	TwoRowMenu::endSession();
	uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
	step_ = 0;
}

} // namespace deluge::gui::menu_item::vari
