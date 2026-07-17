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

#include "model/clip/fx_clip.h"
#include "gui/colour/colour.h"
#include "memory/general_memory_allocator.h"
#include "model/global_effectable/global_effectable.h"
#include "model/model_stack.h"
#include "model/output.h"
#include "model/song/song.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/fx_output.h"
#include "storage/storage_manager.h"
#include "util/misc.h"
#include <algorithm>
#include <cstring>
#include <new>

FXClip::FXClip() : Clip(ClipType::FX) {
	// An FX clip IS automation - it always opens into automation view
	onAutomationClipView = true;
}

// Will replace the Clip in the modelStack, if success.
Error FXClip::clone(ModelStackWithTimelineCounter* modelStack, bool shouldFlattenReversing) const {

	void* clipMemory = GeneralMemoryAllocator::get().allocMaxSpeed(sizeof(FXClip));
	if (!clipMemory) {
		return Error::INSUFFICIENT_RAM;
	}

	auto newClip = new (clipMemory) FXClip();

	newClip->copyBasicsFrom(this);
	Error error = newClip->paramManager.cloneParamCollectionsFrom(&paramManager, true);
	if (error != Error::NONE) {
		newClip->~FXClip();
		delugeDealloc(clipMemory);
		return error;
	}

	modelStack->setTimelineCounter(newClip);

	newClip->activeIfNoSolo = false;
	newClip->soloingInSessionMode = false;
	newClip->output = output;

	return Error::NONE;
}

void FXClip::expectNoFurtherTicks(Song* song, bool actuallySoundChange) {

	// If it's actually another Clip, that we're recording into the arranger...
	if (output->getActiveClip() && output->getActiveClip()->beingRecordedFromClip == this) {
		output->getActiveClip()->expectNoFurtherTicks(song, actuallySoundChange);
		return;
	}

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithTimelineCounter* modelStack = setupModelStackWithTimelineCounter(modelStackMemory, song, this);

	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addOtherTwoThingsButNoNoteRow(output->toModControllable(), &paramManager);

	if (paramManager.containsAnyParamCollectionsIncludingExpression()) {
		paramManager.expectNoFurtherTicks(modelStackWithThreeMainThings);
	}
}

void FXClip::resumePlayback(ModelStackWithTimelineCounter* modelStack, bool mayMakeSound) {
	setPosForParamManagers(modelStack, true);
}

// Can assume there always was an old Output to begin with
// Does not dispose of the old Output - the caller has to do this
void FXClip::detachFromOutput(ModelStackWithTimelineCounter* modelStack, bool shouldRememberDrumNames,
                              bool shouldDeleteEmptyNoteRowsAtEitherEnd, bool shouldRetainLinksToOutput,
                              bool keepNoteRowsWithMIDIInput, bool shouldGrabMidiCommands,
                              bool shouldBackUpExpressionParamsToo) {
	if (isActiveOnOutput()) {
		output->detachActiveClip(modelStack->song);
	}

	modelStack->song->backUpParamManager((ModControllableAudio*)output->toModControllable(), this, &paramManager);

	if (!shouldRetainLinksToOutput) {
		output = nullptr;
	}
}

bool FXClip::renderAsSingleRow(ModelStackWithTimelineCounter* modelStack, TimelineView* editorScreen, int32_t xScroll,
                               uint32_t xZoom, RGB* image, uint8_t occupancyMask[], bool addUndefinedArea,
                               int32_t noteRowIndexStart, int32_t noteRowIndexEnd, int32_t xStart, int32_t xEnd,
                               bool allowBlur, bool drawRepeats) {

	RGB rgb = getColour();
	std::fill(&image[xStart], &image[xStart] + (xEnd - xStart), rgb.dim());
	if (occupancyMask) {
		memset(&occupancyMask[xStart], 64, (xEnd - xStart));
	}

	if (addUndefinedArea) {
		drawUndefinedArea(xScroll, xZoom, loopLength, image, occupancyMask, kDisplayWidth, editorScreen, false);
	}

	return true;
}

Error FXClip::claimOutput(ModelStackWithTimelineCounter* modelStack) {

	output = modelStack->song->getFXOutput();

	if (!output) {
		return Error::FILE_CORRUPTED;
	}

	return Error::NONE;
}

Error FXClip::setOutput(ModelStackWithTimelineCounter* modelStack, Output* newOutput,
                        FXClip* favourClipForCloningParamManager) {
	output = newOutput;
	Error error = solicitParamManager(modelStack->song, nullptr, favourClipForCloningParamManager);
	if (error != Error::NONE) {
		return error;
	}

	outputChanged(modelStack, newOutput);

	return Error::NONE;
}

void FXClip::setPos(ModelStackWithTimelineCounter* modelStack, int32_t newPos, bool useActualPosForParamManagers) {

	Clip::setPos(modelStack, newPos, useActualPosForParamManagers);

	setPosForParamManagers(modelStack, useActualPosForParamManagers);
}

bool FXClip::shiftHorizontally(ModelStackWithTimelineCounter* modelStack, int32_t amount, bool shiftAutomation,
                               bool shiftSequenceAndMPE) {
	if (!shiftAutomation) {
		return false;
	}

	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addOtherTwoThingsButNoNoteRow(output->toModControllable(), &paramManager);

	if (paramManager.containsAnyParamCollectionsIncludingExpression()) {
		ParamCollectionSummary* summary = paramManager.summaries;

		while (summary->paramCollection) {
			ModelStackWithParamCollection* modelStackWithParamCollection =
			    modelStackWithThreeMainThings->addParamCollection(summary->paramCollection, summary);

			summary->paramCollection->shiftHorizontally(modelStackWithParamCollection, amount, loopLength);
			summary++;
		}
	}

	return true;
}

RGB FXClip::getColour() {
	return RGB::fromHuePastel(colourOffset * -8 / 3);
}

void FXClip::writeDataToFile(Serializer& writer, Song* song) {

	writer.writeAttribute("trackName", output->name.get());

	if (onAutomationClipView) {
		writer.writeAttribute("onAutomationInstrumentClipView", 1);
	}
	if (lastSelectedParamID != deluge::modulation::params::kNoParamID) {
		writer.writeAttribute("lastSelectedParamID", lastSelectedParamID);
		writer.writeAttribute("lastSelectedParamKind", util::to_underlying(lastSelectedParamKind));
		writer.writeAttribute("lastSelectedParamShortcutX", lastSelectedParamShortcutX);
		writer.writeAttribute("lastSelectedParamShortcutY", lastSelectedParamShortcutY);
		writer.writeAttribute("lastSelectedParamArrayPosition", lastSelectedParamArrayPosition);
	}

	Clip::writeDataToFile(writer, song);

	writer.writeOpeningTagEnd();

	Clip::writeMidiCommandsToFile(writer, song);

	writer.writeOpeningTagBeginning("params");
	GlobalEffectable::writeParamAttributesToFile(writer, &paramManager, true);
	writer.writeOpeningTagEnd();
	GlobalEffectable::writeParamTagsToFile(writer, &paramManager, true);
	writer.writeClosingTag("params");
}

Error FXClip::readFromFile(Deserializer& reader, Song* song) {

	char const* tagName;

	int32_t readAutomationUpToPos = kMaxSequenceLength;

	while (*(tagName = reader.readNextTagOrAttributeName())) {

		if (!strcmp(tagName, "params")) {
			paramManager.setupUnpatched();
			GlobalEffectable::initParams(&paramManager);
			GlobalEffectable::readParamsFromFile(reader, &paramManager, readAutomationUpToPos);
		}

		else if (!strcmp(tagName, "onAutomationInstrumentClipView")) {
			onAutomationClipView = reader.readTagOrAttributeValueInt();
		}

		else if (!strcmp(tagName, "lastSelectedParamID")) {
			lastSelectedParamID = reader.readTagOrAttributeValueInt();
		}

		else if (!strcmp(tagName, "lastSelectedParamKind")) {
			lastSelectedParamKind = static_cast<deluge::modulation::params::Kind>(reader.readTagOrAttributeValueInt());
		}

		else if (!strcmp(tagName, "lastSelectedParamShortcutX")) {
			lastSelectedParamShortcutX = reader.readTagOrAttributeValueInt();
		}

		else if (!strcmp(tagName, "lastSelectedParamShortcutY")) {
			lastSelectedParamShortcutY = reader.readTagOrAttributeValueInt();
		}

		else if (!strcmp(tagName, "lastSelectedParamArrayPosition")) {
			lastSelectedParamArrayPosition = reader.readTagOrAttributeValueInt();
		}

		else {
			readTagFromFile(reader, tagName, song, &readAutomationUpToPos);
		}

		reader.exitTag();
	}

	return Error::NONE;
}

bool FXClip::isEmpty(bool displayPopup) {
	return !paramManager.mightContainAutomation();
}
