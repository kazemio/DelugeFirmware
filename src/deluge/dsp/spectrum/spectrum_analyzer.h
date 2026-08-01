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

#include "dsp/stereo_sample.h"
#include <cstdint>
#include <span>

class ModControllableAudio;

namespace deluge::hid::display::oled_canvas {
class Canvas;
}

/// Singleton spectrum analyzer for the SPECTRUM menu screen.
///
/// The audio thread feeds it via maybeFeed() from the per-output post-FX buffers; while the screen
/// is closed target_ is nullptr so the cost is one pointer compare per output per render window.
/// All FFT and display math runs on the UI side (menu item timer callback). No locking is needed:
/// audio rendering and UI timers share one core under the cooperative scheduler and never preempt
/// each other mid-function.
class SpectrumAnalyzer {
public:
	static constexpr int32_t kFFTMagnitude = 10;
	static constexpr int32_t kFFTSize = 1 << kFFTMagnitude;
	static constexpr int32_t kNumBins = kFFTSize / 2 + 1;
	static constexpr int32_t kNumBars = 32;
	static constexpr int32_t kBarWidthPx = 3;
	static constexpr int32_t kBarPitchPx = 4;    // 32 bars * 4px = full 128px width
	static constexpr int32_t kMaxBarHeight = 43; // visible OLED rows (y5..y47)

	// AUDIO THREAD: called at the per-output tap sites. `source` is never nullptr, so when the
	// analyzer is inactive (target_ == nullptr) this is a single failed compare.
	[[gnu::always_inline]] inline void maybeFeed(ModControllableAudio const* source, std::span<StereoSample> audio) {
		if (source == target_) [[unlikely]] {
			feed(audio);
		}
	}

	// UI THREAD
	void setTarget(ModControllableAudio* target);
	void clearIfTarget(ModControllableAudio const* modControllable);
	void runFFTAndUpdateBars();
	void renderToCanvas(deluge::hid::display::oled_canvas::Canvas& canvas) const;
	int32_t barHeight(int32_t bar) const { return barHeights_[bar]; }
	int32_t peakHeight(int32_t bar) const { return peakHeights_[bar]; }

private:
	void feed(std::span<StereoSample> audio);
	void decayBarsAndPeaks();

	ModControllableAudio* volatile target_ = nullptr;
	volatile uint32_t ringWriteIndex_ = 0; // free-running; mask with kFFTSize - 1
	volatile uint32_t samplesFedSinceLastFrame_ = 0;
	q31_t ring_[kFFTSize];
	int32_t fftInput_[kFFTSize];
	int32_t
	    fftOutput_[2 * kNumBins]; // really ne10_fft_cpx_int32_t[kNumBins]; kept raw so NE10.h stays out of this header
	uint8_t barHeights_[kNumBars] = {0};
	uint8_t peakHeights_[kNumBars] = {0};
	uint8_t peakFallCounter_ = 0;
};

extern SpectrumAnalyzer spectrumAnalyzer;
