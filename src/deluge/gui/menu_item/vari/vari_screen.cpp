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
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/song/song.h"
#include "processing/sound/sound_instrument.h"
#include <array>
#include <cstdio>

namespace deluge::gui::menu_item::vari {

using deluge::hid::display::OLED;
using Canvas = deluge::hid::display::oled_canvas::Canvas;

namespace {

// Cells: 4 columns of 29 px from x = 10 (right of the strip), 2 rows of 24 px
constexpr int32_t kCellsX = 10;
constexpr int32_t kCellW = 29;
constexpr int32_t kCellH = 24;

// The setting under each cell: index = the cell, in the screen's order
enum Sign : uint8_t { CHNC, VELO, NOTE, OCT, GATE, RATC, HUMN, MUT };
constexpr std::array<const char*, 8> kLabels = {"CHNC", "VELO", "NOTE", "OCT", "GATE", "RATC", "HUMN", "MUT"};

void smallText(Canvas& c, const char* text, int32_t centreX, int32_t y) {
	const int32_t w = c.getStringWidthInPixels(text, kTextSmallSizeY);
	c.drawString(text, centreX - w / 2, y, kTextSmallSpacingX, kTextSmallSizeY);
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
	const MenuItem* current = getCurrentItem();
	int32_t currentIndex = 0;
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < 8; i++) {
		if (items[i] == current) {
			currentIndex = i;
		}
	}
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
		// the value above, the setting's name under it (inverted on the one chosen)
		smallText(image, text, x0 + kCellW / 2, y0 + 3);
		smallText(image, kLabels[i], x0 + kCellW / 2, y0 + 14);
		if (i == currentIndex) {
			image.invertAreaRounded(x0 + 1, kCellW - 2, y0 + 12, y0 + 12 + kTextSmallSizeY + 2);
		}
	}
	if (confirming_) {
		drawPrintQuestion();
	}
	OLED::markChanged();
}

namespace {
Screen* shownScreen = nullptr;
}

Screen* openScreen() {
	if (shownScreen == nullptr || getCurrentUI() != &soundEditor || soundEditor.getCurrentMenuItem() != shownScreen) {
		return nullptr;
	}
	return shownScreen;
}

void Screen::beginSession(MenuItem* navigatedBackwardFrom) {
	TwoRowMenu::beginSession(navigatedBackwardFrom);
	shownScreen = this;
	confirming_ = false;
}

void Screen::selectPressed() {
	if (!confirming_) {
		confirming_ = true;
		yes_ = false; // NO first: printing changes the grid
		renderUIsForOled();
		return;
	}
	confirming_ = false;
	if (yes_ && getCurrentOutputType() == OutputType::SYNTH) {
		auto* sound = static_cast<SoundInstrument*>(getCurrentOutput());
		if (sound->variPrint(getCurrentInstrumentClip())) {
			display->popupTextTemporary("NOTES PRINTED");
		}
	}
	renderUIsForOled();
}

void Screen::drawPrintQuestion() {
	auto& image = OLED::main;
	constexpr int32_t x0 = 44;
	constexpr int32_t x1 = 125;
	constexpr int32_t y0 = 8;
	constexpr int32_t y1 = 39;
	image.clearAreaExact(x0, y0, x1, y1);
	for (int32_t x = x0; x <= x1; x++) {
		image.drawPixel(x, y0);
		image.drawPixel(x, y1);
	}
	for (int32_t y = y0; y <= y1; y++) {
		image.drawPixel(x0, y);
		image.drawPixel(x1, y);
	}
	image.drawStringCentered("PRINT NOTES?", x0, y0 + 6, kTextSmallSpacingX, kTextSmallSizeY, x1 - x0 + 1);
	// NO / YES: the one chosen inverted
	constexpr int32_t kChoiceY = y0 + 18;
	constexpr int32_t kNoX = x0 + 10;
	constexpr int32_t kYesX = x0 + 46;
	constexpr int32_t kChoiceW = 26;
	image.drawStringCentered("NO", kNoX, kChoiceY, kTextSmallSpacingX, kTextSmallSizeY, kChoiceW);
	image.drawStringCentered("YES", kYesX, kChoiceY, kTextSmallSpacingX, kTextSmallSizeY, kChoiceW);
	const int32_t sx = yes_ ? kYesX : kNoX;
	image.invertAreaRounded(sx, kChoiceW, kChoiceY - 3, kChoiceY + kTextSmallSizeY + 2);
}

void Screen::selectEncoderAction(int32_t offset) {
	if (confirming_) {
		yes_ = offset > 0;
		renderUIsForOled();
		return;
	}
	TwoRowMenu::selectEncoderAction(offset);
	if (display->hasPopup()) {
		display->cancelPopup();
	}
	renderUIsForOled();
}

ActionResult Screen::buttonAction(hid::Button b, bool on, bool inCardRoutine) {
	const ActionResult result = TwoRowMenu::buttonAction(b, on, inCardRoutine);
	if (display->hasPopup()) {
		display->cancelPopup();
	}
	return result;
}

void Screen::endSession() {
	TwoRowMenu::endSession();
	confirming_ = false;
	shownScreen = nullptr;
}

} // namespace deluge::gui::menu_item::vari
