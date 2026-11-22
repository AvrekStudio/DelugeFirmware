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

#include "model/note_fx/note_fx_params.h"
#include <array>
#include <cstdint>

/// NOTE FX engine: the modules that make, change or remove notes (ARP, EUCL, MONO) and the chain that runs a clip's
/// rack. Time is in ticks (the song's), passed in by whoever drives it. Pure logic, no firmware dependency (unit
/// tested).
namespace deluge::note_fx {

constexpr int32_t kMaxHeld = 16;
constexpr int32_t kMaxPending = 32;
constexpr int32_t kNoNote = -1;

/// Where a module sends its notes.
class NoteSink {
public:
	virtual ~NoteSink() = default;
	virtual void noteOn(int32_t note, int32_t velocity) = 0;
	virtual void noteOff(int32_t note) = 0;
};

/// What a module needs to know about time and the track.
struct Context {
	double now = 0;                  ///< current tick
	double barTicks = 192;           ///< ticks in a bar
	bool playing = false;            ///< transport running
	int32_t trackNote = 60;          ///< the track's default NOTE (MASTER menu)
	int32_t trackLenEighths = 8;     ///< the track's LEN (MASTER menu): eighths of a step, 0 = INF
	const int16_t* params = nullptr; ///< this module's effective parameter values
};

/// Small deterministic random generator (xorshift).
struct Random {
	uint32_t state = 0x9E3779B9;
	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
	int32_t below(int32_t n) { return n > 0 ? static_cast<int32_t>(next() % static_cast<uint32_t>(n)) : 0; }
};

/// Notes a module has started and must end - played into the sink with their end time.
class PendingOffs {
public:
	void add(int32_t note, double offTime);
	/// Ends the notes due before `upTo`.
	void process(double upTo, NoteSink& out);
	/// Ends `note` now if it's pending (before it is started again).
	bool endNow(int32_t note, NoteSink& out);
	void flush(NoteSink& out);
	[[nodiscard]] bool empty() const { return count_ == 0; }

private:
	struct Entry {
		int16_t note;
		double off;
	};
	std::array<Entry, kMaxPending> entries_{};
	int32_t count_ = 0;
};

/// The notes held at a module's input, in the order they came.
class HeldNotes {
public:
	struct Note {
		int16_t note;
		uint8_t velocity;
	};
	bool add(int32_t note, int32_t velocity);
	bool remove(int32_t note);
	void clear() { count_ = 0; }
	[[nodiscard]] int32_t count() const { return count_; }
	[[nodiscard]] const Note& operator[](int32_t i) const { return notes_[i]; }

private:
	std::array<Note, kMaxHeld> notes_{};
	int32_t count_ = 0;
};

/// The order an ARP style plays the held notes in: indexes into `held`, -1 = a rest, -2 = a random note.
/// Returns the pattern's length (one cycle, before octaves).
int32_t buildArpPattern(int32_t style, const HeldNotes& held, int16_t* out, int32_t maxOut);

/// Euclidean rhythm: is `step` (0..steps-1) a pulse?
bool euclidHit(int32_t step, int32_t steps, int32_t pulses, int32_t rotate);

/// A step length in ticks for a rate index (synced) or a free rate (0..100%).
double rateTicks(int32_t rateIndex, double barTicks);
double freeRateTicks(int32_t percent, double barTicks);

class Module {
public:
	virtual ~Module() = default;
	virtual void noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) = 0;
	virtual void noteOff(int32_t note, const Context& c, NoteSink& out) = 0;
	/// Time moved on to c.now.
	virtual void advance(const Context& c, NoteSink& out) = 0;
	/// Ends everything this module started and forgets its state.
	virtual void reset(NoteSink& out) = 0;
};

class ArpModule final : public Module {
public:
	void noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) override;
	void noteOff(int32_t note, const Context& c, NoteSink& out) override;
	void advance(const Context& c, NoteSink& out) override;
	void reset(NoteSink& out) override;
	Random random;

private:
	void rebuild(const Context& c);
	void playStep(double at, const Context& c, NoteSink& out);
	void updateChord(const Context& c, NoteSink& out);
	double stepLength(const Context& c) const;

	HeldNotes held_;
	PendingOffs offs_;
	std::array<int16_t, 64> pattern_{};
	int32_t patternLength_ = 0;
	int32_t builtStyle_ = -1; ///< STYLE the pattern was built with (rebuilt when it changes, e.g. turned while playing)
	int32_t position_ = 0;    ///< step within pattern x octaves
	int32_t cycles_ = 0;      ///< full cycles played (REPEAT)
	bool running_ = false;
	double nextStep_ = 0;
	double lastRetrigTime_ = 0;
	std::array<int16_t, kMaxHeld> chordNotes_{};
	int32_t chordCount_ = 0;
};

class EuclidModule final : public Module {
public:
	void noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) override;
	void noteOff(int32_t note, const Context& c, NoteSink& out) override;
	void advance(const Context& c, NoteSink& out) override;
	void reset(NoteSink& out) override;

private:
	HeldNotes held_;
	PendingOffs offs_;
	double lastTime_ = -1;
	bool started_ = false;
};

class MonoModule final : public Module {
public:
	void noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) override;
	void noteOff(int32_t note, const Context& c, NoteSink& out) override;
	void advance(const Context& c, NoteSink& out) override {}
	void reset(NoteSink& out) override;
	Random random;

private:
	void switchTo(int32_t note, int32_t velocity, bool legato, NoteSink& out);
	HeldNotes held_;
	int32_t sounding_ = kNoNote;
};

/// One clip's rack, running: input notes go through the switched-on modules in slot order to `out`.
class Chain {
public:
	/// Rebuilds the module list if the rack's modules / order / on-off changed (ending all notes first, then playing
	/// the held input notes into the new chain). Returns true if it did.
	bool configure(const Rack& rack, NoteSink& out, const Context& c);
	void noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out);
	void noteOff(int32_t note, const Context& c, NoteSink& out);
	/// Time moved on to base.now: each module runs with its parameters (setParams).
	void advance(const Context& base, NoteSink& out);
	void reset(NoteSink& out);
	/// True if anything in the chain changes notes (otherwise notes can go straight through).
	[[nodiscard]] bool active() const { return count_ > 0; }
	[[nodiscard]] Type typeAt(int32_t i) const { return types_[i]; }
	[[nodiscard]] int32_t count() const { return count_; }

	/// Parameters each module uses: the track's values, or a p-lock's while a locked note is held.
	void setParams(int32_t i, const int16_t* params) { params_[i] = params; }

private:
	class Link final : public NoteSink {
	public:
		Chain* chain = nullptr;
		int32_t index = 0; ///< the module notes go to next (count_ = the output)
		NoteSink* out = nullptr;
		const Context* context = nullptr;
		void noteOn(int32_t note, int32_t velocity) override;
		void noteOff(int32_t note) override;
	};
	void inject(int32_t index, bool on, int32_t note, int32_t velocity, const Context& c, NoteSink& out);
	Context contextFor(int32_t i, const Context& base) const;

	ArpModule arp_;
	EuclidModule eucl_;
	MonoModule mono_;
	std::array<Module*, kNumSlots> modules_{};
	std::array<Type, kNumSlots> types_{};
	std::array<const int16_t*, kNumSlots> params_{};
	int32_t count_ = 0;
	uint32_t signature_ = 0xFFFFFFFF;
	HeldNotes input_;
};

} // namespace deluge::note_fx
