/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
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
#include "gui/l10n/strings.h"
#include "gui/menu_item/selection.h"
#include "gui/ui/sound_editor.h"

namespace deluge::gui::menu_item::arpeggiator::midi_cv {

/// Per-clip toggle for MIDI/CV clips. When on, MIDI-follow CCs that are mapped to an arp param control this clip's
/// arpeggiator instead of being passed through to the external instrument. Defaults off.
class MidiIntercept final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override { this->setValue(soundEditor.currentArpSettings->midiInterceptArp); }
	void writeCurrentValue() override { soundEditor.currentArpSettings->midiInterceptArp = this->getValue() != 0; }

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_OFF), //<
		    l10n::getView(STRING_FOR_ON),  //<
		};
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) override {
		return soundEditor.editingCVOrMIDIClip();
	}

	void getColumnLabel(StringBuf& label) override {
		label.append(deluge::l10n::getView(deluge::l10n::built_in::seven_segment, this->name).data());
	}

	// render as a checkbox toggle rather than a value list
	bool isToggle() override { return true; }

	// don't enter a submenu on select button press
	bool shouldEnterSubmenu() override { return false; }
};
} // namespace deluge::gui::menu_item::arpeggiator::midi_cv
