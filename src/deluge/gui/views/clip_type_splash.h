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

#pragma once

#include "definitions_cxx.hpp"
#include "gui/colour/rgb.h"

#include <cstdint>

// Renders a clip-type "splash" word (e.g. "SYNTH", "FX") centered on the 16 main pad columns,
// black-filling the main-pad area first. Words that fit draw full-height from font_5px; wider
// words stack two lines of a local 3x3 font. Pass word == nullptr to just blank the main pads.
// Only rows present in whichRows are touched; the sidebar columns are left alone.
void renderClipTypeSplash(char const* word, RGB colour, uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                          uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]);

// Call once per renderMainPads with whether the splash word will be drawn this render. Returns
// true when that differs from the previous render - every pad is then stale (partial row requests
// would leave word pixels behind, or draw the word onto only some rows), so the caller must
// repaint all rows and drop any render caches.
bool clipTypeSplashStateChanged(bool splashShowing);

// Boot gate: the blank boot song's default synth clip renders before any startup song replaces
// it, which made "SYNTH" flash at power-on. The splash stays off until this is called once boot
// has settled (after the startup-song task resolves, including its do-nothing blank mode).
void clipTypeSplashSetBootSettled();
bool clipTypeSplashBootSettled();

// True when the last live-pad render by a clip view drew the splash word. Only meaningful while
// the current UI is instrumentClipView or audioClipView (other views don't update the state).
// Transitions leaving the clip use this to start their collapse/fade animation from black
// instead of animating the word out, which reads as lag.
bool clipTypeSplashOnLivePads();

// Blanks the main-pad columns (sidebar untouched) of numRows rows of a transition image store.
void clipTypeSplashBlankStoreRows(RGB store[][kDisplayWidth + kSideBarWidth], int32_t numRows);
