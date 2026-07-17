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
#include "gui/views/audio_clip_view.h"
#include "model/clip/clip.h"
#include "util/d_string.h"

class ModelStackWithTimelineCounter;

/// A Clip that produces no sound and accepts no notes: it only carries automation for the song's master output
/// params (the UNPATCHED_GLOBAL set). Lives on the song's single FXOutput. While an FXClip is active, the master
/// render path uses its ParamManager in place of the Song's own (see Song::getActiveMasterParamManager()).
class FXClip final : public Clip {
public:
	FXClip();

	Error clone(ModelStackWithTimelineCounter* modelStack, bool shouldFlattenReversing = false) const override;
	void expectNoFurtherTicks(Song* song, bool actuallySoundChange = true) override;
	void resumePlayback(ModelStackWithTimelineCounter* modelStack, bool mayMakeSound = true) override;
	void detachFromOutput(ModelStackWithTimelineCounter* modelStack, bool shouldRememberDrumName,
	                      bool shouldDeleteEmptyNoteRowsAtEndOfList = false, bool shouldRetainLinksToSounds = false,
	                      bool keepNoteRowsWithMIDIInput = true, bool shouldGrabMidiCommands = false,
	                      bool shouldBackUpExpressionParamsToo = true) override;
	bool renderAsSingleRow(ModelStackWithTimelineCounter* modelStack, TimelineView* editorScreen, int32_t xScroll,
	                       uint32_t xZoom, RGB* image, uint8_t occupancyMask[], bool addUndefinedArea,
	                       int32_t noteRowIndexStart = 0, int32_t noteRowIndexEnd = 2147483647, int32_t xStart = 0,
	                       int32_t xEnd = kDisplayWidth, bool allowBlur = true, bool drawRepeats = false) override;
	Error claimOutput(ModelStackWithTimelineCounter* modelStack) override;
	Error setOutput(ModelStackWithTimelineCounter* modelStack, Output* newOutput,
	                FXClip* favourClipForCloningParamManager = nullptr);
	void setPos(ModelStackWithTimelineCounter* modelStack, int32_t newPos, bool useActualPosForParamManagers) override;
	bool shiftHorizontally(ModelStackWithTimelineCounter* modelStack, int32_t amount, bool shiftAutomation,
	                       bool shiftSequenceAndMPE) override;
	RGB getColour();

	bool wantsToBeginLinearRecording(Song* song) override { return false; }
	void finishLinearRecording(ModelStackWithTimelineCounter* modelStack, Clip* nextPendingLoop,
	                           int32_t buttonLatencyForTempolessRecord) override {}
	Error beginLinearRecording(ModelStackWithTimelineCounter* modelStack, int32_t buttonPressLatency) override {
		return Error::NONE;
	}
	Clip* cloneAsNewOverdub(ModelStackWithTimelineCounter* modelStack, OverDubType newOverdubNature) override {
		return nullptr;
	}
	bool getCurrentlyRecordingLinearly() override { return false; }
	bool currentlyScrollableAndZoomable() override { return true; }
	bool isAbandonedOverdub() override { return false; }
	void quantizeLengthForArrangementRecording(ModelStackWithTimelineCounter* modelStack, int32_t lengthSoFar,
	                                           uint32_t timeRemainder, int32_t suggestedLength,
	                                           int32_t alternativeLongerLength) override {}
	void abortRecording() override {}

	Error readFromFile(Deserializer& reader, Song* song) override;
	void writeDataToFile(Serializer& writer, Song* song) override;
	char const* getXMLTag() override { return "fxClip"; }

	bool renderSidebar(uint32_t whichRows = 0, RGB image[][kDisplayWidth + kSideBarWidth] = nullptr,
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth] = nullptr) override {
		return audioClipView.renderSidebar(whichRows, image, occupancyMask);
	}

	ParamManagerForTimeline* getCurrentParamManager() override { return &paramManager; }

	bool isEmpty(bool displayPopup = true) override;

protected:
	bool cloneOutput(ModelStackWithTimelineCounter* modelStack) override { return false; }
};
