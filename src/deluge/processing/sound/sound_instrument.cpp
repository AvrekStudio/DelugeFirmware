/*
 * Copyright © 2014-2023 Synthstrom Audible Limited
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

#include "processing/sound/sound_instrument.h"
#include "definitions_cxx.hpp"
#include "dsp/stereo_sample.h"
#include "extern.h"
#include "gui/views/view.h"
#include "model/clip/instrument_clip.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "model/voice/voice.h"
#include "modulation/arpeggiator.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "modulation/patch/patch_cable_set.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "storage/audio/audio_file_manager.h"
#include "storage/storage_manager.h"
#include "util/misc.h"

namespace params = deluge::modulation::params;

SoundInstrument::SoundInstrument() : MelodicInstrument(OutputType::SYNTH) {
}

bool SoundInstrument::writeDataToFile(Serializer& writer, Clip* clipForSavingOutputOnly, Song* song) {

	// MelodicInstrument::writeDataToFile(writer, clipForSavingOutputOnly, song); // Nope, this gets called within the
	// below call
	writeMelodicInstrumentAttributesToFile(writer, clipForSavingOutputOnly, song);
	writer.writeAttribute("chordMode", static_cast<int32_t>(chordSettings.mode));
	writer.writeAttribute("chordRootType", chordSettings.rootType);
	writer.writeAttribute("chordFreeType", chordSettings.freeType);
	writer.writeAttribute("chordStrum", chordSettings.strumMs);
	writer.writeAttribute("chordBass", chordSettings.bass);

	ParamManager* paramManager;

	// If saving Output only...
	if (clipForSavingOutputOnly) {
		paramManager = &clipForSavingOutputOnly->paramManager;

		// Or if saving Song...
	}
	else {

		// If no activeClip, that means no Clip has this Output, so there should be a backedUpParamManager that we
		// should use
		if (!activeClip) {
			paramManager = song->getBackedUpParamManagerPreferablyWithClip(this, NULL);
		}
		else {
			paramManager = nullptr;
		}
	}

	Sound::writeToFile(writer, clipForSavingOutputOnly == nullptr, paramManager,
	                   clipForSavingOutputOnly ? &((InstrumentClip*)clipForSavingOutputOnly)->arpSettings : nullptr,
	                   NULL);

	MelodicInstrument::writeMelodicInstrumentTagsToFile(writer, clipForSavingOutputOnly, song);

	return true;
}

// arpSettings optional - no need if you're loading a new V2.0 song where Instruments are all separate from Clips and
// won't store any arp stuff
Error SoundInstrument::readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) {

	char modelStackMemory[MODEL_STACK_MAX_SIZE];
	ModelStackWithModControllable* modelStack =
	    setupModelStackWithSong(modelStackMemory, song)->addTimelineCounter(clip)->addModControllableButNoNoteRow(this);

	return Sound::readFromFile(reader, modelStack, readAutomationUpToPos, &defaultArpSettings);
}

void SoundInstrument::cutAllSound() {
	Sound::killAllVoices();
	for (StrumPending& p : strumPending_) {
		p.key = -1;
	}
}

void SoundInstrument::renderOutput(ModelStack* modelStack, std::span<StereoSample> output, int32_t* reverbBuffer,
                                   int32_t reverbAmountAdjust, int32_t sideChainHitPending,
                                   bool shouldLimitDelayFeedback, bool isClipActive) {
	// this should only happen in the rare case that this is called while replacing an instrument but after the clips
	// have been cleared
	if (!activeClip) [[unlikely]] {
		return;
	}
	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addTimelineCounter(activeClip)
	        ->addOtherTwoThingsButNoNoteRow(this, getParamManager(modelStack->song));

	renderStrum(modelStackWithThreeMainThings, static_cast<int32_t>(output.size())); // CHORD MODE STRUM
	if (latched_.any() && !latchActive()) {
		releaseLatched(modelStackWithThreeMainThings); // LATCH switched off (or the arp): the latched notes end
	}

	if (skippingRendering) {
		compressor.reset();
		compressor.gainReduction = 0;
	}
	else {
		Sound::render(modelStackWithThreeMainThings, output, reverbBuffer, sideChainHitPending, reverbAmountAdjust,
		              shouldLimitDelayFeedback, kMaxSampleValue, recorder);
	}

	if (playbackHandler.isEitherClockActive() && !playbackHandler.ticksLeftInCountIn && isClipActive) {

		// No time to call the proper function and do error checking, sorry.
		ParamCollectionSummary* patchedParamsSummary = &modelStackWithThreeMainThings->paramManager->summaries[1];
		bool anyInterpolating = false;
		if constexpr (params::kNumParams > 64) {
			anyInterpolating = patchedParamsSummary->whichParamsAreInterpolating[0]
			                   || patchedParamsSummary->whichParamsAreInterpolating[1]
			                   || patchedParamsSummary->whichParamsAreInterpolating[2];
		}
		else {
			anyInterpolating = patchedParamsSummary->whichParamsAreInterpolating[0]
			                   || patchedParamsSummary->whichParamsAreInterpolating[1];
		}
		if (anyInterpolating) {
yesTickParamManagerForClip:
			modelStackWithThreeMainThings->paramManager->toForTimeline()->tickSamples(output.size(),
			                                                                          modelStackWithThreeMainThings);
		}
		else {

			// Try other options too.

			// No time to call the proper function and do error checking, sorry.
			ParamCollectionSummary* unpatchedParamsSummary = &modelStackWithThreeMainThings->paramManager->summaries[0];
			if constexpr (params::UNPATCHED_SOUND_MAX_NUM > 32) {
				if (unpatchedParamsSummary->whichParamsAreInterpolating[0]
				    || unpatchedParamsSummary->whichParamsAreInterpolating[1]) {
					goto yesTickParamManagerForClip;
				}
			}
			else {
				if (unpatchedParamsSummary->whichParamsAreInterpolating[0]) {
					goto yesTickParamManagerForClip;
				}
			}

			// No time to call the proper function and do error checking, sorry.
			ParamCollectionSummary* patchCablesSummary = &modelStackWithThreeMainThings->paramManager->summaries[2];
			if constexpr (kMaxNumPatchCables > 32) {
				if (patchCablesSummary->whichParamsAreInterpolating[0]
				    || patchCablesSummary->whichParamsAreInterpolating[1]) {
					goto yesTickParamManagerForClip;
				}
			}
			else {
				if (patchCablesSummary->whichParamsAreInterpolating[0]) {
					goto yesTickParamManagerForClip;
				}
			}

			// No time to call the proper function and do error checking, sorry.
			ParamCollectionSummary* expressionParamsSummary =
			    &modelStackWithThreeMainThings->paramManager->summaries[3];
			if constexpr (kNumExpressionDimensions > 32) {
				if (expressionParamsSummary->whichParamsAreInterpolating[0]
				    || expressionParamsSummary->whichParamsAreInterpolating[1]) {
					goto yesTickParamManagerForClip;
				}
			}
			else {
				if (expressionParamsSummary->whichParamsAreInterpolating[0]) {
					goto yesTickParamManagerForClip;
				}
			}
		}

		// Do the ParamManagers of each NoteRow, too
		for (int32_t i = 0; i < ((InstrumentClip*)activeClip)->noteRows.getNumElements(); i++) {
			NoteRow* thisNoteRow = ((InstrumentClip*)activeClip)->noteRows.getElement(i);
			// No time to call the proper function and do error checking, sorry.
			ParamCollectionSummary* expressionParamsSummary = &thisNoteRow->paramManager.summaries[0];
			bool result = false;
			if constexpr (kNumExpressionDimensions > 32) {
				result = expressionParamsSummary->whichParamsAreInterpolating[0]
				         || expressionParamsSummary->whichParamsAreInterpolating[1];
			}
			else {
				result = expressionParamsSummary->whichParamsAreInterpolating[0];
			}
			if (result) {
				modelStackWithThreeMainThings->setNoteRow(thisNoteRow, thisNoteRow->y);
				modelStackWithThreeMainThings->paramManager = &thisNoteRow->paramManager;
				thisNoteRow->paramManager.tickSamples(output.size(), modelStackWithThreeMainThings);
			}
		}
	}
}

Error SoundInstrument::loadAllAudioFiles(bool mayActuallyReadFiles) {

	bool doingAlternatePath =
	    mayActuallyReadFiles && (audioFileManager.alternateLoadDirStatus == AlternateLoadDirStatus::NONE_SET);
	if (doingAlternatePath) {
		Error error = setupDefaultAudioFileDir();
		if (error != Error::NONE) {
			return error;
		}
	}

	Error error = Sound::loadAllAudioFiles(mayActuallyReadFiles);

	if (doingAlternatePath) {
		audioFileManager.thingFinishedLoading();
	}

	return error;
}

void SoundInstrument::resyncLFOs(bool reset_state) {
	resyncGlobalLFOs(reset_state);
}

ModControllable* SoundInstrument::toModControllable() {
	return this;
}

void SoundInstrument::setupPatching(ModelStackWithTimelineCounter* modelStack) {

	InstrumentClip* clip = (InstrumentClip*)modelStack->getTimelineCounterAllowNull();
	ParamManagerForTimeline* paramManager;
	if (clip) {
		paramManager = &clip->paramManager;

		ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
		    modelStack->addOtherTwoThingsButNoNoteRow(this, paramManager);

		ensureInaccessibleParamPresetValuesWithoutKnobsAreZero(modelStackWithThreeMainThings);
	}
	else {
		paramManager =
		    (ParamManagerForTimeline*)modelStack->song->getBackedUpParamManagerPreferablyWithClip(this, NULL);
		ensureInaccessibleParamPresetValuesWithoutKnobsAreZeroWithMinimalDetails(paramManager);
	}

	ModelStackWithParamCollection* modelStackWithParamCollection =
	    paramManager->getPatchCableSet(modelStack->addOtherTwoThingsButNoNoteRow(this, paramManager));

	PatchCableSet* patchCableSet = (PatchCableSet*)modelStackWithParamCollection->paramCollection;

	patchCableSet->setupPatching(modelStackWithParamCollection);
}

bool SoundInstrument::setActiveClip(ModelStackWithTimelineCounter* modelStack, PgmChangeSend maySendMIDIPGMs) {

	bool clipChanged = MelodicInstrument::setActiveClip(modelStack, maySendMIDIPGMs);

	if (clipChanged) {
		AudioEngine::mustUpdateReverbParamsBeforeNextRender = true;

		if (modelStack) {
			ParamManager* paramManager = &modelStack->getTimelineCounter()->paramManager;
			patcher.performInitialPatching(*this, *paramManager);

			// Grab mono expression params
			ExpressionParamSet* expressionParams = paramManager->getExpressionParamSet();
			if (expressionParams) {
				for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
					monophonicExpressionValues[i] = expressionParams->params[i].getCurrentValue();
				}
			}
			else {
				for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
					monophonicExpressionValues[i] = 0;
				}
			}
			expressionSourcesChangedAtSynthLevel.set();
		}
	}
	return clipChanged;
}

void SoundInstrument::setupWithoutActiveClip(ModelStack* modelStack) {

	ModelStackWithTimelineCounter* modelStackWithTimelineCounter = modelStack->addTimelineCounter(nullptr);

	setupPatching(modelStackWithTimelineCounter);

	ParamManager* paramManager =
	    modelStackWithTimelineCounter->song->getBackedUpParamManagerPreferablyWithClip(this, NULL);
	if (!paramManager) {
		FREEZE_WITH_ERROR("E173");
	}
	patcher.performInitialPatching(*this, *paramManager);

	// Clear mono expression params
	for (int32_t i = 0; i < kNumExpressionDimensions; i++) {
		monophonicExpressionValues[i] = 0;
	}
	expressionSourcesChangedAtSynthLevel.set();

	Instrument::setupWithoutActiveClip(modelStack);
}

void SoundInstrument::prepareForHibernationOrDeletion() {
	Sound::prepareForHibernation();
}

void SoundInstrument::setupPatchingForAllParamManagers(Song* song) {
	song->setupPatchingForAllParamManagersForInstrument(this);
}

void SoundInstrument::deleteBackedUpParamManagers(Song* song) {
	song->deleteBackedUpParamManagersForModControllable(this);
}

extern bool expressionValueChangesMustBeDoneSmoothly;

void SoundInstrument::monophonicExpressionEvent(int32_t newValue, int32_t expressionDimension) {
	expressionSourcesChangedAtSynthLevel[expressionDimension] = true;
	monophonicExpressionValues[expressionDimension] = newValue;
}

// Alternative to what's in the NonAudioInstrument:: implementation, which would almost work here, but we cut corner for
// Sound by avoiding going through the Arp and just talk directly to the Voices. (Despite my having made it now actually
// need to talk to the Arp too, as below...) Note, this virtual function actually overrides/implements from two base
// classes - MelodicInstrument and ModControllable.
void SoundInstrument::polyphonicExpressionEventOnChannelOrNote(int32_t newValue, int32_t expressionDimension,
                                                               int32_t channelOrNoteNumber,
                                                               MIDICharacteristic whichCharacteristic) {
	int32_t s = expressionDimension + util::to_underlying(PatchSource::X);
	for (const auto& voice : this->voices()) {
		if (voice->inputCharacteristics[util::to_underlying(whichCharacteristic)] == channelOrNoteNumber) {
			if (expressionValueChangesMustBeDoneSmoothly) {
				voice->expressionEventSmooth(newValue, s);
			}
			else {
				voice->expressionEventImmediate(*this, newValue, s);
			}
		}
	}

	// Must update MPE values in Arp too - useful either if it's on, or if we're in true monophonic mode - in either
	// case, we could need to suddenly do a note-on for a different note that the Arp knows about, and need these MPE
	// values.
	int32_t n, nEnd;
	if (whichCharacteristic == MIDICharacteristic::NOTE) {
		n = arpeggiator.notes.search(channelOrNoteNumber, GREATER_OR_EQUAL);
		if (n < arpeggiator.notes.getNumElements()) {
			nEnd = 0;
			goto lookAtArpNote;
		}
		return;
	}
	nEnd = arpeggiator.notes.getNumElements();
	for (n = 0; n < nEnd; n++) {
lookAtArpNote:
		ArpNote* arpNote = (ArpNote*)arpeggiator.notes.getElementAddress(n);
		if (arpNote->inputCharacteristics[util::to_underlying(whichCharacteristic)] == channelOrNoteNumber) {
			arpNote->mpeValues[expressionDimension] = newValue >> 16;
		}
	}

	// Let the Sound know about this polyphonic expression event
	// The Sound class will use it to send MIDI out (if enabled in the sound config)
	Sound::polyphonicExpressionEventOnChannelOrNote(newValue, expressionDimension, channelOrNoteNumber,
	                                                whichCharacteristic);
}

void SoundInstrument::sendNote(ModelStackWithThreeMainThings* modelStack, bool isOn, int32_t noteCode,
                               int16_t const* mpeValues, int32_t fromMIDIChannel, uint8_t velocity,
                               uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate) {

	if (!inValidState) {
		return;
	}

	// ARP LATCH: a key let go keeps playing, until a key is played with none held
	if (isOn) {
		if (latched_.any() && down_.none()) {
			releaseLatched(modelStack); // a new set of notes replaces the latched one
		}
		else if (latched_[noteCode]) {
			latched_[noteCode] = false; // the same key again: its latched note ends, it starts anew
			playKey(modelStack, false, noteCode, mpeValues, fromMIDIChannel, velocity, 0, 0, 0);
		}
		down_[noteCode] = true;
	}
	else {
		down_[noteCode] = false;
		if (latchActive()) {
			latched_[noteCode] = true;
			return;
		}
	}
	playKey(modelStack, isOn, noteCode, mpeValues, fromMIDIChannel, velocity, sampleSyncLength, ticksLate, samplesLate);
}

bool SoundInstrument::latchActive() {
	ArpeggiatorSettings* settings = getArpSettings();
	return settings != nullptr && settings->latch && !ArpeggiatorBase::arpIsOff(settings);
}

void SoundInstrument::releaseLatched(ModelStackWithThreeMainThings* modelStack) {
	for (int32_t note = 0; note < 128; note++) {
		if (latched_[note]) {
			latched_[note] = false;
			playKey(modelStack, false, note, zeroMPEValues, MIDI_CHANNEL_NONE, 0, 0, 0, 0);
		}
	}
}

void SoundInstrument::playKey(ModelStackWithThreeMainThings* modelStack, bool isOn, int32_t noteCode,
                              int16_t const* mpeValues, int32_t fromMIDIChannel, uint8_t velocity,
                              uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate) {
	// CHORD MODE comes before the arpeggiator: each note of the chord goes into it as if held
	if (isOn) {
		if (chordSettings.mode != deluge::chordmode::Mode::OFF && !chordModeSuppressed) {
			chordNoteOn(modelStack, noteCode, mpeValues, fromMIDIChannel, velocity, sampleSyncLength, ticksLate,
			            samplesLate);
			return;
		}
		noteOn(modelStack, &arpeggiator, noteCode, mpeValues, sampleSyncLength, ticksLate, samplesLate, velocity,
		       fromMIDIChannel);
	}
	else {
		if (chordNoteOff(modelStack, noteCode)) {
			return;
		}
		noteOff(modelStack, &arpeggiator, noteCode);
	}
}

void SoundInstrument::chordNoteOn(ModelStackWithThreeMainThings* modelStack, int32_t key, int16_t const* mpeValues,
                                  int32_t fromMIDIChannel, uint8_t velocity, uint32_t sampleSyncLength,
                                  int32_t ticksLate, uint32_t samplesLate) {
	using namespace deluge::chordmode;
	chordNoteOff(modelStack, key); // the same key again: its previous chord ends first

	const deluge::chordmode::Scale scale{currentSong->key.modeNotes.toBits(), currentSong->key.rootNote};
	const Chord chord = build(chordSettings, key, scale);

	ChordHeld* slot = nullptr;
	for (ChordHeld& h : chordHeld_) {
		if (h.key < 0) {
			slot = &h;
			break;
		}
	}
	if (slot == nullptr) {
		// every slot taken: play the key alone
		noteOn(modelStack, &arpeggiator, key, mpeValues, sampleSyncLength, ticksLate, samplesLate, velocity,
		       fromMIDIChannel);
		return;
	}
	slot->key = static_cast<int16_t>(key);
	slot->chord = chord;
	slot->started = 0;
	chordLastKey = static_cast<int16_t>(key);
	chordKeysHeld++;
	chordPlayCounter++;

	// STRUM only with the arp off (the arp plays the notes one by one already)
	const bool strum = chordSettings.strumMs > 0 && ArpeggiatorBase::arpIsOff(getArpSettings());
	const int32_t strumSamples = static_cast<int32_t>(chordSettings.strumMs) * kSampleRate / 1000;
	for (int32_t i = 0; i < chord.count; i++) {
		if (i == 0 || !strum) {
			chordStart(modelStack, *slot, i, mpeValues, fromMIDIChannel, velocity, sampleSyncLength, ticksLate,
			           samplesLate);
			continue;
		}
		for (StrumPending& p : strumPending_) {
			if (p.key < 0) {
				p.key = static_cast<int16_t>(key);
				p.note = chord.notes[i];
				p.samplesLeft = strumSamples * i;
				p.velocity = velocity;
				for (int32_t d = 0; d < kNumExpressionDimensions; d++) {
					p.mpe[d] = mpeValues != nullptr ? mpeValues[d] : 0;
				}
				break;
			}
		}
	}
}

void SoundInstrument::chordStart(ModelStackWithThreeMainThings* modelStack, ChordHeld& held, int32_t index,
                                 int16_t const* mpeValues, int32_t fromMIDIChannel, uint8_t velocity,
                                 uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate) {
	const int32_t note = held.chord.notes[index];
	held.started |= static_cast<uint8_t>(1u << index);
	if (chordPitchCount_[note]++ > 0) {
		return; // another held chord sounds it already
	}
	noteOn(modelStack, &arpeggiator, note, mpeValues, sampleSyncLength, ticksLate, samplesLate, velocity,
	       fromMIDIChannel);
}

bool SoundInstrument::chordNoteOff(ModelStackWithThreeMainThings* modelStack, int32_t key) {
	for (ChordHeld& h : chordHeld_) {
		if (h.key != key) {
			continue;
		}
		// strum notes not started yet never will
		for (StrumPending& p : strumPending_) {
			if (p.key == key) {
				p.key = -1;
			}
		}
		for (int32_t i = 0; i < h.chord.count; i++) {
			if (!((h.started >> i) & 1)) {
				continue;
			}
			uint8_t& count = chordPitchCount_[h.chord.notes[i]];
			if (count > 0) {
				count--;
			}
			if (count == 0) {
				noteOff(modelStack, &arpeggiator, h.chord.notes[i]); // no other held chord has it
			}
		}
		h.key = -1;
		h.started = 0;
		if (chordKeysHeld > 0) {
			chordKeysHeld--;
		}
		chordPlayCounter++;
		return true;
	}
	return false;
}

void SoundInstrument::renderStrum(ModelStackWithThreeMainThings* modelStack, int32_t numSamples) {
	for (StrumPending& p : strumPending_) {
		if (p.key < 0) {
			continue;
		}
		p.samplesLeft -= numSamples;
		if (p.samplesLeft <= 0) {
			const int16_t key = p.key;
			p.key = -1;
			// started late: it must still belong to a held chord
			for (ChordHeld& h : chordHeld_) {
				if (h.key != key) {
					continue;
				}
				for (int32_t i = 0; i < h.chord.count; i++) {
					if (h.chord.notes[i] == p.note && !((h.started >> i) & 1)) {
						chordStart(modelStack, h, i, p.mpe.data(), MIDI_CHANNEL_NONE, p.velocity, 0, 0, 0);
						break;
					}
				}
				break;
			}
		}
	}
}

ArpeggiatorSettings* SoundInstrument::getArpSettings(InstrumentClip* clip) {
	return MelodicInstrument::getArpSettings(clip);
}

bool SoundInstrument::readTagFromFile(Deserializer& reader, char const* tagName) {
	using namespace deluge::chordmode;
	auto readClamped = [&](int32_t lo, int32_t hi) {
		const int32_t v = std::clamp<int32_t>(reader.readTagOrAttributeValueInt(), lo, hi);
		reader.exitTag();
		return v;
	};
	if (!strcmp(tagName, "chordMode")) {
		chordSettings.mode = static_cast<Mode>(readClamped(0, static_cast<int32_t>(Mode::NUM) - 1));
		return true;
	}
	if (!strcmp(tagName, "chordRootType")) {
		chordSettings.rootType = static_cast<uint8_t>(readClamped(0, kNumRootTypes - 1));
		return true;
	}
	if (!strcmp(tagName, "chordFreeType")) {
		chordSettings.freeType = static_cast<uint8_t>(readClamped(0, kNumFreeTypes - 1));
		return true;
	}
	if (!strcmp(tagName, "chordStrum")) {
		chordSettings.strumMs = static_cast<uint16_t>(readClamped(0, kStrumMaxMs));
		return true;
	}
	if (!strcmp(tagName, "chordBass")) {
		chordSettings.bass = static_cast<int8_t>(readClamped(kBassMin, kBassMax));
		return true;
	}
	return MelodicInstrument::readTagFromFile(reader, tagName);
}

void SoundInstrument::compensateInstrumentVolumeForResonance(ModelStackWithThreeMainThings* modelStack) {
	Sound::compensateVolumeForResonance(modelStack);
}

void SoundInstrument::loadCrucialAudioFilesOnly() {
	loadAllAudioFiles(true);
}

// Any time it gets edited, we want to grab the default arp settings from the activeClip
void SoundInstrument::beenEdited(bool shouldMoveToEmptySlot) {
	if (activeClip) {
		defaultArpSettings.cloneFrom(&((InstrumentClip*)activeClip)->arpSettings);
	}
	Instrument::beenEdited(shouldMoveToEmptySlot);
}

// Returns num ticks til next arp event
int32_t SoundInstrument::doTickForwardForArp(ModelStack* modelStack, int32_t currentPos) {
	if (!activeClip) {
		return 2147483647;
	}

	ModelStackWithThreeMainThings* modelStackWithThreeMainThings =
	    modelStack->addTimelineCounter(activeClip)
	        ->addOtherTwoThingsButNoNoteRow(this, getParamManager(modelStack->song));

	UnpatchedParamSet* unpatchedParams = modelStackWithThreeMainThings->paramManager->getUnpatchedParamSet();

	ArpeggiatorSettings* arpSettings = getArpSettings();
	arpSettings->updateParamsFromUnpatchedParamSet(unpatchedParams);

	ArpReturnInstruction instruction;

	int32_t ticksTilNextArpEvent =
	    arpeggiator.doTickForward(arpSettings, &instruction, currentPos, activeClip->currentlyPlayingReversed);

	ModelStackWithSoundFlags* modelStackWithSoundFlags = modelStackWithThreeMainThings->addSoundFlags();

	bool atLeastOneOff = false;
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.glideNoteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		atLeastOneOff = true;
		noteOffPostArpeggiator(modelStackWithSoundFlags, instruction.glideNoteCodeOffPostArp[n]);
	}
	for (int32_t n = 0; n < ARP_MAX_INSTRUCTION_NOTES; n++) {
		if (instruction.noteCodeOffPostArp[n] == ARP_NOTE_NONE) {
			break;
		}
		atLeastOneOff = true;
		noteOffPostArpeggiator(modelStackWithSoundFlags, instruction.noteCodeOffPostArp[n]);
	}
	if (atLeastOneOff) {
		invertReversed = false;
	}
	process_postarp_notes(modelStackWithSoundFlags, arpSettings, instruction);

	return ticksTilNextArpEvent;
}

void SoundInstrument::getThingWithMostReverb(Sound** soundWithMostReverb, ParamManager** paramManagerWithMostReverb,
                                             GlobalEffectableForClip** globalEffectableWithMostReverb,
                                             int32_t* highestReverbAmountFound) {
	if (activeClip) {
		Sound::getThingWithMostReverb(soundWithMostReverb, paramManagerWithMostReverb, globalEffectableWithMostReverb,
		                              highestReverbAmountFound, &activeClip->paramManager);
	}
}

ArpeggiatorBase* SoundInstrument::getArp() {
	return &arpeggiator;
}

bool SoundInstrument::noteIsOn(int32_t noteCode, bool resetTimeEntered) {

	ArpeggiatorSettings* arpSettings = getArpSettings();

	if (arpSettings != nullptr
	    && (arpSettings->mode != ArpMode::OFF || polyphonic == PolyphonyMode::LEGATO
	        || polyphonic == PolyphonyMode::MONO)) {
		int32_t n = arpeggiator.notes.search(noteCode, GREATER_OR_EQUAL);
		if (n >= arpeggiator.notes.getNumElements()) {
			return false;
		}
		ArpNote* arpNote = (ArpNote*)arpeggiator.notes.getElementAddress(n);
		return (arpNote->inputCharacteristics[util::to_underlying(MIDICharacteristic::NOTE)] == noteCode);
	}

	if (!hasActiveVoices()) {
		return false;
	}

	for (const auto& voice : this->voices()) {
		if ((voice->noteCodeAfterArpeggiation == noteCode)
		    && voice->envelopes[0].state < EnvelopeStage::RELEASE) { // Ignore releasing notes. Is this right?
			if (resetTimeEntered) {
				voice->envelopes[0].resetTimeEntered();
			}
			return true;
		}
	}
	return false;
}
