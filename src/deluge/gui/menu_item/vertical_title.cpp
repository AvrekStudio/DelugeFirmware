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

#include "gui/menu_item/vertical_title.h"
#include "definitions_cxx.hpp"
#include "hid/display/oled.h"

namespace deluge::gui::menu_item {

namespace {
// Scratch canvas the title is written on horizontally before being copied rotated (static: 768 bytes).
deluge::hid::display::oled_canvas::Canvas scratch;
} // namespace

void drawVerticalTitle(std::string_view title, bool small) {
	using deluge::hid::display::OLED;
	auto& image = OLED::main;
	constexpr int32_t kLength = OLED_MAIN_VISIBLE_HEIGHT; // room along the strip
	const int32_t kTextH = small ? kTextSmallSizeY : kTextSizeYUpdated;
	const int32_t spacingX = small ? kTextSmallSpacingX : kTextSpacingX;
	const int32_t x0 = small ? 3 : 2;

	scratch.clear();
	const int32_t width = scratch.getStringWidthInPixels(title.data(), kTextH);
	const int32_t start = width < kLength ? (kLength - width) / 2 : 0;
	scratch.drawString(title, start, 0, spacingX, kTextH, 0, kLength);

	auto& src = scratch.hackGetImageStore();
	for (int32_t x = 0; x < kLength; x++) {
		for (int32_t y = 0; y < kTextH; y++) {
			if ((src[y >> 3][x] >> (y & 7)) & 1) {
				// reads bottom to top: the text's top faces the left edge
				image.drawPixel(x0 + y, OLED_MAIN_TOPMOST_PIXEL + (kLength - 1 - x));
			}
		}
	}
	image.invertArea(0, kVerticalTitleWidth, OLED_MAIN_TOPMOST_PIXEL, OLED_MAIN_HEIGHT_PIXELS - 1);
}

} // namespace deluge::gui::menu_item
