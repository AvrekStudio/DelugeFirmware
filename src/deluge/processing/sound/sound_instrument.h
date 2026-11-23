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

#pragma once

#include "util/chord_mode_calc.h"
#include <array>
#include <bitset>

#include "definitions_cxx.hpp"
#include "model/instrument/melodic_instrument.h"
#include "modulation/arpeggiator.h"
#include "processing/sound/sound.h"

class ParamManagerForTimeline;
class ParamManagerForTimeline;
class ModelStack;
class ModelStackWithThreeMainThings;

class SoundInstrument final : public Sound, public MelodicInstrument {
public:
	SoundInstrument();
	bool writeDataToFile(Serializer& writer, Clip* clipForSavingOutputOnly, Song* song) override;
	Error readFromFile(Deserializer& reader, Song* song, Clip* clip, int32_t readAutomationUpToPos) override;
	void cutAllSound() override;
	bool noteIsOn(int32_t noteCode, bool resetTimeEntered);

	void renderOutput(ModelStack* modelStack, std::span<StereoSample> buffer, int32_t* reverbBuffer,
	                  int32_t reverbAmountAdjust, int32_t sideChainHitPending, bool shouldLimitDelayFeedback,
	                  bool isClipActive) override;

	// A timelineCounter is required
	void offerReceivedCCToLearnedParams(MIDICable& cable, uint8_t channel, uint8_t ccNumber, uint8_t value,
	                                    ModelStackWithTimelineCounter* modelStack) override {
		Sound::offerReceivedCCToLearnedParamsForClip(cable, channel, ccNumber, value, modelStack);
	}
	bool offerReceivedPitchBendToLearnedParams(MIDICable& cable, uint8_t channel, uint8_t data1, uint8_t data2,
	                                           ModelStackWithTimelineCounter* modelStack) override {
		return Sound::offerReceivedPitchBendToLearnedParams(cable, channel, data1, data2, modelStack);
	}

	Error loadAllAudioFiles(bool mayActuallyReadFiles) override;
	void resyncLFOs(bool reset_state = false) override;
	ModControllable* toModControllable() override;
	bool setActiveClip(ModelStackWithTimelineCounter* modelStack, PgmChangeSend maySendMIDIPGMs) override;
	void setupPatchingForAllParamManagers(Song* song) override;
	void setupPatching(ModelStackWithTimelineCounter* modelStack) override;

	void deleteBackedUpParamManagers(Song* song) override;
	void polyphonicExpressionEventOnChannelOrNote(int32_t newValue, int32_t expressionDimension,
	                                              int32_t channelOrNoteNumber,
	                                              MIDICharacteristic whichCharacteristic) override;
	void monophonicExpressionEvent(int32_t newValue, int32_t expressionDimension) override;

	void sendNote(ModelStackWithThreeMainThings* modelStack, bool isOn, int32_t noteCode, int16_t const* mpeValues,
	              int32_t fromMIDIChannel, uint8_t velocity, uint32_t sampleSyncLength, int32_t ticksLate,
	              uint32_t samplesLate) override;

	ArpeggiatorSettings* getArpSettings(InstrumentClip* clip = nullptr) override;
	bool readTagFromFile(Deserializer& reader, char const* tagName) override;

	void prepareForHibernationOrDeletion() override;
	void compensateInstrumentVolumeForResonance(ModelStackWithThreeMainThings* modelStack) override;
	bool isSkippingRendering() override { return skippingRendering; }
	void loadCrucialAudioFilesOnly() override;
	void beenEdited(bool shouldMoveToEmptySlot = true) override;
	int32_t doTickForwardForArp(ModelStack* modelStack, int32_t currentPos) override;
	void setupWithoutActiveClip(ModelStack* modelStack) override;
	void getThingWithMostReverb(Sound** soundWithMostReverb, ParamManager** paramManagerWithMostReverb,
	                            GlobalEffectableForClip** globalEffectableWithMostReverb,
	                            int32_t* highestReverbAmountFound) override;
	uint8_t* getModKnobMode() override { return &modKnobMode; }
	ArpeggiatorBase* getArp() override;
	char const* getXMLTag() override { return "sound"; }
	const char* getName() override { return name.get(); }

	ArpeggiatorSettings defaultArpSettings;

	/// CHORD MODE (SOUND menu): settings, and the last key played (for the screen)
	deluge::chordmode::Settings chordSettings;
	int16_t chordLastKey = 60;
	uint8_t chordKeysHeld = 0;
	uint32_t chordPlayCounter = 0; // bumped on every key played, so the screen knows to redraw

private:
	// CHORD MODE: the notes each held key started (so its note off ends them, whatever the settings are by then)
	struct ChordHeld {
		int16_t key = -1;
		deluge::chordmode::Chord chord;
		uint8_t started = 0; // a bit per note of the chord that has started (strum notes start late)
	};
	std::array<ChordHeld, 16> chordHeld_{};
	// How many held chords hold each pitch: two chords sharing a note must not end it when only one is let go
	// (the arp keeps one copy of each pitch, and so does a voice)
	std::array<uint8_t, 128> chordPitchCount_{};
	void chordStart(ModelStackWithThreeMainThings* modelStack, ChordHeld& held, int32_t index, int16_t const* mpeValues,
	                int32_t fromMIDIChannel, uint8_t velocity, uint32_t sampleSyncLength, int32_t ticksLate,
	                uint32_t samplesLate);
	// CHORD MODE STRUM (arp off): notes still to start
	struct StrumPending {
		int16_t key = -1;
		int16_t note = 0;
		int32_t samplesLeft = 0;
		uint8_t velocity = 0;
		std::array<int16_t, kNumExpressionDimensions> mpe{};
	};
	std::array<StrumPending, 32> strumPending_{};
	void chordNoteOn(ModelStackWithThreeMainThings* modelStack, int32_t key, int16_t const* mpeValues,
	                 int32_t fromMIDIChannel, uint8_t velocity, uint32_t sampleSyncLength, int32_t ticksLate,
	                 uint32_t samplesLate);
	bool chordNoteOff(ModelStackWithThreeMainThings* modelStack, int32_t key);
	void renderStrum(ModelStackWithThreeMainThings* modelStack, int32_t numSamples);

	// ARP LATCH: keys held down, and keys let go whose notes keep playing
	std::bitset<128> down_;
	std::bitset<128> latched_;
	bool latchActive();
	void releaseLatched(ModelStackWithThreeMainThings* modelStack);
	void playKey(ModelStackWithThreeMainThings* modelStack, bool isOn, int32_t noteCode, int16_t const* mpeValues,
	             int32_t fromMIDIChannel, uint8_t velocity, uint32_t sampleSyncLength, int32_t ticksLate,
	             uint32_t samplesLate);
};
