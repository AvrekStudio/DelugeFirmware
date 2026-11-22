/*
 * Copyright (c) 2026 Synthstrom Audible Limited
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

#include "model/note_fx/note_fx_runner.h"
#include "definitions_cxx.hpp"
#include "model/clip/instrument_clip.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "processing/sound/sound_instrument.h"

extern int16_t zeroMPEValues[];

namespace deluge::note_fx {

// ---- Sink: the track's voices

void Runner::Sink::noteOn(int32_t note, int32_t velocity) {
	if (note < 0 || note > 127) {
		return;
	}
	if (sounding[note] < 255) {
		sounding[note]++;
	}
	track->Sound::noteOn(modelStack, track->getArp(), note, zeroMPEValues, 0, 0, 0, velocity, MIDI_CHANNEL_NONE);
}

void Runner::Sink::noteOff(int32_t note) {
	if (note < 0 || note > 127 || sounding[note] == 0) {
		return; // never started (or already ended): nothing to end
	}
	sounding[note]--;
	track->Sound::noteOff(modelStack, track->getArp(), note);
}

bool Runner::Sink::any() const {
	for (uint8_t s : sounding) {
		if (s != 0) {
			return true;
		}
	}
	return false;
}

// ---- Runner

void Runner::updateParams(InstrumentClip* clip, SoundInstrument& track) {
	if (!lockActive_) {
		effective_ = track.noteFxValues;
	}
	for (int32_t i = 0; i < chain_.count(); i++) {
		chain_.setParams(i, effective_.v[static_cast<int32_t>(chain_.typeAt(i))].data());
	}
	context_.barTicks = currentSong->getBarLength();
	context_.playing = playbackHandler.isEitherClockActive();
	context_.trackNote = track.noteDefaults.note;
	context_.now = clock_;
}

void Runner::prepare(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack) {
	sink_.track = &track;
	sink_.modelStack = modelStack;
	auto* clip = static_cast<InstrumentClip*>(track.getActiveClip());
	updateParams(clip, track);
	if (clip != nullptr) {
		if (chain_.configure(clip->noteFxRack, sink_, context_)) {
			updateParams(clip, track);
		}
	}
}

bool Runner::noteOn(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t note, int32_t velocity) {
	prepare(track, modelStack);
	if (!chain_.active()) {
		direct_.set(note);
		return false;
	}
	// A note placed on the grid with p-locks: its values apply while it plays
	auto* clip = static_cast<InstrumentClip*>(track.getActiveClip());
	if (clip != nullptr && context_.playing && clip->noteFxLocks.count() > 0) {
		Values locked = track.noteFxValues;
		if (clip->noteFxLocks.applyTo(clip->lastProcessedPos, note, locked)) {
			effective_ = locked;
			lockActive_ = true;
			lockNote_ = note;
			updateParams(clip, track);
		}
	}
	chain_.noteOn(note, velocity, context_, sink_);
	return true;
}

bool Runner::noteOff(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t note) {
	if (note >= 0 && note < 128 && direct_.test(note)) {
		direct_.reset(note);
		return false; // started straight on the voices: ends there too
	}
	prepare(track, modelStack);
	chain_.noteOff(note, context_, sink_);
	if (lockActive_ && note == lockNote_) {
		lockActive_ = false;
		lockNote_ = -1;
		updateParams(static_cast<InstrumentClip*>(track.getActiveClip()), track);
	}
	return true;
}

void Runner::render(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t numSamples) {
	const bool playing = playbackHandler.isEitherClockActive();
	if (playing) {
		clock_ = static_cast<double>(playbackHandler.getActualSwungTickCount());
	}
	else {
		const float samplesPerTick = currentSong->getTimePerTimerTickFloat();
		if (samplesPerTick > 0) {
			clock_ += numSamples / samplesPerTick;
		}
	}
	if (!chain_.active() && !sink_.any()) {
		// nothing running: just keep the chain in step with the rack
		auto* clip = static_cast<InstrumentClip*>(track.getActiveClip());
		if (clip == nullptr) {
			return;
		}
		bool anyModule = false;
		for (const Slot& s : clip->noteFxRack.slots) {
			anyModule |= s.on && (s.type == Type::ARP || s.type == Type::EUCL || s.type == Type::MONO);
		}
		if (!anyModule) {
			return;
		}
	}
	prepare(track, modelStack);
	chain_.advance(context_, sink_);
}

void Runner::stop(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack) {
	sink_.track = &track;
	sink_.modelStack = modelStack;
	chain_.reset(sink_);
	for (int32_t n = 0; n < 128; n++) {
		while (sink_.sounding[n] > 0) {
			sink_.noteOff(n);
		}
	}
	lockActive_ = false;
}

} // namespace deluge::note_fx
