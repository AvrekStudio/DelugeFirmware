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

#include "gui/menu_item/euclid/euclid_menus.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"
#include "model/clip/instrument_clip.h"
#include "model/model_stack.h"
#include "model/note/euclid.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"

namespace deluge::gui::menu_item::euclid {

namespace {

/// The row whose AUD pad opened the page (nullptr if none)
ModelStackWithNoteRow* currentRow(char* memory) {
	if (getCurrentClip() == nullptr || getCurrentClip()->type != ClipType::INSTRUMENT) {
		return nullptr;
	}
	ModelStackWithTimelineCounter* modelStack = currentSong->setupModelStackWithCurrentClip(memory);
	auto* clip = static_cast<InstrumentClip*>(modelStack->getTimelineCounter());
	ModelStackWithNoteRow* rowStack = clip->getNoteRowOnScreen(instrumentClipView.lastAuditionedYDisplay, modelStack);
	return rowStack->getNoteRowAllowNull() != nullptr ? rowStack : nullptr;
}

int32_t currentSteps() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	return rowStack != nullptr ? rowStack->getNoteRow()->euclidSteps(rowStack) : 16;
}

/// The grid shows the new pattern
void redraw() {
	uiNeedsRendering(&instrumentClipView, 0xFFFFFFFF, 0);
	uiNeedsRendering(&soundEditor, 0xFFFFFFFF, 0);
}

} // namespace

// Param --------------------------------------------------------------------------------------------------------------

int32_t Param::getMinValue() const {
	switch (field_) {
	case Field::LEN:
		return deluge::euclid::kMinSteps;
	case Field::RO1:
	case Field::RO2:
	case Field::TRO:
		return deluge::euclid::minRotation(currentSteps());
	default:
		return 0;
	}
}

int32_t Param::getMaxValue() const {
	switch (field_) {
	case Field::LEN:
		return std::min<int32_t>(deluge::euclid::kMaxSteps, kMaxSequenceLength / NoteRow::euclidStepTicks());
	case Field::RO1:
	case Field::RO2:
	case Field::TRO:
		return deluge::euclid::maxRotation(currentSteps());
	default:
		return currentSteps();
	}
}

void Param::readCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	if (rowStack == nullptr) {
		setValue(0);
		return;
	}
	NoteRow* row = rowStack->getNoteRow();
	const deluge::euclid::Settings& s = row->euclid;
	switch (field_) {
	case Field::PL1:
		setValue(s.pulses1);
		break;
	case Field::PL2:
		setValue(s.pulses2);
		break;
	case Field::LEN:
		setValue(row->euclidSteps(rowStack));
		break;
	case Field::RO1:
		setValue(s.rotate1);
		break;
	case Field::RO2:
		setValue(s.rotate2);
		break;
	case Field::TRO:
		setValue(s.trackRotate);
		break;
	}
}

void Param::selectEncoderAction(int32_t offset) {
	if (field_ == Field::RO1 || field_ == Field::RO2 || field_ == Field::TRO) {
		// the rotations go round: past one end, the other
		readCurrentValue();
		setValue(deluge::euclid::wrapRotation(getValue() + (offset > 0 ? 1 : -1), currentSteps()));
		writeCurrentValue();
		renderUIsForOled();
		return;
	}
	Integer::selectEncoderAction(offset);
}

void Param::writeCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	if (rowStack == nullptr) {
		return;
	}
	NoteRow* row = rowStack->getNoteRow();
	deluge::euclid::Settings& s = row->euclid;
	const int32_t v = getValue();
	switch (field_) {
	case Field::PL1:
		s.pulses1 = v;
		break;
	case Field::PL2:
		s.pulses2 = v;
		break;
	case Field::RO1:
		s.rotate1 = v;
		break;
	case Field::RO2:
		s.rotate2 = v;
		break;
	case Field::TRO:
		s.trackRotate = v;
		break;
	case Field::LEN: {
		// the row's own length, in steps of 1/16 (it regenerates the pattern if EUC is on)
		const int32_t newLength = v * NoteRow::euclidStepTicks();
		if (newLength != rowStack->getLoopLength() && newLength <= kMaxSequenceLength) {
			const int32_t oldPos = rowStack->getLastProcessedPos();
			const bool hadIndependentPlayPos = row->hasIndependentPlayPos();
			row->setLength(rowStack, newLength, nullptr, oldPos, hadIndependentPlayPos);
		}
		deluge::euclid::fitTo(s, v);
		redraw();
		return;
	}
	}
	row->regenerateEuclid(rowStack);
	redraw();
}

// EUC ----------------------------------------------------------------------------------------------------------------

void Enabled::readCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	setValue(rowStack != nullptr && rowStack->getNoteRow()->euclid.enabled);
}

void Enabled::writeCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	if (rowStack != nullptr) {
		rowStack->getNoteRow()->setEuclidEnabled(getValue(), rowStack);
		redraw();
	}
}

// OP -----------------------------------------------------------------------------------------------------------------

void Operator::readCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	setValue(rowStack != nullptr ? static_cast<int32_t>(rowStack->getNoteRow()->euclid.op) : 0);
}

void Operator::writeCurrentValue() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	if (rowStack != nullptr) {
		NoteRow* row = rowStack->getNoteRow();
		row->euclid.op = static_cast<deluge::euclid::Op>(getValue());
		row->regenerateEuclid(rowStack);
		redraw();
	}
}

// The page -----------------------------------------------------------------------------------------------------------

namespace {
Param pl1{Field::PL1, "PL1"};
Param pl2{Field::PL2, "PL2"};
Param len{Field::LEN, "LEN"};
Enabled enabled{};
Param ro1{Field::RO1, "RO1"};
Param ro2{Field::RO2, "RO2"};
Param tro{Field::TRO, "TRO"};
Operator op{};
} // namespace

Screen euclidScreen{
    l10n::String::EMPTY_STRING, l10n::String::EMPTY_STRING, {&pl1, &pl2, &len, &enabled, &ro1, &ro2, &tro, &op}};

void Screen::selectPressed() {
	char memory[MODEL_STACK_MAX_SIZE];
	ModelStackWithNoteRow* rowStack = currentRow(memory);
	if (rowStack == nullptr) {
		return;
	}
	NoteRow* row = rowStack->getNoteRow();
	row->setEuclidEnabled(!row->euclid.enabled, rowStack);
	redraw();
	renderUIsForOled();
}

} // namespace deluge::gui::menu_item::euclid
