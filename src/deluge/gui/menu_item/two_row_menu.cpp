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

#include "gui/menu_item/two_row_menu.h"
#include "definitions_cxx.hpp"
#include "gui/menu_item/vertical_title.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/views/timeline_view.h"
#include "gui/views/view.h"
#include "hid/button.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/led/indicator_leds.h"
#include "util/d_stringbuf.h"

namespace deluge::gui::menu_item {

namespace {
constexpr int32_t kCellX0 = kVerticalTitleWidth + 1;
constexpr int32_t kCellW = (OLED_MAIN_WIDTH_PIXELS - kCellX0) / TwoRowMenu::kColumns; // 29
constexpr int32_t kRowH = 21;
constexpr int32_t kRowY0 = OLED_MAIN_TOPMOST_PIXEL;
constexpr int32_t kRowY1 = OLED_MAIN_TOPMOST_PIXEL + kRowH + 1; // rows 5-25 and 27-47
constexpr int32_t kValueH = 13;
constexpr int32_t kLabelOffsetY = 15;
constexpr size_t kLabelChars = 4; // labels cut to 4 letters, like the Deluge's column labels
} // namespace

int32_t TwoRowMenu::selectedIndex() const {
	const MenuItem* current = getCurrentItem();
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < kMaxItems; i++) {
		if (items[i] == current) {
			return i;
		}
	}
	return 0;
}

void TwoRowMenu::select(int32_t index) {
	if (index < 0 || index >= static_cast<int32_t>(items.size()) || index >= kMaxItems) {
		return;
	}
	if (!isItemRelevant(items[index])) {
		return;
	}
	focusChild(items[index]);
	lightButtons();
	if (display->haveOLED()) {
		renderUIsForOled();
	}
	else {
		items[index]->drawName();
	}
}

void TwoRowMenu::lightButtons() const {
	using namespace indicator_leds;
	// SYNTH / KIT / MIDI / CV pick the column: the selected one is lit
	const int32_t col = selectedIndex() % kColumns;
	setLedState(IndicatorLED::SYNTH, col == 0);
	setLedState(IndicatorLED::KIT, col == 1);
	setLedState(IndicatorLED::MIDI, col == 2);
	setLedState(IndicatorLED::CV, col == 3);
	setLedState(IndicatorLED::SYNC_SCALING, true);
	setLedState(IndicatorLED::TRIPLETS, true);
	setLedState(IndicatorLED::LOAD, true);
	setLedState(IndicatorLED::SCALE_MODE, false);
	setLedState(IndicatorLED::CROSS_SCREEN_EDIT, false);
}

void TwoRowMenu::beginSession(MenuItem* navigatedBackwardFrom) {
	HorizontalMenu::beginSession(navigatedBackwardFrom);
	lightButtons();
}

void TwoRowMenu::endSession() {
	HorizontalMenu::endSession();
	indicator_leds::setLedState(IndicatorLED::SYNTH, false);
	indicator_leds::setLedState(IndicatorLED::KIT, false);
	indicator_leds::setLedState(IndicatorLED::MIDI, false);
	indicator_leds::setLedState(IndicatorLED::CV, false);
	indicator_leds::setLedState(IndicatorLED::LOAD, false);
	view.setTimeBaseScaleLedState();
	TimelineView* timelineView = getRootUI() != nullptr ? getRootUI()->toTimelineView() : nullptr;
	if (timelineView != nullptr) {
		timelineView->setTripletsLEDState();
	}
	else {
		indicator_leds::setLedState(IndicatorLED::TRIPLETS, false);
	}
}

void TwoRowMenu::renderOLED() {
	using deluge::hid::display::OLED;
	auto& image = OLED::main;
	image.clear();
	drawVerticalTitle(getTitle());

	const MenuItem* current = getCurrentItem();
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < kMaxItems; i++) {
		MenuItem* item = items[i];
		const int32_t x = kCellX0 + (i % kColumns) * kCellW;
		const int32_t y = (i < kColumns) ? kRowY0 : kRowY1;

		if (!isItemRelevant(item)) {
			continue; // an empty cell
		}

		item->readCurrentValue();
		item->renderInHorizontalMenu({.start_x = static_cast<uint8_t>(x),
		                              .start_y = static_cast<uint8_t>(y),
		                              .width = static_cast<uint8_t>(kCellW - 1),
		                              .height = static_cast<uint8_t>(kValueH)});

		DEF_STACK_STRING_BUF(label, 12);
		item->getColumnLabel(label);
		label.removeSpaces();
		if (label.size() > kLabelChars) {
			label.truncate(kLabelChars);
		}
		image.drawStringCentered(label, x, y + kLabelOffsetY, kTextSmallSpacingX, kTextSmallSizeY, kCellW - 1);

		if (item == current) {
			image.invertArea(x, kCellW - 1, y, y + kRowH - 1);
		}
	}
	OLED::markChanged();
}

ActionResult TwoRowMenu::buttonAction(hid::Button b, bool on, bool inCardRoutine) {
	using namespace hid::button;

	if (b == SYNTH || b == KIT || b == MIDI || b == CV) {
		if (on) {
			const int32_t col = b == SYNTH ? 0 : (b == KIT ? 1 : (b == MIDI ? 2 : 3));
			const int32_t row = selectedIndex() / kColumns;
			select(row * kColumns + col);
		}
		return ActionResult::DEALT_WITH;
	}
	if (b == SYNC_SCALING || b == TRIPLETS) {
		if (on) {
			const int32_t rowStart = (b == SYNC_SCALING ? 0 : 1) * kColumns;
			int32_t target = rowStart + selectedIndex() % kColumns;
			// that cell is empty: the row's first used one
			for (int32_t col = 0;
			     col < kColumns && (target >= static_cast<int32_t>(items.size()) || !isItemRelevant(items[target]));
			     col++) {
				target = rowStart + col;
			}
			select(target);
		}
		return ActionResult::DEALT_WITH;
	}
	if (b == LOAD) {
		if (on) {
			if (inCardRoutine) {
				return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
			}
			Buttons::selectButtonPressUsedUp = false;
			return soundEditor.buttonAction(SELECT_ENC, false, inCardRoutine);
		}
		return ActionResult::DEALT_WITH;
	}
	if ((b == SCALE_MODE || b == CROSS_SCREEN_EDIT) && !Buttons::isButtonPressed(SHIFT)) {
		return ActionResult::DEALT_WITH; // everything is on one page
	}
	return HorizontalMenu::buttonAction(b, on, inCardRoutine);
}

} // namespace deluge::gui::menu_item
