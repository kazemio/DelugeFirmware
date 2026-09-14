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

/// OLED takeover cycled from any view by holding CROSS SCREEN and tapping SCALE:
/// normal view -> spectrum analyzer -> display off (burn-in protection) -> normal view.
///
/// The spectrum follows the current context (the clip's output in clip views, the song master
/// everywhere else, including MIDI/CV clips which make no audio themselves). Both takeover modes
/// step aside while a menu or browser is open, and always in automation view (its parameter
/// readout is the point of the view), resuming afterwards. Returning to normal repaints and
/// resends the whole frame, restoring the screen exactly as if it was never taken over.
namespace deluge::gui::spectrum_overlay {

void toggle();

/// True when the spectrum should replace the root UI's OLED contents this frame.
bool shouldShowSpectrum();

/// True when the OLED should be blanked (all pixels off) this frame.
bool shouldBlankDisplay();

/// TimerName::SPECTRUM_OVERLAY handler: retarget, run the FFT, request a redraw, re-arm.
void timerEvent();

} // namespace deluge::gui::spectrum_overlay
