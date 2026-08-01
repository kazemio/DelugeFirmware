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

#include "gui/ui/spectrum_overlay.h"
#include "dsp/spectrum/spectrum_analyzer.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/automation_view.h"
#include "hid/display/display.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/kit.h"
#include "model/output.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "processing/sound/sound_drum.h"

namespace deluge::gui::spectrum_overlay {

namespace {

constexpr int32_t kRefreshMs = 35;

enum class Mode : uint8_t {
	normal,
	spectrum,
	displayOff,
};

Mode mode = Mode::normal;

ModControllableAudio* contextTarget() {
	if (currentSong == nullptr) {
		return nullptr;
	}
	if (rootUIIsClipMinderScreen()) {
		Output* output = getCurrentOutput();
		if (output != nullptr) {
			// Kits follow AFFECT ENTIRE like the rest of the UI: with it off, analyze just the
			// selected drum row (falling back to the whole kit for MIDI/gate drums or no selection).
			if (output->type == OutputType::KIT) {
				InstrumentClip* clip = getCurrentInstrumentClip();
				Kit* kit = (Kit*)output;
				if (clip != nullptr && !clip->affectEntire && kit->selectedDrum != nullptr
				    && kit->selectedDrum->type == DrumType::SOUND) {
					return (SoundDrum*)kit->selectedDrum;
				}
			}
			// Only audio-producing outputs are ModControllableAudio (a MIDI/CV output's
			// toModControllable() returns a plain ModControllable).
			if (output->type == OutputType::SYNTH || output->type == OutputType::KIT
			    || output->type == OutputType::AUDIO) {
				auto* modControllable = (ModControllableAudio*)output->toModControllable();
				if (modControllable != nullptr) {
					return modControllable;
				}
			}
		}
		// MIDI/CV clips produce no audio of their own - show nothing rather than something unrelated.
		return nullptr;
	}
	return &currentSong->globalEffectable;
}

// Both takeover modes only apply on a root view (menus and browsers render normally), never in
// automation view, and only on OLED hardware. Checking the community toggle here too means
// disabling the feature mid-overlay restores the normal display instead of stranding the screen
// (the combo is inert while disabled, so it couldn't cycle back out).
bool takeoverAllowedNow() {
	return display->haveOLED() && getCurrentUI() == getRootUI() && getRootUI() != &automationView
	       && runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::EnableSpectrumAnalyzer);
}

} // namespace

bool shouldShowSpectrum() {
	return mode == Mode::spectrum && takeoverAllowedNow();
}

bool shouldBlankDisplay() {
	return mode == Mode::displayOff && takeoverAllowedNow();
}

void timerEvent() {
	if (mode != Mode::spectrum) {
		return;
	}
	uiTimerManager.setTimer(TimerName::SPECTRUM_OVERLAY, kRefreshMs);
	if (!shouldShowSpectrum()) {
		return; // a menu, browser or automation view owns the screen right now; resume when it closes
	}
	spectrumAnalyzer.setTarget(contextTarget()); // idempotent; follows clip/view switches
	spectrumAnalyzer.runFFTAndUpdateBars();
	renderUIsForOled();
}

void toggle() {
	switch (mode) {
	case Mode::normal:
		mode = Mode::spectrum;
		spectrumAnalyzer.setTarget(contextTarget());
		uiTimerManager.setTimer(TimerName::SPECTRUM_OVERLAY, kRefreshMs);
		break;

	case Mode::spectrum:
		mode = Mode::displayOff;
		uiTimerManager.unsetTimer(TimerName::SPECTRUM_OVERLAY);
		spectrumAnalyzer.setTarget(nullptr);
		break;

	case Mode::displayOff:
		mode = Mode::normal;
		break;
	}
	// Repaint whichever state we've just entered. Every OLED send is the whole frame, so leaving
	// displayOff fully restores the current view's screen in one go.
	renderUIsForOled();
}

} // namespace deluge::gui::spectrum_overlay
