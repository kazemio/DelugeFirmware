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
#include "gui/views/view.h"
#include "hid/display/oled.h"
#include "model/clip/instrument_clip.h"
#include "model/drum/drum.h"
#include "model/instrument/kit.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "processing/sound/sound.h"
#include "util/functions.h"

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
// LFO/mod-FX rate final values are per-sample increments of a uint32 phase (modulation/lfo.h,
// ModFXProcessor), so Hz = value * fs / 2^32.
constexpr float kPhasePerSample = (float)kSampleRate / 4294967296.0f;
// Envelope stages complete when pos (advanced by finalValue per sample) reaches 2^23
// (modulation/envelope.cpp).
constexpr float kEnvelopeStageLength = 8388608.0f;
// Unsynced arp: gatePos += (rate >> 5) >> 8 per sample, one note per 2^24 (arpeggiator.cpp), so
// notes/sec = rate * fs / 2^37.
constexpr float kArpNotesPerSample = (float)kSampleRate / 137438953472.0f;

enum class FreqParam {
	NONE,
	LPF_PATCHED,
	HPF_PATCHED,
	LPF_GLOBAL,
	HPF_GLOBAL,
	BASS,
	TREBLE,
	LFO_RATE,
	MOD_FX_RATE,
	ARP_RATE,
	ENV_ATTACK,
	ENV_DECAY,
	ENV_RELEASE,
};

enum class Unit { HZ, SECONDS };

FreqParam identify(params::Kind kind, int32_t paramID) {
	if (kind == params::Kind::PATCHED) {
		switch (paramID) {
		case params::LOCAL_LPF_FREQ:
			return FreqParam::LPF_PATCHED;
		case params::LOCAL_HPF_FREQ:
			return FreqParam::HPF_PATCHED;
		case params::GLOBAL_LFO_FREQ_1:
		case params::GLOBAL_LFO_FREQ_2:
		case params::LOCAL_LFO_LOCAL_FREQ_1:
		case params::LOCAL_LFO_LOCAL_FREQ_2:
			return FreqParam::LFO_RATE;
		case params::GLOBAL_MOD_FX_RATE:
			return FreqParam::MOD_FX_RATE;
		case params::GLOBAL_ARP_RATE:
			return FreqParam::ARP_RATE;
		default:
			if (paramID >= params::LOCAL_ENV_0_ATTACK && paramID <= params::LOCAL_ENV_3_ATTACK) {
				return FreqParam::ENV_ATTACK;
			}
			if (paramID >= params::LOCAL_ENV_0_DECAY && paramID <= params::LOCAL_ENV_3_DECAY) {
				return FreqParam::ENV_DECAY;
			}
			if (paramID >= params::LOCAL_ENV_0_RELEASE && paramID <= params::LOCAL_ENV_3_RELEASE) {
				return FreqParam::ENV_RELEASE;
			}
			break;
		}
	}
	else if (kind == params::Kind::UNPATCHED_SOUND || kind == params::Kind::UNPATCHED_GLOBAL) {
		if (paramID == params::UNPATCHED_BASS_FREQ) {
			return FreqParam::BASS;
		}
		if (paramID == params::UNPATCHED_TREBLE_FREQ) {
			return FreqParam::TREBLE;
		}
		// Ids below exist only in the GlobalEffectable section and would collide with sound-only
		// unpatched ids, so require the GLOBAL kind for them
		if (kind == params::Kind::UNPATCHED_GLOBAL) {
			switch (paramID) {
			case params::UNPATCHED_LPF_FREQ:
				return FreqParam::LPF_GLOBAL;
			case params::UNPATCHED_HPF_FREQ:
				return FreqParam::HPF_GLOBAL;
			case params::UNPATCHED_MOD_FX_RATE:
				return FreqParam::MOD_FX_RATE;
			case params::UNPATCHED_ARP_RATE:
				return FreqParam::ARP_RATE;
			default:
				break;
			}
		}
	}
	return FreqParam::NONE;
}

// LFO and arp rates are only meaningful in Hz while free-running; when tempo-synced the display
// would be wrong, so the reading is suppressed.
bool isFreeRunning(FreqParam which, params::Kind kind, int32_t paramID) {
	if (which == FreqParam::LFO_RATE) {
		// Patched LFO params only exist on Sounds, so the active ModControllable is a Sound
		auto* sound = static_cast<Sound*>(view.activeModControllableModelStack.modControllable);
		if (sound == nullptr) {
			return false;
		}
		LFO_ID lfoId;
		switch (paramID) {
		case params::GLOBAL_LFO_FREQ_1:
			lfoId = LFO1_ID;
			break;
		case params::LOCAL_LFO_LOCAL_FREQ_1:
			lfoId = LFO2_ID;
			break;
		case params::GLOBAL_LFO_FREQ_2:
			lfoId = LFO3_ID;
			break;
		default:
			lfoId = LFO4_ID;
			break;
		}
		return sound->lfoConfig[lfoId].syncLevel == SYNC_LEVEL_NONE;
	}
	if (which == FreqParam::ARP_RATE) {
		Clip* clip = getCurrentClip();
		if (clip == nullptr || clip->type != ClipType::INSTRUMENT) {
			return false;
		}
		auto* instrumentClip = (InstrumentClip*)clip;
		ArpeggiatorSettings* arpSettings;
		if (clip->output->type == OutputType::KIT && kind == params::Kind::PATCHED) {
			// Kit row: the selected drum's arp
			Drum* drum = ((Kit*)clip->output)->selectedDrum;
			arpSettings = (drum != nullptr) ? &drum->arpSettings : nullptr;
		}
		else { // synth / MIDI / CV, and kit-global (Kit::getArpSettings resolves to the clip's too)
			arpSettings = &instrumentClip->arpSettings;
		}
		return arpSettings != nullptr && arpSettings->syncLevel == SYNC_LEVEL_NONE;
	}
	return true;
}

// Float mirror of the firmware's own value chains, display only: getExp/paramNeutralValues
// (util/functions.cpp), patched preset scaling by paramRanges (modulation/patch/patcher.cpp),
// unpatched cableToExpParamShortcut (>>2), the dumb envelope hack (attack sign flip,
// decay/release via lookupReleaseRate), EQ one-pole setup (mod_controllable_audio.cpp).
float computeValue(FreqParam which, float paramValue, Unit& unit) {
	unit = Unit::HZ;

	// getExp adjustment per raw param unit: default patched paramRange 2^30 and the unpatched
	// >>2 shortcut are both value/4; exceptions below
	float adjustment = paramValue * 0.25f;
	float neutral;
	switch (which) {
	case FreqParam::LPF_PATCHED: // paramRanges[LOCAL_LPF_FREQ] = 2^29 * 1.4
		adjustment = paramValue * (536870912.0f * 1.4f / 4294967296.0f);
		neutral = 2000000.0f;
		break;
	case FreqParam::HPF_PATCHED:
	case FreqParam::HPF_GLOBAL:
		neutral = 2672947.0f;
		break;
	case FreqParam::LPF_GLOBAL:
		neutral = 2000000.0f;
		break;
	case FreqParam::BASS: // (value >> 5) * 6
		adjustment = paramValue * (6.0f / 32.0f);
		neutral = 120000000.0f;
		break;
	case FreqParam::TREBLE:
		adjustment = paramValue * (6.0f / 32.0f);
		neutral = 700000000.0f;
		break;
	case FreqParam::LFO_RATE:
	case FreqParam::MOD_FX_RATE:
		neutral = 121739.0f;
		break;
	case FreqParam::ARP_RATE:
		neutral = (float)kMaxSampleValue;
		break;
	case FreqParam::ENV_ATTACK: // paramRanges = 2^29 * 1.5; the dumb envelope hack negates
		adjustment = -paramValue * (536870912.0f * 1.5f / 4294967296.0f);
		neutral = 4096.0f;
		break;
	case FreqParam::ENV_DECAY:
	case FreqParam::ENV_RELEASE: {
		// finalValue = neutral * lookupReleaseRate(adjustment) / 2^32 (the dumb envelope hack)
		float envNeutral = (which == FreqParam::ENV_DECAY) ? 35840.0f : 71680.0f;
		auto adjInt = (int32_t)std::clamp(paramValue * 0.25f, -2147483648.0f, 2147483520.0f);
		float rate = envNeutral * ((float)lookupReleaseRate(adjInt) / 4294967296.0f);
		unit = Unit::SECONDS;
		return kEnvelopeStageLength / (std::max(rate, 1.0f) * (float)kSampleRate);
	}
	default:
		return 0.0f;
	}

	float value = std::min(neutral * std::exp2(adjustment / 67108864.0f), 2147483647.0f);
	switch (which) {
	case FreqParam::BASS:
	case FreqParam::TREBLE: { // one-pole smoothing coefficient (mod_controllable_audio.cpp)
		float coefficient = value / 4294967296.0f;
		return -std::log(1.0f - coefficient) * (float)kSampleRate / (2.0f * 3.14159265f);
	}
	case FreqParam::LFO_RATE:
	case FreqParam::MOD_FX_RATE:
		return value * kPhasePerSample;
	case FreqParam::ARP_RATE:
		return value * kArpNotesPerSample;
	case FreqParam::ENV_ATTACK:
		unit = Unit::SECONDS;
		return kEnvelopeStageLength / (std::max(value, 1.0f) * (float)kSampleRate);
	default: // filters: full-scale q31 (after <<5) maps to 20.2 kHz via the tan table
		return std::min(value * 32.0f, kQ31) / kQ31 * kMaxFilterHz;
	}
}

void appendUnitValue(float value, Unit unit, StringBuf& buf, bool compact) {
	if (unit == Unit::SECONDS) {
		if (value < 0.0095f) {
			buf.appendFloat(value * 1000.0f, 1, 1);
			buf.append(" ms");
		}
		else if (value < 0.9995f) {
			buf.appendInt((int32_t)(value * 1000.0f + 0.5f));
			buf.append(" ms");
		}
		else if (value < 9.95f) {
			buf.appendFloat(value, 1, 1);
			buf.append(" s");
		}
		else {
			buf.appendInt((int32_t)(value + 0.5f));
			buf.append(" s");
		}
		return;
	}
	if (value < 9.995f) {
		buf.appendFloat(value, 1, 2);
		if (!compact) {
			buf.append(" Hz");
		}
	}
	else if (value < 999.5f) {
		buf.appendInt((int32_t)(value + 0.5f));
		if (!compact) {
			buf.append(" Hz");
		}
	}
	else if (value < 9950.0f) {
		int32_t tenthsOfKhz = (int32_t)(value / 100.0f + 0.5f);
		buf.appendInt(tenthsOfKhz / 10);
		buf.append('.');
		buf.appendInt(tenthsOfKhz % 10);
		buf.append(compact ? "k" : " kHz");
	}
	else {
		buf.appendInt((int32_t)(value / 1000.0f + 0.5f));
		buf.append(compact ? "k" : " kHz");
	}
}

void appendForParamValue(FreqParam which, float paramValue, StringBuf& buf, bool compact) {
	Unit unit;
	float value = computeValue(which, paramValue, unit);
	appendUnitValue(value, unit, buf, compact);
}

float menuValueToParamValue(int32_t menuValue) {
	return (float)menuValue * 85899345.0f - 2147483648.0f; // computeFinalValueForStandardMenuItem
}

} // namespace

bool shouldShowHz(params::Kind kind, int32_t paramID) {
	if (!runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::ShowRealUnits)) {
		return false;
	}
	FreqParam which = identify(kind, paramID);
	return which != FreqParam::NONE && isFreeRunning(which, kind, paramID);
}

void appendHzForMenuValue(params::Kind kind, int32_t paramID, int32_t menuValue, StringBuf& buf, bool compact) {
	appendForParamValue(identify(kind, paramID), menuValueToParamValue(menuValue), buf, compact);
}

void appendHzForKnobPos(params::Kind kind, int32_t paramID, int32_t knobPos, StringBuf& buf, bool compact) {
	appendForParamValue(identify(kind, paramID), (float)(knobPos - 64) * 33554432.0f, buf, compact);
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
