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

#include "gui/menu_item/note_fx/rack_menu.h"
#include "definitions_cxx.hpp"
#include "gui/l10n/l10n.h"
#include "gui/menu_item/note_fx/module_page.h"
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
#include "model/clip/instrument_clip.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include <algorithm>
#include <cstdio>

namespace deluge::gui::menu_item::note_fx {

using deluge::note_fx::kNumSlots;
using deluge::note_fx::kSlotsPerRow;
using deluge::note_fx::longName;
using deluge::note_fx::shortName;

RackMenu noteFxRackMenu{l10n::String::STRING_FOR_NOTE_FX_RACK};
AddList noteFxAddList{l10n::String::STRING_FOR_NOTE_FX_RACK};
ActionList noteFxActionList{l10n::String::STRING_FOR_NOTE_FX_RACK};
DirEdit noteFxDirEdit{l10n::String::STRING_FOR_PLAY_DIRECTION};

namespace {

// The slot the cursor is on - shared by the rack and its two lists.
int32_t cursor = 0;

// Set when a module's EDIT page is opened from one of the lists (EDIT, or a module just added): BACK from it then
// skips the list and lands on the rack. Cleared whenever the rack itself begins, so leaving the menu some other way
// can't leave it set.
bool skipListOnReturn = false;

// The page EDIT opens for a module, or nullptr if it has none.
MenuItem* editPageFor(Type type) {
	switch (type) {
	case Type::DIR:
		return &noteFxDirEdit;
	case Type::ARP:
		return &noteFxArpPage;
	case Type::EUCL:
		return &noteFxEuclPage;
	case Type::MONO:
		return &noteFxMonoPage;
	default:
		return nullptr;
	}
}

MenuPermission refuseIfSkipping() {
	if (skipListOnReturn) {
		skipListOnReturn = false;
		return MenuPermission::NO; // SoundEditor::goUpOneLevel() skips an item that refuses
	}
	return MenuPermission::YES;
}

constexpr int32_t kStripWidth = kVerticalTitleWidth;

// Rack layout
constexpr int32_t kSlotX0 = 13;
constexpr int32_t kSlotW = 27;
constexpr int32_t kSlotGap = 2;
constexpr int32_t kSlotH = 19;
constexpr int32_t kRowGap = 4; // 5 + 19 + 4 + 19 = 47: fills the visible area
constexpr int32_t kSlotY0 = OLED_MAIN_TOPMOST_PIXEL;

Rack* currentRack() {
	InstrumentClip* clip = currentRackClip();
	return clip != nullptr ? &clip->noteFxRack : nullptr;
}

void restoreButtonLeds() {
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

} // namespace

InstrumentClip* currentRackClip() {
	Clip* clip = getCurrentClip();
	if (clip == nullptr || clip->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	return static_cast<InstrumentClip*>(clip);
}

void applyRack(InstrumentClip* clip) {
	if (clip == nullptr) {
		return;
	}
	uiNeedsRendering(getRootUI(), 0xFFFFFFFF, 0); // the clip's grid shows what some modules play (EUCL)
	const Rack& rack = clip->noteFxRack;
	SequenceDirection wanted =
	    rack.active(Type::DIR) ? static_cast<SequenceDirection>(rack.dirMode) : SequenceDirection::FORWARD;
	if (clip->sequenceDirectionMode != wanted) {
		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStackWithTimelineCounter* modelStack =
		    setupModelStackWithTimelineCounter(modelStackMemory, currentSong, clip);
		clip->setSequenceDirectionMode(modelStack, wanted);
	}
}

// ---- RackScreen

bool RackScreen::isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const {
	return currentRackClip() != nullptr;
}

void RackScreen::beginSession(MenuItem* navigatedBackwardFrom) {
	indicator_leds::setLedState(IndicatorLED::LOAD, true);
	indicator_leds::setLedState(IndicatorLED::SYNC_SCALING, true);
	indicator_leds::setLedState(IndicatorLED::TRIPLETS, true);
}

void RackScreen::endSession() {
	restoreButtonLeds();
}

ActionResult RackScreen::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;
	if (b == LOAD) {
		// LOAD does what clicking SELECT does
		if (on) {
			if (inCardRoutine) {
				return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
			}
			Buttons::selectButtonPressUsedUp = false;
			return soundEditor.buttonAction(SELECT_ENC, false, inCardRoutine);
		}
		return ActionResult::DEALT_WITH;
	}
	return ActionResult::NOT_DEALT_WITH;
}

void RackScreen::refresh() {
	if (display->haveOLED()) {
		renderUIsForOled();
	}
	else {
		drawName();
	}
}

void RackScreen::drawTitleStrip() {
	drawVerticalTitle("NOTE FX");
}

void RackScreen::drawList(const char* title, const char* const* items, int32_t count, int32_t selected) {
	deluge::hid::display::oled_canvas::Canvas& image = deluge::hid::display::OLED::main;
	image.clear();
	drawTitleStrip();

	constexpr int32_t x = kStripWidth + 3;
	const int32_t top = OLED_MAIN_TOPMOST_PIXEL;
	image.drawString(title, x, top, kTextSmallSpacingX, kTextSmallSizeY);
	image.drawHorizontalLine(top + 6, x, OLED_MAIN_WIDTH_PIXELS - 1);

	constexpr int32_t kVisible = 4;
	constexpr int32_t kRowSpacing = 8; // 4 rows of 7 px text fit the 43 visible pixels
	int32_t first = 0;
	if (selected >= kVisible) {
		first = selected - kVisible + 1;
	}
	for (int32_t i = 0; i < kVisible && first + i < count; i++) {
		int32_t y = top + 9 + i * kRowSpacing;
		image.drawString(items[first + i], x + 2, y, kTextSpacingX, kTextSizeYUpdated);
		if (first + i == selected) {
			image.invertArea(x, OLED_MAIN_WIDTH_PIXELS - x - 3, y - 1, y + kTextSizeYUpdated);
		}
	}
	if (count > kVisible) { // scroll bar
		int32_t barTop = top + 8;
		int32_t barH = kVisible * kRowSpacing;
		image.drawVerticalLine(OLED_MAIN_WIDTH_PIXELS - 1, barTop + barH * first / count,
		                       barTop + barH * (first + kVisible) / count);
	}
	deluge::hid::display::OLED::markChanged();
}

// ---- RackMenu

void RackMenu::beginSession(MenuItem* navigatedBackwardFrom) {
	RackScreen::beginSession(navigatedBackwardFrom);
	skipListOnReturn = false;
	if (navigatedBackwardFrom == nullptr) {
		// entering from the SOUND menu (not coming back from one of our lists)
		cursor = 0;
	}
	if (!display->haveOLED()) {
		drawName();
	}
}

void RackMenu::renderOLED() {
	deluge::hid::display::oled_canvas::Canvas& image = deluge::hid::display::OLED::main;
	image.clear();
	drawTitleStrip();

	Rack* rack = currentRack();
	if (rack == nullptr) {
		deluge::hid::display::OLED::markChanged();
		return;
	}

	for (int32_t i = 0; i < kNumSlots; i++) {
		const int32_t x = kSlotX0 + (i % kSlotsPerRow) * (kSlotW + kSlotGap);
		const int32_t y = kSlotY0 + (i / kSlotsPerRow) * (kSlotH + kRowGap);
		const int32_t x1 = x + kSlotW - 1;
		const int32_t y1 = y + kSlotH - 1;
		const deluge::note_fx::Slot& slot = rack->slots[i];

		if (slot.empty() || slot.on) {
			image.drawRectangle(x, y, x1, y1);
		}
		else { // switched off: dotted frame
			for (int32_t px = x; px <= x1; px += 2) {
				image.drawPixel(px, y);
				image.drawPixel(px, y1);
			}
			for (int32_t py = y; py <= y1; py += 2) {
				image.drawPixel(x, py);
				image.drawPixel(x1, py);
			}
		}

		if (slot.empty()) {
			const int32_t cx = x + kSlotW / 2;
			const int32_t cy = y + kSlotH / 2;
			image.drawHorizontalLine(cy, cx - 3, cx + 3);
			image.drawVerticalLine(cx, cy - 3, cy + 3);
		}
		else {
			const char* name = shortName(slot.type);
			const int32_t nameW = image.getStringWidthInPixels(name, kTextSmallSizeY);
			if (slot.on) {
				image.drawString(name, x + (kSlotW - nameW) / 2, y + (kSlotH - kTextSmallSizeY) / 2, kTextSmallSpacingX,
				                 kTextSmallSizeY);
			}
			else {
				image.drawString(name, x + (kSlotW - nameW) / 2, y + 3, kTextSmallSpacingX, kTextSmallSizeY);
				const int32_t offW = image.getStringWidthInPixels("OFF", kTextSmallSizeY);
				image.drawString("OFF", x + (kSlotW - offW) / 2, y + 11, kTextSmallSpacingX, kTextSmallSizeY);
			}
		}

		if (i == cursor) {
			image.invertArea(x, kSlotW, y, y1);
		}
	}
	deluge::hid::display::OLED::markChanged();
}

void RackMenu::drawName() {
	Rack* rack = currentRack();
	char text[5] = "S1 -";
	text[1] = static_cast<char>('1' + cursor);
	if (rack != nullptr && !rack->slots[cursor].empty()) {
		display->setText(shortName(rack->slots[cursor].type), false, rack->slots[cursor].on ? 255 : 3);
		return;
	}
	display->setText(text);
}

void RackMenu::selectEncoderAction(int32_t offset) {
	cursor = std::clamp<int32_t>(cursor + offset, 0, kNumSlots - 1);
	refresh();
}

ActionResult RackMenu::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;
	if (b == SYNC_SCALING || b == TRIPLETS) {
		if (on) {
			const int32_t row = (b == SYNC_SCALING) ? 0 : 1;
			cursor = row * kSlotsPerRow + (cursor % kSlotsPerRow);
			refresh();
		}
		return ActionResult::DEALT_WITH;
	}
	return RackScreen::buttonAction(b, on, inCardRoutine);
}

MenuItem* RackMenu::selectButtonPress() {
	Rack* rack = currentRack();
	if (rack == nullptr) {
		return NO_NAVIGATION;
	}
	if (rack->slots[cursor].empty()) {
		Type list[static_cast<int32_t>(Type::NUM_TYPES)];
		if (rack->available(list, static_cast<int32_t>(Type::NUM_TYPES)) == 0) {
			display->displayPopup("NO FX");
			return NO_NAVIGATION;
		}
		return &noteFxAddList;
	}
	return &noteFxActionList;
}

// ---- AddList

void AddList::beginSession(MenuItem* navigatedBackwardFrom) {
	RackScreen::beginSession(navigatedBackwardFrom);
	Rack* rack = currentRack();
	numChoices_ = rack != nullptr ? rack->available(choices_, static_cast<int32_t>(Type::NUM_TYPES)) : 0;
	selected_ = 0;
	if (!display->haveOLED()) {
		drawName();
	}
}

void AddList::renderOLED() {
	const char* names[static_cast<int32_t>(Type::NUM_TYPES)];
	for (int32_t i = 0; i < numChoices_; i++) {
		names[i] = longName(choices_[i]);
	}
	char title[24];
	snprintf(title, sizeof(title), "SLOT %d - ADD", static_cast<int>(cursor + 1));
	drawList(title, names, numChoices_, selected_);
}

void AddList::drawName() {
	display->setText(numChoices_ > 0 ? shortName(choices_[selected_]) : "----");
}

void AddList::selectEncoderAction(int32_t offset) {
	if (numChoices_ > 0) {
		selected_ = std::clamp<int32_t>(selected_ + offset, 0, numChoices_ - 1);
	}
	refresh();
}

MenuItem* AddList::selectButtonPress() {
	Rack* rack = currentRack();
	if (rack != nullptr && numChoices_ > 0 && rack->add(cursor, choices_[selected_])) {
		applyRack(currentRackClip());
		// A module just added opens on its EDIT page; BACK from there lands on the rack.
		if (MenuItem* page = editPageFor(choices_[selected_])) {
			skipListOnReturn = true;
			return page;
		}
	}
	return nullptr; // back to the rack
}

MenuPermission AddList::checkPermissionToBeginSession(ModControllableAudio* modControllable, int32_t whichThing,
                                                      ::MultiRange** currentRange) {
	return refuseIfSkipping();
}

// ---- ActionList

void ActionList::buildActions() {
	numActions_ = 0;
	actions_[numActions_++] = Action::EDIT;
	actions_[numActions_++] = Action::TOGGLE;
	if (cursor < kNumSlots - 1) {
		actions_[numActions_++] = Action::MOVE_RIGHT;
	}
	if (cursor > 0) {
		actions_[numActions_++] = Action::MOVE_LEFT;
	}
	actions_[numActions_++] = Action::DEL;
}

void ActionList::beginSession(MenuItem* navigatedBackwardFrom) {
	RackScreen::beginSession(navigatedBackwardFrom);
	buildActions();
	if (navigatedBackwardFrom == nullptr || selected_ >= numActions_) {
		selected_ = 0;
	}
	if (!display->haveOLED()) {
		drawName();
	}
}

void ActionList::renderOLED() {
	Rack* rack = currentRack();
	if (rack == nullptr || rack->slots[cursor].empty()) {
		return;
	}
	const bool on = rack->slots[cursor].on;
	const char* names[5];
	for (int32_t i = 0; i < numActions_; i++) {
		switch (actions_[i]) {
		case Action::EDIT:
			names[i] = "EDIT";
			break;
		case Action::TOGGLE:
			names[i] = on ? "OFF" : "ON";
			break;
		case Action::MOVE_RIGHT:
			names[i] = "MOVE ->";
			break;
		case Action::MOVE_LEFT:
			names[i] = "<- MOVE";
			break;
		case Action::DEL:
			names[i] = "DEL";
			break;
		}
	}
	char title[24];
	snprintf(title, sizeof(title), "SLOT %d - %s", static_cast<int>(cursor + 1), shortName(rack->slots[cursor].type));
	drawList(title, names, numActions_, selected_);
}

MenuPermission ActionList::checkPermissionToBeginSession(ModControllableAudio* modControllable, int32_t whichThing,
                                                         ::MultiRange** currentRange) {
	return refuseIfSkipping(); // coming back from a module's EDIT page: BACK goes straight to the rack
}

void ActionList::drawName() {
	static constexpr const char* kShort[] = {"EDIT", "ONOF", "MOVR", "MOVL", "DEL"};
	display->setText(kShort[static_cast<int32_t>(actions_[selected_])]);
}

void ActionList::selectEncoderAction(int32_t offset) {
	selected_ = std::clamp<int32_t>(selected_ + offset, 0, numActions_ - 1);
	refresh();
}

MenuItem* ActionList::selectButtonPress() {
	Rack* rack = currentRack();
	if (rack == nullptr || rack->slots[cursor].empty()) {
		return nullptr;
	}
	switch (actions_[selected_]) {
	case Action::EDIT:
		if (MenuItem* page = editPageFor(rack->slots[cursor].type)) {
			skipListOnReturn = true;
			return page;
		}
		return NO_NAVIGATION;
	case Action::TOGGLE:
		rack->toggle(cursor);
		break;
	case Action::MOVE_RIGHT:
	case Action::MOVE_LEFT: {
		int32_t to = rack->move(cursor, actions_[selected_] == Action::MOVE_RIGHT ? 1 : -1);
		if (to >= 0) {
			cursor = to; // the cursor follows the module
		}
		break;
	}
	case Action::DEL:
		rack->remove(cursor);
		break;
	}
	applyRack(currentRackClip());
	return nullptr; // back to the rack
}

// ---- DirEdit

void DirEdit::readCurrentValue() {
	InstrumentClip* clip = currentRackClip();
	setValue(clip != nullptr ? clip->noteFxRack.dirMode : 0);
}

void DirEdit::writeCurrentValue() {
	InstrumentClip* clip = currentRackClip();
	if (clip != nullptr) {
		clip->noteFxRack.dirMode = static_cast<uint8_t>(getValue());
		applyRack(clip);
	}
}

deluge::vector<std::string_view> DirEdit::getOptions(OptType optType) {
	(void)optType;
	return {
	    l10n::getView(l10n::String::STRING_FOR_FORWARD),
	    l10n::getView(l10n::String::STRING_FOR_REVERSED),
	    l10n::getView(l10n::String::STRING_FOR_PING_PONG),
	};
}

} // namespace deluge::gui::menu_item::note_fx
