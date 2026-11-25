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
#include "gui/menu_item/formatted_title.h"
#include "gui/menu_item/unpatched_param.h"
#include "model/sync.h"

namespace deluge::gui::menu_item::lfo {

// LFO sync, backed by the UNPATCHED_LFOn_SYNC params (which makes it automatable and CC-controllable).
// The menu value is the "sync value" 0..NUM_SYNC_VALUES-1 (0 = OFF, i.e. free-running rate param);
// see lfoSyncValueToParamValue() for the stored full-range encoding.
class Sync final : public UnpatchedParam, public FormattedTitle {
public:
	Sync(l10n::String name, l10n::String title, uint8_t lfoId)
	    : UnpatchedParam(name, title, deluge::modulation::params::UNPATCHED_LFO1_SYNC + lfoId),
	      FormattedTitle(title, lfoId + 1), lfoId_(lfoId) {}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] int32_t getMaxValue() const override { return NUM_SYNC_VALUES - 1; }

	void readCurrentValue() override;
	void getColumnLabel(StringBuf& label) override;
	void renderInHorizontalMenu(const SlotPosition& slot) override;
	void getNotificationValue(StringBuf& value) override;

protected:
	int32_t getFinalValue() override;
	void drawValue() override;
	void drawPixelsForOled() override;

private:
	void getNoteLengthName(StringBuf& buffer);
	uint8_t lfoId_;
};

} // namespace deluge::gui::menu_item::lfo
