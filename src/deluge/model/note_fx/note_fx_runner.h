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

#pragma once

#include "model/note_fx/note_fx_engine.h"
#include <bitset>
#include <cstdint>

class SoundInstrument;
class InstrumentClip;
class ModelStackWithThreeMainThings;

namespace deluge::note_fx {

/// Runs a synth track's NOTE FX rack (its active clip's) between the notes it is given (sequencer, keyboard, MIDI)
/// and its voices. Clocked from the track's audio rendering.
class Runner {
public:
	/// A note into the track. Returns false if the rack doesn't change notes - the caller then plays it directly.
	bool noteOn(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t note, int32_t velocity);
	bool noteOff(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t note);
	/// Called for every audio block (even when the track's voices aren't rendering).
	void render(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack, int32_t numSamples);
	/// Ends everything (e.g. the track is being removed).
	void stop(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack);

private:
	class Sink final : public NoteSink {
	public:
		SoundInstrument* track = nullptr;
		ModelStackWithThreeMainThings* modelStack = nullptr;
		std::array<uint8_t, 128> sounding{};
		void noteOn(int32_t note, int32_t velocity) override;
		void noteOff(int32_t note) override;
		[[nodiscard]] bool any() const;
	};

	void prepare(SoundInstrument& track, ModelStackWithThreeMainThings* modelStack);
	void updateParams(InstrumentClip* clip, SoundInstrument& track);

	Chain chain_;
	Sink sink_;
	Context context_;
	double clock_ = 0;
	std::bitset<128> direct_; ///< notes played straight to the voices while the rack had nothing to do
	Values effective_;        ///< the track's values, plus the p-locks of the locked note being played
	bool lockActive_ = false;
	int32_t lockNote_ = -1;
};

} // namespace deluge::note_fx
