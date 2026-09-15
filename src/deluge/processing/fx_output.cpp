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

#include "processing/fx_output.h"
#include "model/clip/clip.h"
#include "model/global_effectable/global_effectable.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "modulation/params/param_manager.h"
#include "storage/storage_manager.h"
#include <cstring>

FXOutput::FXOutput(Song& song) : Output(OutputType::AUDIO_FX), song(song) {
}

ModControllable* FXOutput::toModControllable() {
	return &song.globalEffectable;
}

// FXOutputs are only ever written as part of a Song, so clipForSavingOutputOnly will always be NULL
bool FXOutput::writeDataToFile(Serializer& writer, Clip* clipForSavingOutputOnly, Song* song) {

	writer.writeAttribute("name", name.get());

	Output::writeDataToFile(writer, clipForSavingOutputOnly, song);

	writer.writeOpeningTagEnd();

	// If no activeClip, there's a backedUpParamManager holding the params - save it so they survive the round-trip
	if (!activeClip) {
		ParamManager* paramManager =
		    song->getBackedUpParamManagerPreferablyWithClip((ModControllableAudio*)toModControllable(), nullptr);
		if (paramManager) {
			writer.writeOpeningTagBeginning("params");
			GlobalEffectable::writeParamAttributesToFile(writer, paramManager, true);
			writer.writeOpeningTagEnd();
			GlobalEffectable::writeParamTagsToFile(writer, paramManager, true);
			writer.writeClosingTag("params");
		}
	}

	// The GLOBAL-domain macros block (FX-clip macros live on this Output, like audio-clip macros
	// live on theirs). Only written when a macro deviates from its defaults.
	if (Macros::anyMacroConfigured(macros)) {
		Macros::writeMacrosToFile(writer, macros, Macros::domainForOutput(this));
	}

	return true;
}

// clip will always be NULL and is of no consequence - see note in parent output.h
Error FXOutput::readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) {
	char const* tagName;

	ParamManagerForTimeline paramManager;

	while (*(tagName = reader.readNextTagOrAttributeName())) {

		if (!strcmp(tagName, "params")) {
			paramManager.setupUnpatched();
			GlobalEffectable::initParams(&paramManager);
			GlobalEffectable::readParamsFromFile(reader, &paramManager, readAutomationUpToPos);
			reader.exitTag("params");
		}

		else if (!strcmp(tagName, "macros")) {
			Macros::readMacrosFromFile(reader, macros, Macros::domainForOutput(this));
			reader.exitTag();
		}

		else if (Output::readTagFromFile(reader, tagName)) {}

		else {
			reader.exitTag(tagName);
		}
	}

	// Always keep a backedUpParamManager for this output, so Output::getParamManager() can never fail (E170) even if
	// it ends up with no active Clip
	if (!paramManager.containsAnyMainParamCollections()) {
		Error error = paramManager.setupUnpatched();
		if (error != Error::NONE) {
			return error;
		}
		GlobalEffectable::initParams(&paramManager);
	}
	song->backUpParamManager((ModControllableAudio*)toModControllable(), nullptr, &paramManager);

	return Error::NONE;
}

void FXOutput::deleteBackedUpParamManagers(Song* song) {
	song->deleteBackedUpParamManagersForModControllable((ModControllableAudio*)toModControllable());
}

ModelStackWithAutoParam* FXOutput::getModelStackWithParam(ModelStackWithTimelineCounter* modelStack, Clip* clip,
                                                          int32_t paramID, deluge::modulation::params::Kind paramKind,
                                                          bool affectEntire, bool useMenuStack) {
	ModelStackWithAutoParam* modelStackWithParam = nullptr;

	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addOtherTwoThingsButNoNoteRow(toModControllable(), &clip->paramManager);

	if (modelStackWithThreeMainThings) {
		modelStackWithParam = modelStackWithThreeMainThings->getUnpatchedAutoParamFromId(paramID);
	}

	return modelStackWithParam;
}
