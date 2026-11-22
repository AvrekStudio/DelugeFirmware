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

#include "gui/menu_item/note_fx/module_page.h"
#include "definitions_cxx.hpp"
#include "gui/menu_item/vertical_title.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/button.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/led/indicator_leds.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/instrument.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "processing/sound/sound_instrument.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace deluge::gui::menu_item::note_fx {

using namespace deluge::note_fx;

ModulePage noteFxArpPage{l10n::String::STRING_FOR_NOTE_FX_RACK, Type::ARP};
ModulePage noteFxEuclPage{l10n::String::STRING_FOR_NOTE_FX_RACK, Type::EUCL};
ModulePage noteFxMonoPage{l10n::String::STRING_FOR_NOTE_FX_RACK, Type::MONO};

namespace {
constexpr int32_t kColumns = 4;
constexpr int32_t kCellX0 = kVerticalTitleWidth + 1;
constexpr int32_t kCellW = (OLED_MAIN_WIDTH_PIXELS - kCellX0) / kColumns;
constexpr int32_t kRowH = 21;

// The grid note held for p-locking (while a module page is open)
struct HeldNote {
	bool active = false;
	int32_t pos = 0;
	int32_t note = 0;
	int32_t x = 0;
	int32_t y = 0;
} held;

// ARP step editor: the step (column) held, -1 = none
int32_t arpHeldStep = -1;
// the step playing when the grid was last drawn
int32_t arpDrawnStep = -1;
constexpr int32_t kPlayheadRefreshMs = 25;

// ARP step editor colours
const RGB kStepNote{255, 160, 40};    // a step's note
const RGB kStepNoteOff{110, 70, 30};  // the note of an OFF step
const RGB kStepRow4Dim{70, 55, 30};   // row 4 when the step's note is moved up / down
const RGB kStepOn{240, 240, 240};     // row 8: ON
const RGB kStepOff{220, 30, 30};      // row 8: OFF
const RGB kStepPlaying{40, 200, 255}; // row 8: the step playing
constexpr int32_t kZeroRow = 3;       // row 4 (from the top): no offset
constexpr int32_t kOnOffRow = 7;      // row 8

Instrument* track() {
	return getCurrentInstrument();
}
} // namespace

int32_t ModulePage::cellParam(int32_t cell) const {
	switch (type_) {
	case Type::ARP: {
		static constexpr int8_t kCells[8] = {ARP_STYLE,  ARP_RATE,  ARP_GATE,   ARP_HUMAN,
		                                     ARP_OCTAVE, ARP_CHORD, ARP_RETRIG, ARP_LEN};
		int32_t p = kCells[cell];
		if (p == ARP_RATE) {
			if (showSync_) {
				return ARP_SYNC;
			}
			bool locked;
			if (value(ARP_SYNC, &locked) == SYNC_OFF) {
				return ARP_FREE_RATE;
			}
		}
		return p;
	}
	case Type::EUCL: {
		static constexpr int8_t kCells[8] = {EUCL_NOTE,   EUCL_RATE, EUCL_STEPS, EUCL_PULSES,
		                                     EUCL_ROTATE, EUCL_GATE, -1,         -1};
		return kCells[cell];
	}
	case Type::MONO: {
		static constexpr int8_t kCells[8] = {MONO_MODE, MONO_LEGATO, MONO_RETRIG, -1, -1, -1, -1, -1};
		return kCells[cell];
	}
	default:
		return -1;
	}
}

int32_t ModulePage::value(int32_t param, bool* locked) const {
	*locked = false;
	Instrument* t = track();
	if (t == nullptr) {
		return 0;
	}
	InstrumentClip* clip = currentRackClip();
	if (held.active && clip != nullptr) {
		if (const NoteLock* l = clip->noteFxLocks.find(held.pos, held.note, type_, param)) {
			*locked = true;
			return l->value;
		}
	}
	return t->noteFxValues.get(type_, param);
}

void ModulePage::setValue(int32_t param, int32_t v) {
	Instrument* t = track();
	if (t == nullptr) {
		return;
	}
	int32_t count;
	const ParamSpec* spec = paramSpecs(type_, &count);
	v = std::clamp<int32_t>(v, spec[param].min, spec[param].max);
	InstrumentClip* clip = currentRackClip();
	if (held.active && clip != nullptr) {
		if (!clip->noteFxLocks.set(held.pos, held.note, type_, param, v)) {
			display->displayPopup("FULL");
		}
		return;
	}
	t->noteFxValues.set(type_, param, v);
}

void ModulePage::lightButtons() const {
	// SYNTH / KIT / MIDI / CV pick the column: lit while browsing the parameters
	indicator_leds::setLedState(IndicatorLED::SYNTH, true);
	indicator_leds::setLedState(IndicatorLED::KIT, true);
	indicator_leds::setLedState(IndicatorLED::MIDI, true);
	indicator_leds::setLedState(IndicatorLED::CV, true);
}

void ModulePage::endSession() {
	RackScreen::endSession();
	indicator_leds::setLedState(IndicatorLED::SYNTH, false);
	indicator_leds::setLedState(IndicatorLED::KIT, false);
	indicator_leds::setLedState(IndicatorLED::MIDI, false);
	indicator_leds::setLedState(IndicatorLED::CV, false);
	arpHeldStep = -1;
	if (type_ == Type::ARP) {
		uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
		uiNeedsRendering(getRootUI(), 0xFFFFFFFF, 0); // the grid goes back to the clip
	}
}

ActionResult ModulePage::timerCallback() {
	if (type_ != Type::ARP) {
		return ActionResult::DEALT_WITH;
	}
	Instrument* t = track();
	int32_t step = -1;
	if (t != nullptr && t->type == OutputType::SYNTH) {
		step = static_cast<SoundInstrument*>(t)->noteFxRunner.arpStep();
	}
	if (step != arpDrawnStep) {
		uiNeedsRendering(&soundEditor, 1 << 0, 0); // row 8 (the bottom one) shows where the ARP is
	}
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kPlayheadRefreshMs);
	return ActionResult::DEALT_WITH;
}

void ModulePage::beginSession(MenuItem* navigatedBackwardFrom) {
	RackScreen::beginSession(navigatedBackwardFrom);
	lightButtons();
	arpHeldStep = -1;
	if (type_ == Type::ARP) {
		uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0); // the grid shows the ARP's steps
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kPlayheadRefreshMs);
	}
	held.active = false;
	showSync_ = false;
	if (cellParam(selected_) < 0) {
		selected_ = 0;
	}
	if (!display->haveOLED()) {
		drawName();
	}
}

void ModulePage::renderOLED() {
	using deluge::hid::display::OLED;
	auto& image = OLED::main;
	image.clear();

	char title[12];
	if (held.active) {
		char name[kNoteCodeStringBufferSize];
		noteCodeToString(held.note, name);
		snprintf(title, sizeof(title), "%s LCK", name);
	}
	else {
		snprintf(title, sizeof(title), "%s", shortName(type_));
	}
	drawVerticalTitle(title);

	int32_t count;
	const ParamSpec* spec = paramSpecs(type_, &count);
	for (int32_t cell = 0; cell < 8; cell++) {
		const int32_t p = cellParam(cell);
		if (p < 0) {
			continue;
		}
		const int32_t x = kCellX0 + (cell % kColumns) * kCellW;
		const int32_t y = OLED_MAIN_TOPMOST_PIXEL + (cell / kColumns) * (kRowH + 1);
		bool locked;
		const int32_t v = value(p, &locked);
		char buf[8];
		formatParam(type_, p, v, buf, sizeof(buf));
		image.drawStringCentered(buf, x, y + 4, kTextSpacingX, kTextSizeYUpdated, kCellW - 1);
		image.drawStringCentered(spec[p].label, x, y + 15, kTextSmallSpacingX, kTextSmallSizeY, kCellW - 1);
		if (locked) { // p-locked on the held note: a small square in the corner
			image.invertArea(x + kCellW - 5, 3, y + 1, y + 3);
		}
		if (cell == selected_) {
			image.invertArea(x, kCellW - 1, y, y + kRowH - 1);
		}
	}
	OLED::markChanged();
}

void ModulePage::drawName() {
	const int32_t p = cellParam(selected_);
	int32_t count;
	const ParamSpec* spec = paramSpecs(type_, &count);
	display->setText(p >= 0 ? spec[p].label : "----");
}

void ModulePage::selectEncoderAction(int32_t offset) {
	const int32_t p = cellParam(selected_);
	if (p < 0) {
		return;
	}
	bool locked;
	setValue(p, value(p, &locked) + offset);
	refresh();
	if (type_ == Type::ARP) {
		uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0); // LEN changes the steps shown on the grid
	}
}

MenuItem* ModulePage::selectButtonPress() {
	const int32_t p = cellParam(selected_);
	InstrumentClip* clip = currentRackClip();
	bool locked = false;
	if (p >= 0) {
		value(p, &locked);
	}
	if (held.active && locked && clip != nullptr) {
		clip->noteFxLocks.clear(held.pos, held.note, type_, p); // click on a locked value: unlock it
	}
	else if (type_ == Type::ARP && (selected_ == 1)) {
		showSync_ = !showSync_; // RATE <-> SYNC
	}
	refresh();
	return NO_NAVIGATION;
}

ActionResult ModulePage::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;
	int32_t target = -1;
	if (b == SYNTH || b == KIT || b == MIDI || b == CV) {
		const int32_t col = b == SYNTH ? 0 : (b == KIT ? 1 : (b == MIDI ? 2 : 3));
		target = (selected_ / kColumns) * kColumns + col;
	}
	else if (b == SYNC_SCALING || b == TRIPLETS) {
		const int32_t rowStart = b == SYNC_SCALING ? 0 : kColumns;
		target = rowStart + selected_ % kColumns;
		// that cell is empty: the row's first used one
		for (int32_t col = 0; cellParam(target) < 0 && col < kColumns; col++) {
			target = rowStart + col;
		}
	}
	if (target >= 0) {
		if (on && cellParam(target) >= 0) {
			if (target != 1) {
				showSync_ = false;
			}
			selected_ = target;
			refresh();
		}
		return ActionResult::DEALT_WITH;
	}
	return RackScreen::buttonAction(b, on, inCardRoutine);
}

bool ModulePage::arpPadAction(int32_t x, int32_t y, bool on) {
	Instrument* t = track();
	if (t == nullptr || x >= kArpSteps) {
		return true;
	}
	ArpSteps& steps = t->noteFxArpSteps;
	const int32_t len = t->noteFxValues.get(Type::ARP, ARP_LEN);
	if (!on) {
		if (x == arpHeldStep) {
			arpHeldStep = -1;
		}
		return true;
	}
	if (x >= len) {
		return true; // beyond LEN
	}
	const int32_t row = kDisplayHeight - 1 - y; // the Deluge counts pad rows from the bottom
	char text[16];
	if (row == kOnOffRow) {
		steps.toggle(x);
		snprintf(text, sizeof(text), "STEP %d %s", static_cast<int>(x + 1), steps.isOn(x) ? "ON" : "OFF");
	}
	else {
		steps.setOffset(x, kZeroRow - row);
		arpHeldStep = x;
		snprintf(text, sizeof(text), "STEP %d %+d", static_cast<int>(x + 1), static_cast<int>(steps.offset[x]));
	}
	display->displayPopup(text);
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
	return true;
}

bool ModulePage::stepEncoderAction(int32_t offset) {
	Instrument* t = track();
	if (type_ != Type::ARP || arpHeldStep < 0 || t == nullptr) {
		return false;
	}
	ArpSteps& steps = t->noteFxArpSteps;
	steps.setOffset(arpHeldStep, steps.offset[arpHeldStep] + (offset > 0 ? 1 : -1));
	char text[16];
	snprintf(text, sizeof(text), "STEP %d %+d", static_cast<int>(arpHeldStep + 1),
	         static_cast<int>(steps.offset[arpHeldStep]));
	display->displayPopup(text);
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
	return true;
}

bool ModulePage::renderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                            uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (type_ != Type::ARP) {
		return false;
	}
	if (image == nullptr) {
		return true;
	}
	Instrument* t = track();
	const ArpSteps* steps = t != nullptr ? &t->noteFxArpSteps : nullptr;
	const int32_t len = t != nullptr ? t->noteFxValues.get(Type::ARP, ARP_LEN) : 0;
	int32_t playing = -1;
	if (t != nullptr && t->type == OutputType::SYNTH) {
		playing = static_cast<SoundInstrument*>(t)->noteFxRunner.arpStep();
	}
	arpDrawnStep = playing;
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (!(whichRows & (1 << y))) {
			continue;
		}
		const int32_t row = kDisplayHeight - 1 - y;
		for (int32_t x = 0; x < kDisplayWidth + kSideBarWidth; x++) {
			RGB colour = colours::black;
			if (steps != nullptr && x < len && x < kArpSteps) {
				const int32_t offset = steps->offset[x];
				const int32_t noteRow = std::clamp<int32_t>(kZeroRow - offset, 0, kOnOffRow - 1);
				if (row == kOnOffRow) {
					colour = x == playing ? kStepPlaying : (steps->isOn(x) ? kStepOn : kStepOff);
				}
				else if (row == noteRow) {
					colour = steps->isOn(x) ? kStepNote : kStepNoteOff;
				}
				else if (row == kZeroRow) {
					colour = kStepRow4Dim;
				}
			}
			image[y][x] = colour;
			if (occupancyMask != nullptr) {
				occupancyMask[y][x] = colour == colours::black ? 0 : 64;
			}
		}
	}
	return true;
}

bool ModulePage::padAction(int32_t x, int32_t y, bool on) {
	if (type_ == Type::ARP) {
		return arpPadAction(x, y, on); // no p-lock for the ARP: the grid is its step editor
	}
	if (!on) {
		if (held.active && held.x == x && held.y == y) {
			held.active = false;
			refresh();
		}
		return true;
	}
	InstrumentClip* clip = currentRackClip();
	if (clip == nullptr) {
		return true;
	}
	const int32_t yNote = clip->getYNoteFromYDisplay(y, currentSong);
	NoteRow* row = clip->getNoteRowForYNote(yNote);
	if (row != nullptr) {
		const int32_t effectiveLength = row->loopLengthIfIndependent ? row->loopLengthIfIndependent : clip->loopLength;
		const int32_t squareStart = instrumentClipView.getPosFromSquare(x);
		const int32_t squareWidth = static_cast<int32_t>(instrumentClipView.getSquareWidth(x, effectiveLength));
		const int32_t i = row->notes.search(squareStart, GREATER_OR_EQUAL);
		if (i < row->notes.getNumElements()) {
			Note* n = row->notes.getElement(i);
			if (n->pos < squareStart + squareWidth) {
				held = {true, n->pos, yNote, x, y};
				refresh();
				return true;
			}
		}
	}
	display->displayPopup("NO NOTE");
	return true;
}

namespace {
ModulePage* openModulePage() {
	MenuItem* item = soundEditor.getCurrentMenuItem();
	for (ModulePage* page : {&noteFxArpPage, &noteFxEuclPage, &noteFxMonoPage}) {
		if (item == page) {
			return page;
		}
	}
	return nullptr;
}
} // namespace

void renderEuclidNotes(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth]) {
	InstrumentClip* clip = getCurrentInstrumentClip();
	if (clip == nullptr || clip->output == nullptr || clip->output->type != OutputType::SYNTH
	    || !clip->noteFxRack.active(Type::EUCL)) {
		return;
	}
	auto* instrument = static_cast<Instrument*>(clip->output);
	const Values& v = instrument->noteFxValues;
	int32_t note = v.get(Type::EUCL, EUCL_NOTE);
	if (note == kEuclNoteIn) {
		return; // follows the notes played: no fixed row to show
	}
	if (note == kEuclNoteTrack) {
		note = instrument->noteDefaults.note;
	}
	int32_t yDisplay = -1;
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (clip->getYNoteFromYDisplay(y, currentSong) == note) {
			yDisplay = y;
			break;
		}
	}
	if (yDisplay < 0 || !(whichRows & (1 << yDisplay))) {
		return;
	}
	const double step = rateTicks(v.get(Type::EUCL, EUCL_RATE), currentSong->getBarLength());
	const int32_t steps = v.get(Type::EUCL, EUCL_STEPS);
	const int32_t pulses = std::min<int32_t>(v.get(Type::EUCL, EUCL_PULSES), steps);
	const int32_t rotate = v.get(Type::EUCL, EUCL_ROTATE);
	const RGB kEuclidNote{230, 20, 20};
	for (int32_t x = 0; x < kDisplayWidth; x++) {
		const int64_t start = instrumentClipView.getPosFromSquare(x);
		const int64_t end = start + instrumentClipView.getSquareWidth(x, clip->loopLength);
		if (start >= clip->loopLength) {
			break;
		}
		for (int64_t k = static_cast<int64_t>(std::ceil(start / step)); k * step < end; k++) {
			if (euclidHit(static_cast<int32_t>(k % steps), steps, pulses, rotate)) {
				image[yDisplay][x] = kEuclidNote;
				break;
			}
		}
	}
}

bool modulePageVerticalEncoderAction(int32_t offset) {
	ModulePage* page = getCurrentUI() == &soundEditor ? openModulePage() : nullptr;
	return page != nullptr && page->stepEncoderAction(offset);
}

bool modulePageRenderPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                          uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (getRootUI() != &instrumentClipView) {
		return false;
	}
	ModulePage* page = openModulePage();
	return page != nullptr && page->renderPads(whichRows, image, occupancyMask);
}

bool modulePagePadAction(int32_t x, int32_t y, bool on) {
	if (x >= kDisplayWidth || getRootUI() != &instrumentClipView) {
		return false;
	}
	MenuItem* item = soundEditor.getCurrentMenuItem();
	for (ModulePage* page : {&noteFxArpPage, &noteFxEuclPage, &noteFxMonoPage}) {
		if (item == page) {
			return page->padAction(x, y, on);
		}
	}
	return false;
}

} // namespace deluge::gui::menu_item::note_fx
