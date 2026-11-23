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

#include "gui/menu_item/chord_mode.h"
#include "gui/menu_item/vertical_title.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/song/song.h"
#include "processing/sound/sound_instrument.h"
#include "util/d_stringbuf.h"
#include <cstdio>

namespace deluge::gui::menu_item::chord_mode {

using namespace deluge::chordmode;
using deluge::hid::display::OLED;

namespace {
SoundInstrument* instrument() {
	if (getCurrentOutputType() != OutputType::SYNTH || soundEditor.currentSound == nullptr) {
		return nullptr;
	}
	return static_cast<SoundInstrument*>(soundEditor.currentSound);
}

constexpr int32_t kRefreshMs = 50;

// Keyboard: two octaves of white keys, 6 px apart
constexpr int32_t kKeysX = kVerticalTitleWidth + 2;
constexpr int32_t kKeyPitch = 6;
constexpr int32_t kKeysTop = OLED_MAIN_TOPMOST_PIXEL;
constexpr int32_t kKeysBottom = kKeysTop + 15;
constexpr int32_t kBlackBottom = kKeysTop + 9;
constexpr int32_t kNameX = kKeysX + 14 * kKeyPitch + 2;

// Settings: four columns under the keyboard
constexpr int32_t kColX = kVerticalTitleWidth + 1;
constexpr int32_t kColTop = kKeysBottom + 4;
constexpr int32_t kLabelY = kColTop + 2;
constexpr int32_t kValueY = kLabelY + 8;

constexpr int8_t kWhiteIndex[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
constexpr const char* kPitchNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

bool isBlack(int32_t semitone) {
	return kWhiteIndex[semitone % 12] < 0;
}

/// Left edge of a key (semitone 0 .. 23 from the keyboard's first C)
int32_t keyX(int32_t semitone) {
	const int32_t octave = semitone / 12;
	const int32_t pc = semitone % 12;
	if (!isBlack(pc)) {
		return kKeysX + (octave * 7 + kWhiteIndex[pc]) * kKeyPitch;
	}
	return kKeysX + (octave * 7 + kWhiteIndex[pc - 1] + 1) * kKeyPitch - 2; // across the gap
}

void drawKeyboard(deluge::hid::display::oled_canvas::Canvas& image) {
	for (int32_t k = 0; k < 14; k++) {
		image.invertArea(kKeysX + k * kKeyPitch, kKeyPitch - 1, kKeysTop, kKeysBottom);
	}
	for (int32_t s = 0; s < 24; s++) {
		if (isBlack(s)) {
			image.clearAreaExact(keyX(s), kKeysTop, keyX(s) + 2, kBlackBottom);
		}
	}
}

/// A note of the chord: a square at the foot of a white key, a dot on a black one
void drawNoteMark(deluge::hid::display::oled_canvas::Canvas& image, int32_t s, bool root) {
	const int32_t x = keyX(s);
	if (isBlack(s)) {
		image.drawPixel(x + 1, kBlackBottom - 2);
		image.drawPixel(x + 1, kBlackBottom - 3);
	}
	else {
		// a small hollow square
		image.clearAreaExact(x + 1, kKeysBottom - 3, x + 3, kKeysBottom - 1);
		image.drawPixel(x + 2, kKeysBottom - 2);
	}
	if (root) {
		// a little arrow under the keyboard
		const int32_t c = x + (isBlack(s) ? 1 : 2);
		image.drawPixel(c, kKeysBottom + 2);
		image.drawHorizontalLine(kKeysBottom + 3, c - 1, c + 1);
	}
}

/// ROOT mode: a tick at the top of each key of the song's scale
void drawScaleTick(deluge::hid::display::oled_canvas::Canvas& image, int32_t s) {
	const int32_t x = keyX(s) + (isBlack(s) ? 1 : 2);
	if (isBlack(s)) {
		image.drawPixel(x, kKeysTop + 1);
	}
	else {
		image.clearPixel(x, kKeysTop + 1);
	}
}
} // namespace

// Settings --------------------------------------------------------------------------------------------------------

Settings* Setting::settings() {
	SoundInstrument* i = instrument();
	return i != nullptr ? &i->chordSettings : nullptr;
}

void Setting::drawValue() {
	char buf[12];
	format(getValue(), buf, sizeof(buf));
	display->setText(buf, true);
}

void Setting::drawPixelsForOled() {
	char buf[12];
	format(getValue(), buf, sizeof(buf));
	OLED::main.drawStringCentred(buf, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX, kTextHugeSizeY);
}

void Mode::readCurrentValue() {
	Settings* s = settings();
	setValue(s != nullptr ? static_cast<int32_t>(s->mode) : 0);
}
void Mode::writeCurrentValue() {
	if (Settings* s = settings()) {
		s->mode = static_cast<deluge::chordmode::Mode>(getValue());
	}
}
int32_t Mode::getMaxValue() const {
	return static_cast<int32_t>(deluge::chordmode::Mode::NUM) - 1;
}
void Mode::format(int32_t value, char* out, size_t size) const {
	snprintf(out, size, "%s", kModeNames[value % kModeNames.size()]);
}

void Type::readCurrentValue() {
	Settings* s = settings();
	setValue(s != nullptr ? s->type() : 0);
}
void Type::writeCurrentValue() {
	if (Settings* s = settings()) {
		s->setType(getValue());
	}
}
int32_t Type::getMaxValue() const {
	Settings* s = settings();
	return (s != nullptr ? s->typeCount() : kNumFreeTypes) - 1;
}
void Type::format(int32_t value, char* out, size_t size) const {
	Settings* s = settings();
	const bool root = s != nullptr && s->mode == deluge::chordmode::Mode::ROOT;
	snprintf(out, size, "%s", root ? kRootTypeNames[value % kNumRootTypes] : kFreeTypeNames[value % kNumFreeTypes]);
}

void Strum::readCurrentValue() {
	Settings* s = settings();
	setValue(s != nullptr ? s->strumMs : 0);
}
void Strum::writeCurrentValue() {
	if (Settings* s = settings()) {
		s->strumMs = static_cast<uint16_t>(getValue());
	}
}
void Strum::format(int32_t value, char* out, size_t size) const {
	if (value == 0) {
		snprintf(out, size, "OFF");
	}
	else {
		snprintf(out, size, "%d", static_cast<int>(value));
	}
}

void Bass::readCurrentValue() {
	Settings* s = settings();
	setValue(s != nullptr ? s->bass : 0);
}
void Bass::writeCurrentValue() {
	if (Settings* s = settings()) {
		s->bass = static_cast<int8_t>(getValue());
	}
}
void Bass::format(int32_t value, char* out, size_t size) const {
	if (value == 0) {
		snprintf(out, size, "OFF");
	}
	else {
		snprintf(out, size, "%+d", static_cast<int>(value));
	}
}

// The screen ------------------------------------------------------------------------------------------------------

void Screen::beginSession(MenuItem* navigatedBackwardFrom) {
	TwoRowMenu::beginSession(navigatedBackwardFrom);
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kRefreshMs);
}

void Screen::endSession() {
	TwoRowMenu::endSession();
	uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
}

ActionResult Screen::timerCallback() {
	// redraw when a key is played or let go: the keyboard shows its chord
	SoundInstrument* i = instrument();
	if (i != nullptr && i->chordPlayCounter != drawnPlayCounter_) {
		renderUIsForOled();
	}
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kRefreshMs);
	return ActionResult::DEALT_WITH;
}

void Screen::clearPopup() {
	if (display->hasPopup()) {
		display->cancelPopup();
	}
}

void Screen::selectEncoderAction(int32_t offset) {
	TwoRowMenu::selectEncoderAction(offset);
	clearPopup();
	renderUIsForOled(); // MODE changes TYPE's list, any setting changes the chord shown
}

ActionResult Screen::buttonAction(hid::Button b, bool on, bool inCardRoutine) {
	const ActionResult result = TwoRowMenu::buttonAction(b, on, inCardRoutine);
	clearPopup();
	return result;
}

void Screen::renderOLED() {
	auto& image = OLED::main;
	image.clear();
	drawVerticalTitle(getTitle(), true);
	drawKeyboard(image);

	SoundInstrument* inst = instrument();
	const Settings* s = inst != nullptr ? &inst->chordSettings : nullptr;
	if (inst != nullptr) {
		drawnPlayCounter_ = inst->chordPlayCounter;
	}

	if (s != nullptr && s->mode != deluge::chordmode::Mode::OFF) {
		const Scale scale{currentSong->key.modeNotes.toBits(), currentSong->key.rootNote};
		const Chord chord = build(*s, inst->chordLastKey, scale);

		if (s->mode == deluge::chordmode::Mode::ROOT) {
			for (int32_t k = 0; k < 24; k++) {
				if (scale.contains(k)) {
					drawScaleTick(image, k);
				}
			}
		}

		// the root in the low octave, or the high one if the bass goes below it
		drawChordNotes(image, chord.notes.data(), chord.count, chord.root, s->bass < 0);
		drawChordName(image, chord.root, qualityName(chord));
	}

	// MODE TYPE STRUM BASS: label above, value below; the selected one framed in white
	const MenuItem* current = getCurrentItem();
	for (int32_t i = 0; i < static_cast<int32_t>(items.size()) && i < 4; i++) {
		auto* item = static_cast<Setting*>(items[i]);
		item->readCurrentValue();
		DEF_STACK_STRING_BUF(label, 12);
		item->getColumnLabel(label);
		char value[12];
		item->format(item->getValue(), value, sizeof(value));
		drawSettingCell(image, i, 4, label.c_str(), value, item == current);
	}
	OLED::markChanged();
}

// Shared with the keyboard's CHORD / CHORD LIB pages ------------------------------------------------------------

void drawChordKeyboard(deluge::hid::display::oled_canvas::Canvas& image) {
	drawKeyboard(image);
}

void drawChordNotes(deluge::hid::display::oled_canvas::Canvas& image, const int16_t* notes, int32_t count, int32_t root,
                    bool rootInHighOctave) {
	const int32_t rootPos = ((root % 12) + 12) % 12 + (rootInHighOctave ? 12 : 0);
	for (int32_t n = 0; n < count; n++) {
		int32_t p = rootPos + notes[n] - root;
		while (p < 0) {
			p += 12;
		}
		while (p > 23) {
			p -= 12;
		}
		drawNoteMark(image, p, notes[n] == root);
	}
}

void drawChordName(deluge::hid::display::oled_canvas::Canvas& image, int32_t root, const char* quality) {
	const int32_t nameW = OLED_MAIN_WIDTH_PIXELS - kNameX;
	image.drawStringCentered(kPitchNames[((root % 12) + 12) % 12], kNameX, kKeysTop + 1, kTextSpacingX,
	                         kTextSizeYUpdated, nameW);
	image.drawStringCentered(quality, kNameX, kKeysTop + 11, kTextSmallSpacingX, kTextSmallSizeY, nameW);
}

void drawEmptyCell(deluge::hid::display::oled_canvas::Canvas& image, int32_t index, int32_t count) {
	// a discreet dotted frame where a setting could go (as the horizontal menus' empty places)
	const int32_t w = (OLED_MAIN_WIDTH_PIXELS - kColX) / count;
	const int32_t startX = kColX + index * w + (w - 17) / 2;
	const int32_t endX = startX + 17;
	const int32_t startY = kColTop + 3;
	const int32_t endY = OLED_MAIN_HEIGHT_PIXELS - 4;
	constexpr int32_t kDotInterval = 5;
	for (int32_t x = startX + 1; x < endX; x += kDotInterval) {
		image.drawPixel(x, startY);
		image.drawPixel(x, endY);
	}
	for (int32_t y = startY + 3; y < endY; y += kDotInterval) {
		image.drawPixel(startX - 2, y);
		image.drawPixel(endX + 2, y);
	}
}

void drawArrowsCell(deluge::hid::display::oled_canvas::Canvas& image, int32_t index, int32_t count, const char* label,
                    bool selected) {
	const int32_t w = (OLED_MAIN_WIDTH_PIXELS - kColX) / count;
	const int32_t x = kColX + index * w;
	image.drawStringCentered(label, x, kLabelY, kTextSmallSpacingX, kTextSmallSizeY, w);
	// two filled arrowheads, left and right: turn either way
	const int32_t cx = x + (w - 1) / 2;
	const int32_t cy = kValueY + 3;
	for (int32_t i = 0; i < 4; i++) {
		for (int32_t dy = -i; dy <= i; dy++) {
			image.drawPixel(cx - 6 + i, cy + dy); // <
			image.drawPixel(cx + 6 - i, cy + dy); // >
		}
	}
	if (selected) {
		image.invertAreaRounded(x, w - 1, kColTop, OLED_MAIN_HEIGHT_PIXELS - 1);
	}
}

void drawSettingCell(deluge::hid::display::oled_canvas::Canvas& image, int32_t index, int32_t count, const char* label,
                     const char* value, bool selected) {
	const int32_t w = (OLED_MAIN_WIDTH_PIXELS - kColX) / count;
	const int32_t x = kColX + index * w;
	image.drawStringCentered(label, x, kLabelY, kTextSmallSpacingX, kTextSmallSizeY, w);
	image.drawStringCentered(value, x, kValueY, kTextSpacingX, kTextSizeYUpdated, w);
	if (selected) {
		image.invertAreaRounded(x, w - 1, kColTop, OLED_MAIN_HEIGHT_PIXELS - 1);
	}
}

} // namespace deluge::gui::menu_item::chord_mode
