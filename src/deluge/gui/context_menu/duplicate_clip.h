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

#include "gui/context_menu/context_menu.h"

namespace deluge::gui::context_menu {

/// SHIFT + horizontal encoder press in the clip view: PAGE+ (the page shown, 16 steps, added at the end) or
/// SEQUENCE (the whole clip doubled, as before)
class DuplicateClip final : public ContextMenu {
public:
	DuplicateClip() = default;
	char const* getTitle() override { return "DUPLICATE"; }
	std::span<char const*> getOptions() override;
	bool acceptCurrentOption() override;
	bool canSeeViewUnderneath() override { return true; }
};

extern DuplicateClip duplicateClip;

} // namespace deluge::gui::context_menu
