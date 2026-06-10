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
#include "gui/menu_item/integer.h"
#include "io/midi/midi_fanout.h"

namespace deluge::gui::menu_item::midi {

// One destination CC slot of the MIDI fan-out. Menu value 0 = OFF, 1-120 = CC 0-119.
class FanOutDest final : public IntegerWithOff {
public:
	FanOutDest(l10n::String newName, int32_t newSlot) : IntegerWithOff(newName), slot(newSlot) {}
	[[nodiscard]] int32_t getMaxValue() const override { return MIDIFanOut::kMaxDestCC + 1; }
	void readCurrentValue() override {
		uint8_t cc = MIDIFanOut::destCC[slot];
		this->setValue((cc == MIDIFanOut::kDestCCNone) ? 0 : cc + 1);
	}
	void writeCurrentValue() override {
		int32_t value = this->getValue();
		MIDIFanOut::destCC[slot] = (value == 0) ? MIDIFanOut::kDestCCNone : value - 1;
	}

protected:
	int32_t getDisplayValue() override { return this->getValue() - 1; }

private:
	int32_t slot;
};
} // namespace deluge::gui::menu_item::midi
