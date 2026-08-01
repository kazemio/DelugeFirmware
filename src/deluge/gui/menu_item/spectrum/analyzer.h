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
#include "dsp/spectrum/spectrum_analyzer.h"
#include "gui/menu_item/menu_item.h"
#include "gui/ui/sound_editor.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/settings/runtime_feature_settings.h"
#include <algorithm>

namespace deluge::gui::menu_item::spectrum {

/// Full-screen live spectrum analyzer of the current output (synth / kit / single drum / audio
/// clip). The analyzer only runs while this screen is open: beginSession() points the singleton at
/// the current output and endSession() detaches it again.
class Analyzer final : public MenuItem {
public:
	using MenuItem::MenuItem;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) override {
		return display->haveOLED() && modControllable != nullptr
		       && runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::EnableSpectrumAnalyzer);
	}

	void beginSession(MenuItem* navigatedBackwardFrom = nullptr) override {
		spectrumAnalyzer.setTarget(soundEditor.currentModControllable);
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kRefreshMs);
	}

	void endSession() override {
		uiTimerManager.unsetTimer(TimerName::UI_SPECIFIC);
		spectrumAnalyzer.setTarget(nullptr);
		MenuItem::endSession();
	}

	ActionResult timerCallback() override {
		spectrumAnalyzer.runFFTAndUpdateBars();
		renderUIsForOled();
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kRefreshMs);
		return ActionResult::DEALT_WITH;
	}

	void renderOLED() override {
		spectrumAnalyzer.renderToCanvas(hid::display::OLED::main);
		hid::display::OLED::markChanged();
	}

private:
	static constexpr int32_t kRefreshMs = 35; // ~28Hz, matching the OLED pipeline's ~30Hz cadence
};

} // namespace deluge::gui::menu_item::spectrum
