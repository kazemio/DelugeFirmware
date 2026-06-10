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

#include "io/midi/midi_fanout.h"
#include "definitions_cxx.hpp"
#include "io/midi/midi_engine.h"
#include "io/midi/midi_follow.h"
#include "model/clip/clip.h"
#include "model/instrument/midi_instrument.h"
#include "model/output.h"

namespace MIDIFanOut {

uint8_t destCC[kNumDestSlots] = {kDestCCNone, kDestCCNone, kDestCCNone, kDestCCNone,
                                 kDestCCNone, kDestCCNone, kDestCCNone, kDestCCNone};

void sendFanOut(int32_t value) {
	Clip* clip = midiFollow.getSelectedOrActiveClip();
	if (!clip || !clip->output || clip->output->type != OutputType::MIDI_OUT) {
		return;
	}
	MIDIInstrument* instrument = (MIDIInstrument*)clip->output;
	int32_t midiOutputFilter = instrument->getChannel();
	int32_t masterChannel = instrument->getOutputMasterChannel();
	for (uint8_t cc : destCC) {
		if (cc != kDestCCNone) {
			midiEngine.sendCC(instrument, masterChannel, cc, value, midiOutputFilter);
		}
	}
}

} // namespace MIDIFanOut
