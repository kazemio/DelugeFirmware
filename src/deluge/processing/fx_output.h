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

#pragma once

#include "definitions_cxx.hpp"
#include "model/output.h"

class Song;
class ModelStackWithTimelineCounter;

/// The (single) FX track of a Song. Produces no sound of its own: its clips (FXClips) carry automation for the
/// song's master output params, so its ModControllable is the Song's GlobalEffectableForSong rather than anything
/// owned here. The master render path reads the active FXClip's ParamManager via Song::getActiveMasterParamManager().
class FXOutput final : public Output {
public:
	explicit FXOutput(Song& song);

	void renderOutput(ModelStack* modelStack, std::span<StereoSample> buffer, int32_t* reverbBuffer,
	                  int32_t reverbAmountAdjust, int32_t sideChainHitPending, bool shouldLimitDelayFeedback,
	                  bool isClipActive) override {
		// FX clips make no sound - their params are applied to the master output in Song::renderAudio /
		// AudioEngine::renderSongFX.
	}

	bool matchesPreset(OutputType otherType, int32_t channel, int32_t channelSuffix, char const* otherName,
	                   char const* dirPath) override {
		return false;
	}

	ModControllable* toModControllable() override;

	Error readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) override;
	bool writeDataToFile(Serializer& writer, Clip* clipForSavingOutputOnly, Song* song) override;
	void deleteBackedUpParamManagers(Song* song) override;

	ModelStackWithAutoParam* getModelStackWithParam(ModelStackWithTimelineCounter* modelStack, Clip* clip,
	                                                int32_t paramID, deluge::modulation::params::Kind paramKind,
	                                                bool affectEntire, bool useMenuStack) override;

	bool wantsToBeginArrangementRecording() override { return false; }

	char const* getXMLTag() override { return "fxTrack"; }

protected:
	Clip* createNewClipForArrangementRecording(ModelStack* modelStack) override { return nullptr; }

private:
	Song& song;
};
