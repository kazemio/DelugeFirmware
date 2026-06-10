/*
 * Copyright © 2014-2023 Synthstrom Audible Limited
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

#include <cstdint>

// Re-broadcasts a single learned incoming CC (GlobalMIDICommand::FAN_OUT) to up to
// kNumDestSlots destination CCs on the active MIDI clip's output.
namespace MIDIFanOut {

constexpr int32_t kNumDestSlots = 8;
constexpr uint8_t kDestCCNone = 255;
constexpr uint8_t kMaxDestCC = 119;

extern uint8_t destCC[kNumDestSlots];

void sendFanOut(int32_t value);
}; // namespace MIDIFanOut
