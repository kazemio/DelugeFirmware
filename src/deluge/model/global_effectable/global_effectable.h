/*
 * Copyright © 2016-2023 Synthstrom Audible Limited
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

#include "definitions_cxx.hpp"
#include "dsp/filter/filter_set.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include "modulation/arpeggiator.h"
#include "util/functions.h"
using namespace deluge;
class Serializer;

class GlobalEffectable : public ModControllableAudio {
public:
	GlobalEffectable();
	void cloneFrom(ModControllableAudio* other) override;

	static void initParams(ParamManager* paramManager);
	static void initParamsForAudioClip(ParamManagerForTimeline* paramManager);
	void modButtonAction(uint8_t whichModButton, bool on, ParamManagerForTimeline* paramManager) override;
	bool modEncoderButtonAction(uint8_t whichModEncoder, bool on, ModelStackWithThreeMainThings* modelStack) override;
	ModelStackWithAutoParam* getParamFromModEncoder(int32_t whichModEncoder, ModelStackWithThreeMainThings* modelStack,
	                                                bool allowCreation = true) override;
	void setupFilterSetConfig(int32_t* postFXVolume, ParamManager* paramManager);
	void processFilters(std::span<StereoSample> buffer);

	int32_t getShiftAmountForSaturation() { return (clippingAmount >= 3) ? (clippingAmount - 3) : 0; }

	/// clipping amount must be greater than 0! Check before calling
	/// Shift amount is givben by getShiftAmountForSaturation
	[[gnu::always_inline]] q31_t saturate(q31_t data, uint32_t* workingValue, int32_t shiftAmount) {
		// Clipping
		return getTanHAntialiased(data, workingValue, 3 + clippingAmount) << (shiftAmount);
	}

	/// Refreshes clippingAmount from the UNPATCHED_SATURATION param, then saturates the buffer if it's non-zero
	void processSaturation(std::span<StereoSample> buffer, ParamManager* paramManager);

	std::array<uint32_t, 2> lastSaturationTanHWorkingValue = {2147483648u, 2147483648u};
	void compensateVolumeForResonance(ParamManagerForTimeline* paramManager);
	void processFXForGlobalEffectable(std::span<StereoSample> buffer, int32_t* postFXVolume, ParamManager* paramManager,
	                                  const Delay::State& delayWorkingState, bool anySoundComingIn, q31_t verbAmount);

	void writeAttributesToFile(Serializer& writer, bool writeToFile);
	void writeTagsToFile(Serializer& writer, ParamManager* paramManager, bool writeToFile);
	Error readTagFromFile(Deserializer& reader, char const* tagName, ParamManagerForTimeline* paramManager,
	                      int32_t readAutomationUpToPos, ArpeggiatorSettings* arpSettings, Song* song) override;
	static void writeParamAttributesToFile(Serializer& writer, ParamManager* paramManager, bool writeAutomation,
	                                       int32_t* valuesForOverride = nullptr);
	static void writeParamTagsToFile(Serializer& writer, ParamManager* paramManager, bool writeAutomation,
	                                 int32_t* valuesForOverride = nullptr);
	static void readParamsFromFile(Deserializer& reader, ParamManagerForTimeline* paramManager,
	                               int32_t readAutomationUpToPos);
	static bool readParamTagFromFile(Deserializer& reader, char const* tagName, ParamManagerForTimeline* paramManager,
	                                 int32_t readAutomationUpToPos);
	Delay::State createDelayWorkingState(ParamManager& paramManager, bool shouldLimitDelayFeedback = false,
	                                     bool soundComingIn = true);
	bool isEditingComp() override { return editingComp; }
	int32_t getKnobPosForNonExistentParam(int32_t whichModEncoder, ModelStackWithAutoParam* modelStack) override;
	ActionResult modEncoderActionForNonExistentParam(int32_t offset, int32_t whichModEncoder,
	                                                 ModelStackWithAutoParam* modelStack) override;
	dsp::filter::FilterSet filterSet;
	ModFXParam currentModFXParam;
	FilterType currentFilterType;
	bool editingComp;
	CompParam currentCompParam;

	ModFXType getModFXType() override;

protected:
	int maxCompParam = 0;
	virtual int32_t getParameterFromKnob(int32_t whichModEncoder);
	ModFXType getActiveModFXType(ParamManager* paramManager);

private:
	void ensureModFXParamIsValid();
	void displayCompressorAndReverbSettings(bool on);
	char const* getCompressorModeDisplayName();
	char const* getCompressorParamDisplayName();
	void displayModFXSettings(bool on);
	char const* getModFXTypeDisplayName();
	char const* getModFXParamDisplayName();
};
