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
#include "gui/menu_item/integer.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/instrument/instrument.h"
#include "model/instrument/track_note_defaults.h"
#include "model/song/song.h"
#include "processing/sound/sound_drum.h"
#include "util/functions.h"

/// MASTER menu, second row: LEN / NOTE / RAND (synth and kit tracks)
namespace deluge::gui::menu_item::track_defaults {

using namespace deluge::track_defaults;

inline NoteDefaults* currentNoteDefaults() {
	Instrument* instrument = getCurrentInstrument();
	return instrument != nullptr ? &instrument->noteDefaults : nullptr;
}

inline void drawSlotText(const char* text, const SlotPosition& slot) {
	deluge::hid::display::OLED::main.drawStringCentered(text, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset,
	                                                    kTextSpacingX, kTextSpacingY, slot.width);
}

inline void drawBigText(const char* text) {
	deluge::hid::display::OLED::main.drawStringCentred(text, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX,
	                                                   kTextHugeSizeY);
}

/// A value shown as text everywhere (cell, full screen, 7-segment)
class TextInteger : public Integer {
public:
	using Integer::Integer;
	[[nodiscard]] bool showNotification() const override { return false; }
	void renderInHorizontalMenu(const SlotPosition& slot) override {
		char buf[12];
		format(getValue(), buf, sizeof(buf));
		drawSlotText(buf, slot);
	}
	void drawPixelsForOled() override {
		char buf[12];
		format(getValue(), buf, sizeof(buf));
		drawBigText(buf);
	}
	void drawValue() override {
		char buf[12];
		format(getValue(), buf, sizeof(buf));
		display->setText(buf, true);
	}

protected:
	virtual void format(int32_t value, char* out, size_t size) = 0;
};

/// LEN: length of a note placed by a tap on the grid, 0.25 to 64 steps (quarters up to 4, then whole steps)
class Len final : public TextInteger {
public:
	using TextInteger::TextInteger;
	void readCurrentValue() override {
		NoteDefaults* d = currentNoteDefaults();
		setValue(lenIndexFromQuarters(d != nullptr ? d->lenQuarters : kLenQuartersDefault));
	}
	void writeCurrentValue() override {
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->lenQuarters = static_cast<int16_t>(lenQuartersFromIndex(getValue()));
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return kLenIndexMax; }
	void getColumnLabel(StringBuf& label) override { label.append("LEN"); }

protected:
	void format(int32_t value, char* out, size_t size) override { formatLen(lenQuartersFromIndex(value), out, size); }
};

/// NOTE: synth track = the note its generators play; kit row = the note its sample plays at (C3 = as recorded)
class Note final : public TextInteger {
public:
	using TextInteger::TextInteger;
	void readCurrentValue() override {
		if (SoundDrum* drum = currentDrum()) {
			setValue(drum->defaultNote);
			return;
		}
		NoteDefaults* d = currentNoteDefaults();
		setValue(d != nullptr ? d->note : 60);
	}
	void writeCurrentValue() override {
		if (SoundDrum* drum = currentDrum()) {
			drum->defaultNote = static_cast<uint8_t>(getValue());
			return;
		}
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->note = static_cast<uint8_t>(getValue());
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return 127; }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return !soundEditor.editingKitAffectEntire();
	}
	void getColumnLabel(StringBuf& label) override { label.append("NOTE"); }

protected:
	void format(int32_t value, char* out, size_t size) override { noteCodeToString(value, out); }

private:
	static SoundDrum* currentDrum() {
		return soundEditor.editingKitRow() ? static_cast<SoundDrum*>(soundEditor.currentSound) : nullptr;
	}
};

/// RAND: the chance, 0 to 100 %, that each note of the track plays
class Rand final : public TextInteger {
public:
	using TextInteger::TextInteger;
	void readCurrentValue() override {
		NoteDefaults* d = currentNoteDefaults();
		setValue(d != nullptr ? d->chance : kChanceMax);
	}
	void writeCurrentValue() override {
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->chance = static_cast<uint8_t>(getValue());
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return kChanceMax; }
	void getColumnLabel(StringBuf& label) override { label.append("RAND"); }

protected:
	void format(int32_t value, char* out, size_t size) override {
		snprintf(out, size, "%d%%", static_cast<int>(value));
	}
};

} // namespace deluge::gui::menu_item::track_defaults
