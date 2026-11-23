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

#include "gui/menu_item/integer.h"
#include "gui/menu_item/two_row_menu.h"
#include "hid/display/oled_canvas/canvas.h"
#include "util/chord_mode_calc.h"

// CHORD MODE (SOUND menu, synth tracks): "CHORD MODE" written down the left, a two-octave keyboard showing the chord
// of the last key played with its name on the right, then MODE TYPE STRUM BASS. No popups.

namespace deluge::gui::menu_item::chord_mode {

/// A CHORD MODE setting: shown as text, no popup
class Setting : public Integer {
public:
	using Integer::Integer;
	[[nodiscard]] bool showNotification() const override { return false; }
	void drawValue() override;
	void drawPixelsForOled() override;
	virtual void format(int32_t value, char* out, size_t size) const = 0;

protected:
	static deluge::chordmode::Settings* settings();
};

class Mode final : public Setting {
public:
	using Setting::Setting;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override;
	void getColumnLabel(StringBuf& label) override { label.append("MODE"); }
	void format(int32_t value, char* out, size_t size) const override;
};

class Type final : public Setting {
public:
	using Setting::Setting;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override;
	void getColumnLabel(StringBuf& label) override { label.append("TYPE"); }
	void format(int32_t value, char* out, size_t size) const override;
};

class Strum final : public Setting {
public:
	using Setting::Setting;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return deluge::chordmode::kStrumMaxMs; }
	void getColumnLabel(StringBuf& label) override { label.append("STRUM"); }
	void format(int32_t value, char* out, size_t size) const override;
};

class Bass final : public Setting {
public:
	using Setting::Setting;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	[[nodiscard]] int32_t getMinValue() const override { return deluge::chordmode::kBassMin; }
	[[nodiscard]] int32_t getMaxValue() const override { return deluge::chordmode::kBassMax; }
	void getColumnLabel(StringBuf& label) override { label.append("BASS"); }
	void format(int32_t value, char* out, size_t size) const override;
};

/// The CHORD MODE screen: SYNTH / KIT / MIDI / CV pick the column, LOAD = SELECT, turning SELECT edits
class Screen final : public TwoRowMenu {
public:
	using TwoRowMenu::TwoRowMenu;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void endSession() override;
	void renderOLED() override;
	ActionResult timerCallback() override;
	void selectEncoderAction(int32_t offset) override;
	ActionResult buttonAction(hid::Button b, bool on, bool inCardRoutine) override;

private:
	uint32_t drawnPlayCounter_ = 0;
	static void clearPopup();
};

// Drawing shared with the keyboard's CHORD / CHORD LIB pages
void drawChordKeyboard(deluge::hid::display::oled_canvas::Canvas& image);
/// The notes on the keyboard (the root marked), the root in the low octave or the high one
void drawChordNotes(deluge::hid::display::oled_canvas::Canvas& image, const int16_t* notes, int32_t count, int32_t root,
                    bool rootInHighOctave);
/// The chord's name on the right of the keyboard: root, then quality
void drawChordName(deluge::hid::display::oled_canvas::Canvas& image, int32_t root, const char* quality);
/// An empty place for a setting: a dotted frame
void drawEmptyCell(deluge::hid::display::oled_canvas::Canvas& image, int32_t index, int32_t count);
/// A setting under the keyboard, `index` of `count` columns: label above, value below, framed when selected
void drawSettingCell(deluge::hid::display::oled_canvas::Canvas& image, int32_t index, int32_t count, const char* label,
                     const char* value, bool selected);

} // namespace deluge::gui::menu_item::chord_mode
