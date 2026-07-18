/*
 * Copyright © 2019-2023 Synthstrom Audible Limited
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

#include "gui/ui/param_freq_display.h"

#include "definitions_cxx.hpp"
#include "hid/display/oled.h"
#include "model/settings/runtime_feature_settings.h"

#include <algorithm>
#include <cmath>

namespace params = deluge::modulation::params;

namespace deluge::gui::param_freq_display {

namespace {

// The filter DSP maps a full-scale (q31) frequency value linearly onto the angle range of its tan
// lookup, whose top entry is arctan-ed back to pi*f/fs (dsp/filter/filter.h curveFrequency +
// lookuptables tanTable): full scale = 20.2 kHz at 44.1 kHz.
constexpr float kMaxFilterHz = 20211.4f;
constexpr float kQ31 = 2147483648.0f;

enum class FreqParam { NONE, LPF_PATCHED, HPF_PATCHED, LPF_GLOBAL, HPF_GLOBAL, BASS, TREBLE };

FreqParam identify(params::Kind kind, int32_t paramID) {
	if (kind == params::Kind::PATCHED) {
		if (paramID == params::LOCAL_LPF_FREQ) {
			return FreqParam::LPF_PATCHED;
		}
		if (paramID == params::LOCAL_HPF_FREQ) {
			return FreqParam::HPF_PATCHED;
		}
	}
	else if (kind == params::Kind::UNPATCHED_SOUND || kind == params::Kind::UNPATCHED_GLOBAL) {
		if (paramID == params::UNPATCHED_BASS_FREQ) {
			return FreqParam::BASS;
		}
		if (paramID == params::UNPATCHED_TREBLE_FREQ) {
			return FreqParam::TREBLE;
		}
		// LPF/HPF ids below exist only in the GlobalEffectable section and would collide with
		// sound-only unpatched ids, so require the GLOBAL kind for them
		if (kind == params::Kind::UNPATCHED_GLOBAL) {
			if (paramID == params::UNPATCHED_LPF_FREQ) {
				return FreqParam::LPF_GLOBAL;
			}
			if (paramID == params::UNPATCHED_HPF_FREQ) {
				return FreqParam::HPF_GLOBAL;
			}
		}
	}
	return FreqParam::NONE;
}

// Float mirror of the firmware's own value chains, display only: getExp/paramNeutralValues
// (util/functions.cpp), patched preset scaling by paramRanges (modulation/patch/patcher.cpp),
// unpatched cableToExpParamShortcut (>>2), EQ one-pole setup (mod_controllable_audio.cpp).
float computeHz(FreqParam which, float paramValue) {
	float adjustment; // getExp units: 2^26 per octave
	float neutral;
	bool isOnePoleEq = false;

	switch (which) {
	case FreqParam::LPF_PATCHED: // preset scaled by paramRanges[LOCAL_LPF_FREQ] = 2^29 * 1.4
		adjustment = paramValue * (536870912.0f * 1.4f / 4294967296.0f);
		neutral = 2000000.0f;
		break;
	case FreqParam::HPF_PATCHED: // paramRanges 2^30, identical to the unpatched >>2
		adjustment = paramValue * 0.25f;
		neutral = 2672947.0f;
		break;
	case FreqParam::LPF_GLOBAL:
		adjustment = paramValue * 0.25f;
		neutral = 2000000.0f;
		break;
	case FreqParam::HPF_GLOBAL:
		adjustment = paramValue * 0.25f;
		neutral = 2672947.0f;
		break;
	case FreqParam::BASS: // (value >> 5) * 6
		adjustment = paramValue * (6.0f / 32.0f);
		neutral = 120000000.0f;
		isOnePoleEq = true;
		break;
	case FreqParam::TREBLE:
		adjustment = paramValue * (6.0f / 32.0f);
		neutral = 700000000.0f;
		isOnePoleEq = true;
		break;
	default:
		return 0.0f;
	}

	float value = std::min(neutral * std::exp2(adjustment / 67108864.0f), 2147483647.0f);
	if (isOnePoleEq) {
		float coefficient = value / 4294967296.0f;
		return -std::log(1.0f - coefficient) * (float)kSampleRate / (2.0f * 3.14159265f);
	}
	return std::min(value * 32.0f, kQ31) / kQ31 * kMaxFilterHz;
}

void appendHz(FreqParam which, float paramValue, StringBuf& buf, bool compact) {
	float hz = computeHz(which, paramValue);
	if (hz < 999.5f) {
		buf.appendInt((int32_t)(hz + 0.5f));
		if (!compact) {
			buf.append(" Hz");
		}
	}
	else if (hz < 9950.0f) {
		int32_t tenthsOfKhz = (int32_t)(hz / 100.0f + 0.5f);
		buf.appendInt(tenthsOfKhz / 10);
		buf.append('.');
		buf.appendInt(tenthsOfKhz % 10);
		buf.append(compact ? "k" : " kHz");
	}
	else {
		buf.appendInt((int32_t)(hz / 1000.0f + 0.5f));
		buf.append(compact ? "k" : " kHz");
	}
}

float menuValueToParamValue(int32_t menuValue) {
	return (float)menuValue * 85899345.0f - 2147483648.0f; // computeFinalValueForStandardMenuItem
}

} // namespace

bool shouldShowHz(params::Kind kind, int32_t paramID) {
	return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::FilterFrequencyDisplay)
	       && identify(kind, paramID) != FreqParam::NONE;
}

void appendHzForMenuValue(params::Kind kind, int32_t paramID, int32_t menuValue, StringBuf& buf, bool compact) {
	appendHz(identify(kind, paramID), menuValueToParamValue(menuValue), buf, compact);
}

void appendHzForKnobPos(params::Kind kind, int32_t paramID, int32_t knobPos, StringBuf& buf, bool compact) {
	appendHz(identify(kind, paramID), (float)(knobPos - 64) * 33554432.0f, buf, compact);
}

void drawMenuHzLine(params::Kind kind, int32_t paramID, int32_t menuValue) {
	if (!shouldShowHz(kind, paramID)) {
		return;
	}
	DEF_STACK_STRING_BUF(hzText, 12);
	appendHzForMenuValue(kind, paramID, menuValue, hzText);
	// Sits in the free band between the big value (ends y=28) and the horizontal bar (y=35)
	deluge::hid::display::OLED::main.drawStringCentred(hzText.c_str(), 28 + OLED_MAIN_TOPMOST_PIXEL, kTextSpacingX,
	                                                   kTextSizeYUpdated);
}

bool drawCompactHz(params::Kind kind, int32_t paramID, int32_t menuValue, int32_t startX, int32_t yPixel,
                   int32_t width) {
	if (!shouldShowHz(kind, paramID)) {
		return false;
	}
	DEF_STACK_STRING_BUF(hzText, 8);
	appendHzForMenuValue(kind, paramID, menuValue, hzText, true);
	deluge::hid::display::OLED::main.drawStringCentered(hzText.c_str(), startX, yPixel, kTextTitleSpacingX,
	                                                    kTextTitleSizeY, width);
	return true;
}

} // namespace deluge::gui::param_freq_display
