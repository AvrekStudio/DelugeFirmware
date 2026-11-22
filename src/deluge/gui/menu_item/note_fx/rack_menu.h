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

#include "gui/menu_item/menu_item.h"
#include "gui/menu_item/selection.h"
#include "model/note_fx/note_fx_rack.h"

class InstrumentClip;

namespace deluge::gui::menu_item::note_fx {

using deluge::note_fx::Rack;
using deluge::note_fx::Type;

/// Common to every NOTE FX RACK screen: the vertical "NOTE FX" title on the left, the SYNC-SCALING / TRIPLETS VIEW
/// (row up / row down) and LOAD (= clicking SELECT) buttons lit while it's shown.
class RackScreen : public MenuItem {
public:
	using MenuItem::MenuItem;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void endSession() override;
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override;

protected:
	static void drawTitleStrip();
	/// Title + list of items (4 visible, scrolls), selected one highlighted.
	static void drawList(const char* title, const char* const* items, int32_t count, int32_t selected);
	void refresh();
};

/// The rack: 8 slots on two rows. SELECT turns to move between slots, SYNC-SCALING / TRIPLETS VIEW jump to the top /
/// bottom row, clicking an empty slot opens the list of modules, a filled one its actions.
class RackMenu final : public RackScreen {
public:
	using RackScreen::RackScreen;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void renderOLED() override;
	void drawName() override;
	void selectEncoderAction(int32_t offset) override;
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	MenuItem* selectButtonPress() override;
};

/// "+": the modules not yet in the rack.
class AddList final : public RackScreen {
public:
	using RackScreen::RackScreen;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void renderOLED() override;
	void drawName() override;
	void selectEncoderAction(int32_t offset) override;
	MenuItem* selectButtonPress() override;

private:
	Type choices_[static_cast<int32_t>(Type::NUM_TYPES)]{};
	int32_t numChoices_ = 0;
	int32_t selected_ = 0;
};

/// A filled slot: EDIT, ON/OFF, MOVE ->, <- MOVE, DEL.
class ActionList final : public RackScreen {
public:
	using RackScreen::RackScreen;
	void beginSession(MenuItem* navigatedBackwardFrom) override;
	void renderOLED() override;
	void drawName() override;
	void selectEncoderAction(int32_t offset) override;
	MenuItem* selectButtonPress() override;
	MenuPermission checkPermissionToBeginSession(ModControllableAudio* modControllable, int32_t whichThing,
	                                             ::MultiRange** currentRange) override;

private:
	enum class Action : uint8_t { EDIT, TOGGLE, MOVE_RIGHT, MOVE_LEFT, DEL };
	void buildActions();
	Action actions_[5]{};
	int32_t numActions_ = 0;
	int32_t selected_ = 0;
};

/// The DIR module's setting: the clip's play direction.
class DirEdit final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override;
	void writeCurrentValue() override;
	deluge::vector<std::string_view> getOptions(OptType optType) override;
};

/// The clip whose rack the menu shows (nullptr outside an instrument clip).
InstrumentClip* currentRackClip();
/// Makes the clip play as its rack says (DIR on: its direction, otherwise forward).
void applyRack(InstrumentClip* clip);

extern RackMenu noteFxRackMenu;
extern AddList noteFxAddList;
extern ActionList noteFxActionList;
extern DirEdit noteFxDirEdit;

} // namespace deluge::gui::menu_item::note_fx
