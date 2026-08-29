/*
 * Copyright © 2026 Synthstrom Audible Limited
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
#include "gui/l10n/l10n.h"
#include "gui/l10n/strings.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/toggle.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/oled.h"
#include "processing/sound/sound.h"
#include "util/d_stringbuf.h"
#include "util/lookuptables/lookuptables.h"
#include <cstdint>

/// CHORD MODE (SOUND menu, Synth only): on every note-on, stacks up to 3 extra notes on top of the root, in root
/// position, without writing them onto the sequencer grid - see Sound::chordModeOn and Sound::noteOn/noteOff.
/// Two screens, paged with SCALE / CROSS SCREEN (same HorizontalMenuGroup mechanism as ARP's BASIC/PATTERN pair -
/// see chordModeGroup in menus.cpp):
///   MAIN:   ENABLED (on/off), TYPE (pick a chord type from the same simple 9-type table used by the Chords
///           sidebar column / Arp Chord Simulator - NONE included), STRUM RATE (milliseconds between each
///           stacked note firing, minimum 0 - see Sound::chordModeStrumRateMs).
///   MANUAL: the 3 signed semitone offsets, used whenever TYPE is left on NONE - see
///           Sound::getChordModeExtraOffsets.
/// TYPE wins whenever it isn't NONE; MANUAL only applies while TYPE is left on NONE.
namespace deluge::gui::menu_item::chord_mode {

class OnOff final : public Toggle {
public:
	using Toggle::Toggle;
	void readCurrentValue() override { this->setValue(soundEditor.currentSound->chordModeOn); }
	void writeCurrentValue() override { soundEditor.currentSound->chordModeOn = this->getValue(); }
};

/// One of the 3 manual semitone-offset slots. `slotIndex` (0-2) picks which of Sound::chordModeOffsets it edits.
/// Shown 3-up on the MANUAL screen (see menus.cpp), selected via SYNTH/KIT/MIDI.
class Offset final : public Integer {
public:
	Offset(l10n::String newName, l10n::String title, int32_t slotIndex)
	    : Integer(newName, title), slotIndex_(slotIndex) {}
	void readCurrentValue() override { this->setValue(soundEditor.currentSound->chordModeOffsets[slotIndex_]); }
	void writeCurrentValue() override {
		soundEditor.currentSound->chordModeOffsets[slotIndex_] = static_cast<int8_t>(this->getValue());
	}
	[[nodiscard]] int32_t getMinValue() const override { return -12; }
	[[nodiscard]] int32_t getMaxValue() const override { return 12; }
	// Show the actual number (e.g. "+5") instead of a knob graphic, matching arpOctavesMenu/arpStepRepeatMenu on
	// the ARP PATTERN screen.
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }
	void getColumnLabel(StringBuf& label) override {
		label.append(deluge::l10n::get(deluge::l10n::built_in::seven_segment, this->name));
	}
	// 0 means "no note in this slot" - show OFF instead of "0", same pattern as e.g. arpeggiator::SequenceLength.
	// A positive offset gets an explicit "+" (negative ones already read fine with the sign the number itself
	// carries) so it's unambiguous at a glance which side of the root each stacked note sits on.
	void renderInHorizontalMenu(const SlotPosition& slot) override {
		int32_t value = getValue();
		if (value == 0) {
			const auto off_string = l10n::get(l10n::String::STRING_FOR_OFF);
			return deluge::hid::display::OLED::main.drawStringCentered(off_string, slot.start_x,
			                                                           slot.start_y + kHorizontalMenuSlotYOffset,
			                                                           kTextSpacingX, kTextSpacingY, slot.width);
		}
		if (value > 0) {
			DEF_STACK_STRING_BUF(paramValue, 10);
			paramValue.append('+');
			paramValue.appendInt(value);
			return deluge::hid::display::OLED::main.drawStringCentered(paramValue, slot.start_x,
			                                                           slot.start_y + kHorizontalMenuSlotYOffset,
			                                                           kTextTitleSpacingX, kTextTitleSizeY, slot.width);
		}
		Number::renderInHorizontalMenu(slot);
	}

private:
	int32_t slotIndex_;
};

/// The MAIN screen's TYPE item: pick a chord type (including NONE) from the same table the Chords sidebar column /
/// Arp Chord Simulator use. Whenever this isn't NONE it overrides the MANUAL offsets entirely - see
/// Sound::getChordModeExtraOffsets.
class Type final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override { this->setValue(soundEditor.currentSound->chordModeType); }
	void writeCurrentValue() override {
		int32_t currentValue = this->getValue();
		if (currentValue < 0 || currentValue >= MAX_CHORD_TYPES) {
			return;
		}
		soundEditor.currentSound->chordModeType = currentValue;
	}
	void getColumnLabel(StringBuf& label) override {
		label.append(deluge::l10n::get(deluge::l10n::built_in::seven_segment, this->name));
	}
	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_NONE),      //<
		    l10n::getView(STRING_FOR_FIFTH),     //<
		    l10n::getView(STRING_FOR_SUS2),      //<
		    l10n::getView(STRING_FOR_MINOR),     //<
		    l10n::getView(STRING_FOR_MAJOR),     //<
		    l10n::getView(STRING_FOR_SUS4),      //<
		    l10n::getView(STRING_FOR_MINOR7),    //<
		    l10n::getView(STRING_FOR_DOMINANT7), //<
		    l10n::getView(STRING_FOR_MAJOR7),    //<
		};
	}
};

/// The MAIN screen's STRUM RATE item: the number of milliseconds between each stacked note firing - see
/// Sound::chordModeStrumRateMs and Sound::noteOn/noteOff/render. 0, its minimum, still staggers the notes by a
/// clamped, inaudible ~1ms rather than firing them in one instant block chord - see chordModeStrumRateMs's own
/// comment for why. Only has any effect with the arp off - the arp itself is never touched by this setting
/// either way.
class StrumRate final : public Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override { this->setValue(soundEditor.currentSound->chordModeStrumRateMs); }
	void writeCurrentValue() override { soundEditor.currentSound->chordModeStrumRateMs = this->getValue(); }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return 500; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }
	void getColumnLabel(StringBuf& label) override {
		label.append(deluge::l10n::get(deluge::l10n::built_in::seven_segment, this->name));
	}
	// 0 means STRUM is off - show OFF instead of "0", same pattern as chord_mode::Offset / arpeggiator::SequenceLength.
	void renderInHorizontalMenu(const SlotPosition& slot) override {
		if (getValue() == 0) {
			const auto off_string = l10n::get(l10n::String::STRING_FOR_OFF);
			return deluge::hid::display::OLED::main.drawStringCentered(off_string, slot.start_x,
			                                                           slot.start_y + kHorizontalMenuSlotYOffset,
			                                                           kTextSpacingX, kTextSpacingY, slot.width);
		}
		Number::renderInHorizontalMenu(slot);
	}
};

} // namespace deluge::gui::menu_item::chord_mode
