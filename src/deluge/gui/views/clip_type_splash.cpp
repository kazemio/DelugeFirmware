/*
 * Copyright © 2019-2023 Synthstrom Audible Limited
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

#include "gui/views/clip_type_splash.h"

#include "gui/colour/colour.h"
#include "gui/fonts/fonts.h"

#include <algorithm>
#include <cstring>

using namespace deluge::gui;

namespace {

bool splashWasShowing = false;

constexpr int32_t kLetterSpacing = 1;
constexpr int32_t kFullSizeLetterHeight = 5;
constexpr int32_t kMiniLetterHeight = 3;

// Display row (0 = bottom pad row) holding the top pixel of a single-line word, and of each line
// of a stacked two-line word. Single line: one dark row above, two below. Stacked: one dark row
// above, one-row gap between the lines, bottom line flush with the bottom of the grid.
constexpr int32_t kSingleLineTopY = 6;
constexpr int32_t kStackedTopLineTopY = 6;
constexpr int32_t kStackedBottomLineTopY = 2;

// 3x3 glyphs for the stacked lines, column-major like font_5px: one byte per column, bit 0 = top pixel.
// Only the letters appearing in stacked clip-type words exist.
struct MiniGlyph {
	char letter;
	uint8_t columns[3];
};
constexpr MiniGlyph kMiniFont[] = {
    {'S', {0b100, 0b111, 0b001}}, {'Y', {0b001, 0b110, 0b001}}, {'N', {0b111, 0b001, 0b111}},
    {'T', {0b001, 0b111, 0b001}}, {'H', {0b111, 0b010, 0b111}}, {'A', {0b110, 0b011, 0b110}},
    {'U', {0b111, 0b100, 0b111}}, {'D', {0b111, 0b101, 0b010}}, {'I', {0b101, 0b111, 0b101}},
    {'O', {0b111, 0b101, 0b111}},
};

void drawGlyphColumn(uint8_t columnBits, int32_t x, int32_t topY, int32_t height, RGB colour, uint32_t whichRows,
                     RGB image[][kDisplayWidth + kSideBarWidth]) {
	if (x < 0 || x >= kDisplayWidth) {
		return;
	}
	for (int32_t i = 0; i < height; i++) {
		int32_t y = topY - i; // bit 0 is the glyph's top pixel; display rows count up from the bottom
		if (y >= 0 && y < kDisplayHeight && (whichRows & (1 << y)) && (columnBits & (1 << i))) {
			image[y][x] = colour;
		}
	}
}

int32_t fullSizeWordWidth(char const* word, size_t length) {
	int32_t width = -kLetterSpacing;
	for (size_t i = 0; i < length; i++) {
		width += font_5px_desc[word[i] - ' '].w_px + kLetterSpacing;
	}
	return width;
}

void drawFullSizeLine(char const* word, size_t length, int32_t x, RGB colour, uint32_t whichRows,
                      RGB image[][kDisplayWidth + kSideBarWidth]) {
	for (size_t i = 0; i < length; i++) {
		lv_font_glyph_dsc_t const& glyph = font_5px_desc[word[i] - ' '];
		for (int32_t col = 0; col < glyph.w_px; col++) {
			drawGlyphColumn(font_5px[glyph.glyph_index + col], x + col, kSingleLineTopY, kFullSizeLetterHeight, colour,
			                whichRows, image);
		}
		x += glyph.w_px + kLetterSpacing;
	}
}

void drawMiniLine(char const* letters, size_t length, int32_t topY, RGB colour, uint32_t whichRows,
                  RGB image[][kDisplayWidth + kSideBarWidth]) {
	int32_t width = static_cast<int32_t>(length) * (3 + kLetterSpacing) - kLetterSpacing;
	int32_t x = (kDisplayWidth - width) / 2;
	for (size_t i = 0; i < length; i++) {
		for (MiniGlyph const& glyph : kMiniFont) {
			if (glyph.letter == letters[i]) {
				for (int32_t col = 0; col < 3; col++) {
					drawGlyphColumn(glyph.columns[col], x + col, topY, kMiniLetterHeight, colour, whichRows, image);
				}
				break;
			}
		}
		x += 3 + kLetterSpacing;
	}
}

} // namespace

bool clipTypeSplashStateChanged(bool splashShowing) {
	if (splashShowing != splashWasShowing) {
		splashWasShowing = splashShowing;
		return true;
	}
	return false;
}

bool clipTypeSplashOnLivePads() {
	return splashWasShowing;
}

void clipTypeSplashBlankStoreRows(RGB store[][kDisplayWidth + kSideBarWidth], int32_t numRows) {
	for (int32_t y = 0; y < numRows; y++) {
		std::fill(&store[y][0], &store[y][kDisplayWidth], colours::black);
	}
}

void renderClipTypeSplash(char const* word, RGB colour, uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                          uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		if (whichRows & (1 << y)) {
			std::fill(&image[y][0], &image[y][kDisplayWidth], colours::black);
			if (occupancyMask) {
				memset(occupancyMask[y], 0, kDisplayWidth);
			}
		}
	}

	if (!word) {
		return;
	}

	size_t length = strlen(word);
	int32_t width = fullSizeWordWidth(word, length);
	if (width <= kDisplayWidth) {
		drawFullSizeLine(word, length, (kDisplayWidth - width) / 2, colour, whichRows, image);
	}
	else {
		// Too wide for one line: stack two lines of mini letters, longer half on top
		// ("SYNTH" -> "SYN"/"TH", "AUDIO" -> "AUD"/"IO")
		size_t topLength = (length + 1) / 2;
		drawMiniLine(word, topLength, kStackedTopLineTopY, colour, whichRows, image);
		drawMiniLine(word + topLength, length - topLength, kStackedBottomLineTopY, colour, whichRows, image);
	}
}
