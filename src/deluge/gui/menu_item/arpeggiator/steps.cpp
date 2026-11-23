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

#include "gui/menu_item/arpeggiator/steps.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "modulation/arpeggiator.h"
#include "processing/sound/sound.h"
#include <algorithm>
#include <cstdio>

namespace deluge::gui::menu_item::arpeggiator {

using deluge::modulation::ArpSteps;

namespace {
ArpScreen* openScreen = nullptr;

constexpr int32_t kPlayheadRefreshMs = 25;
constexpr int32_t kZeroRow = 3;  // row 4 from the top: the step's note when not moved
constexpr int32_t kOnOffRow = 7; // row 8 from the top: ON / OFF
constexpr int32_t kViewMax = ArpSteps::kMaxOffset - kZeroRow;

const RGB kStepNote{255, 160, 40};    // a step's note
const RGB kStepNoteOff{110, 70, 30};  // the note of an OFF step
const RGB kRow4Dim{70, 55, 30};       // row 4 when the step's note is moved up / down
const RGB kStepOn{240, 240, 240};     // row 8: ON
const RGB kStepOff{220, 30, 30};      // row 8: OFF
const RGB kStepPlaying{40, 200, 255}; // row 8: the step playing
const RGB kOutOfView{60, 35, 10};     // a step's note above / below the rows shown, on the edge row

ArpeggiatorSettings* settings() {
	return soundEditor.currentArpSettings;
}

int32_t playingStep() {
	Sound* sound = soundEditor.currentSound;
	ArpeggiatorBase* arp = sound != nullptr ? sound->getArp() : nullptr;
	return arp != nullptr ? arp->playingStep() : -1;
}
} // namespace

// OCTA ----------------------------------------------------------------------------------------------------------------

void OctaveSpan::readCurrentValue() {
	setValue(deluge::modulation::OctaveSpan::fromSettings(settings()->numOctaves, settings()->octavesBelow));
}

void OctaveSpan::writeCurrentValue() {
	deluge::modulation::OctaveSpan::toSettings(getValue(), settings()->numOctaves, settings()->octavesBelow);
}

int32_t OctaveSpan::getMinValue() const {
	return deluge::modulation::OctaveSpan::kMin;
}

int32_t OctaveSpan::getMaxValue() const {
	return deluge::modulation::OctaveSpan::kMax;
}

// LEN -----------------------------------------------------------------------------------------------------------------

void StepCount::readCurrentValue() {
	setValue(settings()->steps.len);
}

void StepCount::writeCurrentValue() {
	settings()->steps.setLen(getValue());
	if (openArpScreen() != nullptr) {
		uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0); // the steps beyond LEN go dark on the grid
	}
}

// RHYT ----------------------------------------------------------------------------------------------------------------

void StepsCell::renderInHorizontalMenu(const SlotPosition& slot) {
	// the steps in two rows of eight: ON = a 2x2 square, OFF = a dot, beyond LEN = nothing
	using deluge::hid::display::OLED;
	auto& image = OLED::main;
	const ArpSteps& steps = settings()->steps;
	constexpr int32_t kPitch = 3;
	const int32_t x0 = slot.start_x + (slot.width - (8 * kPitch - 1)) / 2;
	for (int32_t s = 0; s < steps.len; s++) {
		const int32_t x = x0 + (s % 8) * kPitch;
		const int32_t y = slot.start_y + 3 + (s / 8) * 5;
		image.drawPixel(x, y + 1);
		if (steps.isOn(s)) {
			image.drawPixel(x + 1, y + 1);
			image.drawPixel(x, y);
			image.drawPixel(x + 1, y);
		}
	}
}

// The screen ----------------------------------------------------------------------------------------------------------

ArpScreen* openArpScreen() {
	if (openScreen == nullptr || getCurrentUI() != &soundEditor || soundEditor.getCurrentMenuItem() != openScreen) {
		return nullptr;
	}
	return openScreen;
}

void ArpScreen::beginSession(MenuItem* navigatedBackwardFrom) {
	TwoRowMenu::beginSession(navigatedBackwardFrom);
	openScreen = this;
	heldStep_ = -1;
	view_ = 0;
	showSteps();
	startOrStopPlayhead();
}

void ArpScreen::endSession() {
	TwoRowMenu::endSession();
	if (timerRunning_) {
		uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
		timerRunning_ = false;
	}
	heldStep_ = -1;
	openScreen = nullptr;
	uiNeedsRendering(getRootUI(), 0xFFFFFFFF, 0); // the grid goes back to the clip
}

void ArpScreen::focusChanged() {
	heldStep_ = -1;
	showSteps();
	startOrStopPlayhead();
}

bool ArpScreen::editingSteps() const {
	return getCurrentItem() == &stepsCell_ && getRootUI() == &instrumentClipView && settings() != nullptr;
}

void ArpScreen::showSteps() const {
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
}

void ArpScreen::startOrStopPlayhead() {
	if (editingSteps() && !timerRunning_) {
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kPlayheadRefreshMs);
		timerRunning_ = true;
	}
	else if (!editingSteps() && timerRunning_) {
		uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
		timerRunning_ = false;
	}
}

ActionResult ArpScreen::timerCallback() {
	if (!editingSteps()) {
		timerRunning_ = false;
		return ActionResult::DEALT_WITH;
	}
	if (playingStep() != drawnStep_) {
		uiNeedsRendering(&soundEditor, 1 << 0, 0); // row 8 (the bottom one) shows where the arp is
	}
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kPlayheadRefreshMs);
	return ActionResult::DEALT_WITH;
}

bool ArpScreen::padAction(int32_t x, int32_t y, bool on) {
	if (!editingSteps() || x >= kDisplayWidth) {
		return false;
	}
	ArpSteps& steps = settings()->steps;
	if (!on) {
		if (x == heldStep_) {
			heldStep_ = -1;
		}
		return true;
	}
	if (x >= steps.len) {
		return true; // beyond LEN
	}
	const int32_t row = kDisplayHeight - 1 - y; // the Deluge counts pad rows from the bottom
	char text[20];
	if (row == kOnOffRow) {
		steps.toggle(x);
		snprintf(text, sizeof(text), "STEP %d %s", static_cast<int>(x + 1), steps.isOn(x) ? "ON" : "OFF");
	}
	else {
		steps.setOffset(x, view_ + kZeroRow - row);
		heldStep_ = x;
		snprintf(text, sizeof(text), "STEP %d %+d", static_cast<int>(x + 1), static_cast<int>(steps.offset[x]));
	}
	display->displayPopup(text);
	soundEditor.markInstrumentAsEdited();
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
	renderUIsForOled(); // the RHYT cell shows the steps
	return true;
}

bool ArpScreen::stepEncoderAction(int32_t offset) {
	if (!editingSteps() || offset == 0) {
		return false;
	}
	char text[20];
	if (heldStep_ < 0) {
		// no step held: scroll the rows shown
		view_ = std::clamp<int32_t>(view_ + (offset > 0 ? 1 : -1), -kViewMax, kViewMax);
		snprintf(text, sizeof(text), "ROW 4 = %+d", static_cast<int>(view_));
	}
	else {
		ArpSteps& steps = settings()->steps;
		steps.setOffset(heldStep_, steps.offset[heldStep_] + (offset > 0 ? 1 : -1));
		// the rows shown follow the note
		const int32_t note = steps.offset[heldStep_];
		if (note > view_ + kZeroRow) {
			view_ = note - kZeroRow;
		}
		else if (note < view_ + kZeroRow - (kOnOffRow - 1)) {
			view_ = note - kZeroRow + (kOnOffRow - 1);
		}
		snprintf(text, sizeof(text), "STEP %d %+d", static_cast<int>(heldStep_ + 1),
		         static_cast<int>(steps.offset[heldStep_]));
		soundEditor.markInstrumentAsEdited();
	}
	display->displayPopup(text);
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
	return true;
}

void ArpScreen::renderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                           uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	const ArpSteps& steps = settings()->steps;
	const int32_t playing = playingStep();
	drawnStep_ = playing;
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (!(whichRows & (1 << y))) {
			continue;
		}
		const int32_t row = kDisplayHeight - 1 - y;
		for (int32_t x = 0; x < kDisplayWidth + kSideBarWidth; x++) {
			RGB colour = colours::black;
			if (x < steps.len && x < ArpSteps::kMaxSteps) {
				const int32_t noteRow = kZeroRow - (steps.offset[x] - view_);
				const int32_t zeroRow = kZeroRow + view_; // where offset 0 is (if shown)
				if (row == kOnOffRow) {
					colour = x == playing ? kStepPlaying : (steps.isOn(x) ? kStepOn : kStepOff);
				}
				else if (row == noteRow) {
					colour = steps.isOn(x) ? kStepNote : kStepNoteOff;
				}
				else if ((noteRow < 0 && row == 0) || (noteRow >= kOnOffRow && row == kOnOffRow - 1)) {
					colour = kOutOfView;
				}
				else if (row == zeroRow) {
					colour = kRow4Dim;
				}
			}
			image[y][x] = colour;
			if (occupancyMask != nullptr) {
				occupancyMask[y][x] = colour == colours::black ? 0 : 64;
			}
		}
	}
}

} // namespace deluge::gui::menu_item::arpeggiator
