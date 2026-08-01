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

#include "dsp/spectrum/spectrum_analyzer.h"
#include "NE10.h"
#include "definitions.h"
#include "dsp/fft/fft_config_manager.h"
#include "hid/display/oled_canvas/canvas.h"
#include "util/fixedpoint.h"
#include "util/functions.h"
#include "util/lookuptables/lookuptables.h"
#include <algorithm>
#include <cstring>

PLACE_SDRAM_BSS SpectrumAnalyzer spectrumAnalyzer;

namespace {

// Pre-FFT attenuation of the Hann-windowed signal. The FFT runs unscaled (scaled_flag = false), so
// N=1024 needs 10 bits of growth headroom below the q31 signal (nominally 24-bit peak-to-peak).
constexpr int32_t kPreShift = 10;

// Display window: top edge is a full-scale sine's band level, spanning 48dB (log2 power range of
// 16) down to the floor. Both in quickLog() units (log2 << 25).
constexpr int32_t kLogTop = 41 << 25;
constexpr int32_t kLogRange = 16 << 25;

// Ballistics, per ~35ms frame: bars fall 2px, peak dots fall 1px every 3rd frame.
constexpr int32_t kBarFallPerFrame = 2;
constexpr int32_t kPeakFallFramePeriod = 3;

// Log-spaced band edges: bin 1 (43Hz) to bin 512 (Nyquist) spans exactly 9 octaves, so
// edge[b] = round(2^(9b/32)). Bottom bands share the lowest bins (standard for log analyzers).
constexpr uint16_t kBandEdges[SpectrumAnalyzer::kNumBars + 1] = {1,  1,  1,   2,   2,   3,   3,   4,   5,   6,   7,
                                                                 9,  10, 13,  15,  19,  23,  27,  33,  41,  49,  60,
                                                                 73, 89, 108, 131, 159, 193, 235, 285, 347, 421, 512};

} // namespace

void SpectrumAnalyzer::feed(std::span<StereoSample> audio) {
	uint32_t w = ringWriteIndex_;
	for (StereoSample sample : audio) {
		ring_[w++ & (kFFTSize - 1)] = (sample.l >> 1) + (sample.r >> 1);
	}
	ringWriteIndex_ = w;
	samplesFedSinceLastFrame_ += audio.size();
}

void SpectrumAnalyzer::setTarget(ModControllableAudio* target) {
	if (target == target_) {
		return; // idempotent - beginSession can be re-entered via focusRegained
	}
	// Clear the audio so no FFT ever runs on a blend of old and new source, and drop the peak dots
	// (a held peak from the old source would lie about the new one). The bars themselves carry
	// across: they decay normally for the frame or two the buffer takes to refill, then instant
	// attack snaps them to the new source's levels - no dip to zero on target switches.
	target_ = nullptr;
	memset(ring_, 0, sizeof(ring_));
	memset(peakHeights_, 0, sizeof(peakHeights_));
	ringWriteIndex_ = 0;
	samplesFedSinceLastFrame_ = 0;
	peakFallCounter_ = 0;
	target_ = target;
}

void SpectrumAnalyzer::clearIfTarget(ModControllableAudio const* modControllable) {
	if (target_ == modControllable) {
		target_ = nullptr;
	}
}

void SpectrumAnalyzer::decayBarsAndPeaks() {
	bool peaksFallThisFrame = (++peakFallCounter_ >= kPeakFallFramePeriod);
	if (peaksFallThisFrame) {
		peakFallCounter_ = 0;
	}
	for (int32_t b = 0; b < kNumBars; b++) {
		barHeights_[b] = (barHeights_[b] > kBarFallPerFrame) ? (barHeights_[b] - kBarFallPerFrame) : 0;
		if (peaksFallThisFrame && peakHeights_[b] > 0) {
			peakHeights_[b]--;
		}
	}
}

void SpectrumAnalyzer::renderToCanvas(deluge::hid::display::oled_canvas::Canvas& canvas) const {
	constexpr int32_t bottomY = OLED_MAIN_HEIGHT_PIXELS - 1;
	// Always-visible baseline, so a silent spectrum is distinguishable from the display-off state.
	canvas.drawHorizontalLine(bottomY, 0, OLED_MAIN_WIDTH_PIXELS - 1);
	for (int32_t b = 0; b < kNumBars; b++) {
		int32_t x = b * kBarPitchPx;
		int32_t barHeight = barHeights_[b];
		if (barHeight > 0) {
			for (int32_t i = 0; i < kBarWidthPx; i++) {
				canvas.drawVerticalLine(x + i, bottomY + 1 - barHeight, bottomY);
			}
		}
		int32_t peakHeight = peakHeights_[b];
		if (peakHeight > barHeight) {
			int32_t peakY = std::max<int32_t>(OLED_MAIN_TOPMOST_PIXEL, bottomY - peakHeight);
			canvas.drawHorizontalLine(peakY, x, x + kBarWidthPx - 1);
		}
	}
}

void SpectrumAnalyzer::runFFTAndUpdateBars() {
	uint32_t samplesFed = samplesFedSinceLastFrame_;
	samplesFedSinceLastFrame_ = 0;

	decayBarsAndPeaks();

	// If the target barely rendered since last frame (silent output skipping rendering), skip the
	// FFT and just let the bars fall. No CPU-load gate: the whole analysis is well under 1ms per
	// ~35ms frame and runs in a UI-priority task the scheduler won't start if audio is at risk -
	// gating on cpuDireness made the display die exactly on the busy tracks it's most useful for.
	if (samplesFed < kFFTSize / 4) {
		return;
	}

	// Hann-window the newest kFFTSize samples. (w + i) == (w - kFFTSize + i) mod kFFTSize.
	uint32_t w = ringWriteIndex_;
	for (int32_t i = 0; i < kFFTSize; i++) {
		q31_t sample = ring_[(w + i) & (kFFTSize - 1)];
		int32_t hanningValue = interpolateTableSigned(i, kFFTMagnitude, hanningWindow, 8);
		fftInput_[i] = multiply_32x32_rshift32_rounded(sample, hanningValue) >> kPreShift;
	}

	ne10_fft_r2c_1d_int32_neon((ne10_fft_cpx_int32_t*)fftOutput_, fftInput_, FFTConfigManager::getConfig(kFFTMagnitude),
	                           false);

	for (int32_t b = 0; b < kNumBars; b++) {
		int32_t firstBin = kBandEdges[b];
		int32_t binsEnd = std::max<int32_t>(firstBin + 1, kBandEdges[b + 1]);

		// Max (not sum) squared magnitude over the band, so wide high bands don't tower over bass.
		uint64_t maxMagSquared = 0;
		for (int32_t bin = firstBin; bin < binsEnd; bin++) {
			int64_t re = fftOutput_[2 * bin];
			int64_t im = fftOutput_[2 * bin + 1];
			uint64_t magSquared = (uint64_t)(re * re + im * im);
			maxMagSquared = std::max(maxMagSquared, magSquared);
		}
		if (maxMagSquared == 0) {
			continue;
		}

		int32_t logMag;
		if (maxMagSquared >> 32) {
			logMag = quickLog((uint32_t)(maxMagSquared >> 32)) + (32 << 25);
		}
		else {
			logMag = quickLog((uint32_t)maxMagSquared);
		}

		int32_t height = (int32_t)((int64_t)kMaxBarHeight * (logMag - (kLogTop - kLogRange)) / kLogRange);
		height = std::clamp<int32_t>(height, 0, kMaxBarHeight);

		// Instant attack; decay already applied above.
		barHeights_[b] = std::max<uint8_t>(barHeights_[b], height);
		peakHeights_[b] = std::max<uint8_t>(peakHeights_[b], barHeights_[b]);
	}
}
