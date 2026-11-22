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

#pragma once

#include "definitions_cxx.hpp"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/toggle.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/instrument/instrument.h"
#include "model/instrument/track_note_defaults.h"
#include "model/song/song.h"
#include "util/functions.h"

/// MASTER menu, second row: the track's default note - LEN, NOTE, SCL (synth tracks).
namespace deluge::gui::menu_item::track_defaults {

using namespace deluge::track_defaults;

inline NoteDefaults* currentNoteDefaults() {
	Instrument* instrument = getCurrentInstrument();
	return instrument != nullptr ? &instrument->noteDefaults : nullptr;
}

inline bool editingSynthTrack() {
	return getCurrentOutputType() == OutputType::SYNTH && !soundEditor.editingKit();
}

inline void drawSlotText(const char* text, const SlotPosition& slot) {
	deluge::hid::display::OLED::main.drawStringCentered(text, slot.start_x,
	                                                    slot.start_y + kHorizontalMenuSlotYOffset + 2, kTextSpacingX,
	                                                    kTextSizeYUpdated, slot.width);
}

/// LEN: length of a note placed by a tap on the grid, 0.125x to 128x a square in steps of 0.125, or INF.
class Len final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		NoteDefaults* d = currentNoteDefaults();
		setValue(d != nullptr ? d->lenEighths : kLenNormal);
	}
	void writeCurrentValue() override {
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->lenEighths = static_cast<int16_t>(getValue());
		}
	}
	[[nodiscard]] int32_t getMinValue() const override { return kLenInfinite; }
	[[nodiscard]] int32_t getMaxValue() const override { return kLenMax; }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return editingSynthTrack();
	}
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append("LEN"); }
	void renderInHorizontalMenu(const SlotPosition& slot) override {
		char buf[8];
		formatLen(getValue(), buf, sizeof(buf));
		drawSlotText(buf, slot);
	}
	void drawPixelsForOled() override {
		char buf[8];
		formatLen(getValue(), buf, sizeof(buf));
		deluge::hid::display::OLED::main.drawStringCentred(buf, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX,
		                                                   kTextHugeSizeY);
	}
	void drawValue() override {
		char buf[8];
		formatLen(getValue(), buf, sizeof(buf));
		display->setText(buf, true);
	}
};

/// NOTE: the note the track's note generators play. With SCL on it only steps through the song's scale.
class Note final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		NoteDefaults* d = currentNoteDefaults();
		setValue(d != nullptr ? d->note : 60);
	}
	void writeCurrentValue() override {
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->note = static_cast<uint8_t>(getValue());
		}
	}
	void selectEncoderAction(int32_t offset) override {
		NoteDefaults* d = currentNoteDefaults();
		const bool lock = d != nullptr && d->scaleLock;
		setValue(stepNote(getValue(), offset, lock, currentSong->key.modeNotes.toBits(), currentSong->key.rootNote));
		Number::selectEncoderAction(offset);
	}
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return 127; }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return editingSynthTrack();
	}
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append("NOTE"); }
	void renderInHorizontalMenu(const SlotPosition& slot) override {
		char buf[kNoteCodeStringBufferSize];
		noteCodeToString(getValue(), buf);
		drawSlotText(buf, slot);
	}
	void drawPixelsForOled() override {
		char buf[kNoteCodeStringBufferSize];
		noteCodeToString(getValue(), buf);
		deluge::hid::display::OLED::main.drawStringCentred(buf, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX,
		                                                   kTextHugeSizeY);
	}
	void drawValue() override {
		char buf[kNoteCodeStringBufferSize];
		noteCodeToString(getValue(), buf);
		display->setText(buf, true);
	}
};

/// SCL: NOTE limited to the song's scale (on) or every note (off). Switching it on moves NOTE onto the scale.
class ScaleLock final : public Toggle {
public:
	using Toggle::Toggle;
	void readCurrentValue() override {
		NoteDefaults* d = currentNoteDefaults();
		setValue(d == nullptr || d->scaleLock);
	}
	void writeCurrentValue() override {
		if (NoteDefaults* d = currentNoteDefaults()) {
			d->scaleLock = getValue();
			if (d->scaleLock) {
				d->note = static_cast<uint8_t>(
				    snapToScale(d->note, currentSong->key.modeNotes.toBits(), currentSong->key.rootNote));
			}
		}
	}
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return editingSynthTrack();
	}
	[[nodiscard]] bool showNotification() const override { return false; }
	void getColumnLabel(StringBuf& label) override { label.append("SCL"); }
	void renderInHorizontalMenu(const SlotPosition& slot) override { drawSlotText(getValue() ? "ON" : "OFF", slot); }
};

} // namespace deluge::gui::menu_item::track_defaults
