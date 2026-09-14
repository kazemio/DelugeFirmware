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
#include "gui/menu_item/unpatched_param.h"
#include "gui/ui/sound_editor.h"
#include "modulation/params/param_set.h"

namespace deluge::gui::menu_item::fx {

/// Saturation displays its actual 16 DSP steps (0-15) rather than the standard 0-50 menu range.
class Saturation final : public UnpatchedParam {
public:
	using UnpatchedParam::UnpatchedParam;

	[[nodiscard]] int32_t getMaxValue() const override { return 15; }

	void readCurrentValue() override {
		int32_t value = soundEditor.currentParamManager->getUnpatchedParamSet()->getValue(getP());
		// Same mapping the DSP uses, so the menu is 1:1 with the audible steps
		this->setValue(((uint32_t)value + 2147483648u) >> 28);
	}

	int32_t getFinalValue() override {
		if (this->getValue() >= getMaxValue()) {
			return 2147483647;
		}
		return (int32_t)(((uint32_t)this->getValue() << 28) - 2147483648u);
	}
};

} // namespace deluge::gui::menu_item::fx
