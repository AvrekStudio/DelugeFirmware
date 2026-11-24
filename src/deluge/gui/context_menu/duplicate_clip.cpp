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

#include "gui/context_menu/duplicate_clip.h"
#include "gui/views/instrument_clip_view.h"

namespace deluge::gui::context_menu {

DuplicateClip duplicateClip{};

std::span<char const*> DuplicateClip::getOptions() {
	static char const* options[] = {"PAGE+", "SEQUENCE"};
	return {options, 2};
}

bool DuplicateClip::acceptCurrentOption() {
	const int32_t option = currentOption;
	close(); // back on the clip view first: its undo needs it
	if (option == 0) {
		instrumentClipView.duplicatePageAction();
	}
	else {
		instrumentClipView.doubleClipLengthAction();
	}
	return true;
}

} // namespace deluge::gui::context_menu
