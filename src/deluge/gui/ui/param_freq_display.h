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

#include "modulation/params/param.h"
#include "util/d_stringbuf.h"

#include <cstdint>

// Display-only conversion of params stored/automated as 0-50 menu values / 0-128 knob positions
// (nothing about storage changes) to real-world units: filter and EQ cutoffs in Hz, free-running
// LFO / mod-FX / arp rates in Hz, and envelope attack/decay/release stage lengths in ms/s.
// Gated behind the ShowRealUnits community feature; tempo-synced LFO/arp rates are
// suppressed (the Hz reading would be wrong).
namespace deluge::gui::param_freq_display {

// True when kind+paramID identify a convertible param AND the community flag is on AND (for
// LFO/arp rates) the thing is free-running.
bool shouldShowHz(deluge::modulation::params::Kind kind, int32_t paramID);

// Appends e.g. "310 Hz" / "1.8 kHz" for a 0-50 menu value. compact drops the unit: "310", "1.8k".
void appendHzForMenuValue(deluge::modulation::params::Kind kind, int32_t paramID, int32_t menuValue, StringBuf& buf,
                          bool compact = false);

// Same for a 0-128 knob position (centre 64), the space used by gold-knob popups and macro targets.
void appendHzForKnobPos(deluge::modulation::params::Kind kind, int32_t paramID, int32_t knobPos, StringBuf& buf,
                        bool compact = false);

// Draws the Hz reading as a small centred line under the big value of a full-screen OLED menu.
// No-op when the flag is off or the param has no Hz meaning.
void drawMenuHzLine(deluge::modulation::params::Kind kind, int32_t paramID, int32_t menuValue);

// Draws a compact Hz reading in a horizontal-menu slot. Returns false (drawing nothing) when the
// flag is off or the param has no Hz meaning, so the caller can fall back to its usual rendering.
bool drawCompactHz(deluge::modulation::params::Kind kind, int32_t paramID, int32_t menuValue, int32_t startX,
                   int32_t yPixel, int32_t width);

} // namespace deluge::gui::param_freq_display
