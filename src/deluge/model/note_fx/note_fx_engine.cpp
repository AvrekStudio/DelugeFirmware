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

#include "model/note_fx/note_fx_engine.h"
#include <algorithm>
#include <cmath>

namespace deluge::note_fx {

// ---- PendingOffs

void PendingOffs::add(int32_t note, double offTime) {
	if (count_ < kMaxPending) {
		entries_[count_++] = {static_cast<int16_t>(note), offTime};
	}
}

void PendingOffs::process(double upTo, NoteSink& out) {
	for (int32_t i = 0; i < count_;) {
		if (entries_[i].off <= upTo) {
			int32_t note = entries_[i].note;
			entries_[i] = entries_[--count_];
			out.noteOff(note);
		}
		else {
			i++;
		}
	}
}

bool PendingOffs::endNow(int32_t note, NoteSink& out) {
	for (int32_t i = 0; i < count_; i++) {
		if (entries_[i].note == note) {
			entries_[i] = entries_[--count_];
			out.noteOff(note);
			return true;
		}
	}
	return false;
}

void PendingOffs::flush(NoteSink& out) {
	while (count_ > 0) {
		out.noteOff(entries_[--count_].note);
	}
}

// ---- HeldNotes

bool HeldNotes::add(int32_t note, int32_t velocity) {
	for (int32_t i = 0; i < count_; i++) {
		if (notes_[i].note == note) {
			return false;
		}
	}
	if (count_ >= kMaxHeld) {
		return false;
	}
	notes_[count_++] = {static_cast<int16_t>(note), static_cast<uint8_t>(velocity)};
	return true;
}

bool HeldNotes::remove(int32_t note) {
	for (int32_t i = 0; i < count_; i++) {
		if (notes_[i].note == note) {
			for (int32_t j = i; j < count_ - 1; j++) {
				notes_[j] = notes_[j + 1];
			}
			count_--;
			return true;
		}
	}
	return false;
}

// ---- patterns

int32_t buildArpPattern(int32_t style, const HeldNotes& held, int16_t* out, int32_t maxOut) {
	const int32_t n = held.count();
	if (n == 0) {
		return 0;
	}
	// indexes sorted by pitch
	int16_t asc[kMaxHeld];
	for (int32_t i = 0; i < n; i++) {
		asc[i] = static_cast<int16_t>(i);
	}
	std::sort(asc, asc + n, [&](int16_t a, int16_t b) { return held[a].note < held[b].note; });
	int16_t desc[kMaxHeld];
	for (int32_t i = 0; i < n; i++) {
		desc[i] = asc[n - 1 - i];
	}

	int32_t len = 0;
	auto push = [&](int32_t v) {
		if (len < maxOut) {
			out[len++] = static_cast<int16_t>(v);
		}
	};
	auto pushAll = [&](const int16_t* a, int32_t from, int32_t to) { // [from, to)
		for (int32_t i = from; i < to; i++) {
			push(a[i]);
		}
	};
	// lo, hi, lo+1, hi-1 ... (starting from `a`'s first)
	auto converge = [&](const int16_t* a, int16_t* dst) {
		int32_t lo = 0;
		int32_t hi = n - 1;
		int32_t k = 0;
		while (lo <= hi) {
			dst[k++] = a[lo++];
			if (lo <= hi) {
				dst[k++] = a[hi--];
			}
		}
	};

	switch (style) {
	case STYLE_ORDER:
		for (int32_t i = 0; i < n; i++) {
			push(i);
		}
		break;
	case STYLE_UP:
		pushAll(asc, 0, n);
		break;
	case STYLE_DOWN:
		pushAll(desc, 0, n);
		break;
	case STYLE_UP_DOWN: // c e g e
		pushAll(asc, 0, n);
		for (int32_t i = n - 2; i >= 1; i--) {
			push(asc[i]);
		}
		break;
	case STYLE_DOWN_UP:
		pushAll(desc, 0, n);
		for (int32_t i = n - 2; i >= 1; i--) {
			push(desc[i]);
		}
		break;
	case STYLE_UP_AND_DOWN: // c e g g e c
		pushAll(asc, 0, n);
		pushAll(desc, 0, n);
		break;
	case STYLE_DOWN_AND_UP:
		pushAll(desc, 0, n);
		pushAll(asc, 0, n);
		break;
	case STYLE_RANDOM:
		for (int32_t i = 0; i < n; i++) {
			push(-2);
		}
		break;
	case STYLE_VELOCITY_UP:
	case STYLE_VELOCITY_DOWN: {
		int16_t byVel[kMaxHeld];
		for (int32_t i = 0; i < n; i++) {
			byVel[i] = static_cast<int16_t>(i);
		}
		std::stable_sort(byVel, byVel + n, [&](int16_t a, int16_t b) {
			return style == STYLE_VELOCITY_UP ? held[a].velocity < held[b].velocity
			                                  : held[a].velocity > held[b].velocity;
		});
		pushAll(byVel, 0, n);
		break;
	}
	case STYLE_CONVERGE_UP:
	case STYLE_CONVERGE_DOWN:
	case STYLE_DIVERGE_UP:
	case STYLE_DIVERGE_DOWN:
	case STYLE_CONVERGE_DIVERGE:
	case STYLE_DIVERGE_CONVERGE: {
		int16_t conv[kMaxHeld];
		const bool fromHigh = style == STYLE_CONVERGE_DOWN || style == STYLE_DIVERGE_UP;
		converge(fromHigh ? desc : asc, conv);
		int16_t div[kMaxHeld]; // diverging = the converging order backwards (from the middle out)
		for (int32_t i = 0; i < n; i++) {
			div[i] = conv[n - 1 - i];
		}
		if (style == STYLE_CONVERGE_UP || style == STYLE_CONVERGE_DOWN) {
			pushAll(conv, 0, n);
		}
		else if (style == STYLE_DIVERGE_UP || style == STYLE_DIVERGE_DOWN) {
			pushAll(div, 0, n);
		}
		else if (style == STYLE_CONVERGE_DIVERGE) {
			pushAll(conv, 0, n);
			pushAll(div, 1, n - 1);
		}
		else {
			pushAll(div, 0, n);
			pushAll(conv, 1, n - 1);
		}
		break;
	}
	case STYLE_PINKY_UP: // c g e g: the top note between each of the others, going up
	case STYLE_PINKY_DOWN:
		if (n == 1) {
			push(asc[0]);
			break;
		}
		for (int32_t i = 0; i < n - 1; i++) {
			push(style == STYLE_PINKY_UP ? asc[i] : asc[n - 2 - i]);
			push(asc[n - 1]);
		}
		break;
	case STYLE_THUMB_UP: // c e c g: the bottom note between each of the others
	case STYLE_THUMB_DOWN:
		if (n == 1) {
			push(asc[0]);
			break;
		}
		for (int32_t i = 1; i < n; i++) {
			push(asc[0]);
			push(style == STYLE_THUMB_UP ? asc[i] : asc[n - i]);
		}
		break;
	case STYLE_REGRESS2_UP:
	case STYLE_REGRESS3_UP:
	case STYLE_REGRESS4_UP:
	case STYLE_REGRESS2_DOWN:
	case STYLE_REGRESS3_DOWN:
	case STYLE_REGRESS4_DOWN: { // groups of k notes, each group starting one note further: c e, e g, g b
		const bool up = style <= STYLE_REGRESS4_UP;
		const int32_t k = 2 + (style - (up ? STYLE_REGRESS2_UP : STYLE_REGRESS2_DOWN));
		const int16_t* a = up ? asc : desc;
		if (n <= k) {
			pushAll(a, 0, n);
			break;
		}
		for (int32_t start = 0; start + k <= n; start++) {
			pushAll(a, start, start + k);
		}
		break;
	}
	case STYLE_DOUBLE_1_2:
	case STYLE_DOUBLE_1_3:
	case STYLE_SILENCE_1_2:
	case STYLE_SILENCE_1_3: { // going up, every 2nd / 3rd note played twice - or followed by a rest
		const int32_t every = (style == STYLE_DOUBLE_1_2 || style == STYLE_SILENCE_1_2) ? 2 : 3;
		const bool silence = style == STYLE_SILENCE_1_2 || style == STYLE_SILENCE_1_3;
		for (int32_t i = 0; i < n; i++) {
			push(asc[i]);
			if (i % every == every - 1) {
				push(silence ? -1 : asc[i]);
			}
		}
		break;
	}
	default:
		pushAll(asc, 0, n);
		break;
	}
	return len;
}

bool euclidHit(int32_t step, int32_t steps, int32_t pulses, int32_t rotate) {
	if (steps <= 0 || pulses <= 0) {
		return false;
	}
	if (pulses >= steps) {
		return true;
	}
	int32_t s = ((step - rotate) % steps + steps) % steps;
	// Bresenham: pulses spread as evenly as possible, the first step is a pulse
	return (s * pulses) % steps < pulses;
}

double rateTicks(int32_t rateIndex, double barTicks) {
	rateIndex = std::clamp<int32_t>(rateIndex, 0, kNumRates - 1);
	return barTicks / kRateDivisions[rateIndex];
}

double freeRateTicks(int32_t percent, double barTicks) {
	// 0% = a whole bar per step, 100% = 1/64, exponentially in between
	return barTicks / std::pow(64.0, std::clamp<int32_t>(percent, 0, 100) / 100.0);
}

// ---- ARP

double ArpModule::stepLength(const Context& c) const {
	if (c.params[ARP_SYNC] == SYNC_OFF) {
		return freeRateTicks(c.params[ARP_FREE_RATE], c.barTicks);
	}
	return rateTicks(c.params[ARP_RATE], c.barTicks);
}

void ArpModule::rebuild(const Context& c) {
	patternLength_ = buildArpPattern(c.params[ARP_STYLE], held_, pattern_.data(), pattern_.size());
}

void ArpModule::updateChord(const Context& c, NoteSink& out) {
	// CHORD: the held notes as a chord under the arpeggio, 1 or 2 octaves down (or up)
	for (int32_t i = 0; i < chordCount_; i++) {
		out.noteOff(chordNotes_[i]);
	}
	chordCount_ = 0;
	const int32_t shift = c.params[ARP_CHORD] * 12;
	if (shift == 0 || !running_) {
		return;
	}
	for (int32_t i = 0; i < held_.count(); i++) {
		int32_t note = held_[i].note + shift;
		if (note >= 0 && note <= 127) {
			chordNotes_[chordCount_++] = static_cast<int16_t>(note);
			out.noteOn(note, held_[i].velocity);
		}
	}
}

void ArpModule::noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) {
	const bool wasEmpty = held_.count() == 0;
	if (!held_.add(note, velocity)) {
		return;
	}
	rebuild(c);
	const int32_t retrig = c.params[ARP_RETRIG];
	if (wasEmpty || retrig == 1 /* NOTE */) {
		position_ = 0;
		cycles_ = 0;
	}
	if (wasEmpty || !running_) {
		running_ = true;
		const double step = stepLength(c);
		if (c.params[ARP_SYNC] == SYNC_LOCKED) {
			nextStep_ = std::ceil(c.now / step - 1e-6) * step; // strictly on the grid
		}
		else {
			nextStep_ = c.now; // SYNC ON keeps the first key's offset; OFF is free-running
		}
		lastRetrigTime_ = c.now;
	}
	updateChord(c, out);
	// The first step plays at the next advance(), once every note arriving with this one (a chord on the same tick)
	// is in.
}

void ArpModule::noteOff(int32_t note, const Context& c, NoteSink& out) {
	if (!held_.remove(note)) {
		return;
	}
	rebuild(c);
	if (held_.count() == 0) {
		running_ = false;
		position_ = 0;
	}
	else if (patternLength_ > 0) {
		position_ %= patternLength_ * std::max<int32_t>(1, std::abs(c.params[ARP_OCTAVE]) + 1);
	}
	updateChord(c, out);
}

void ArpModule::playStep(double at, const Context& c, NoteSink& out) {
	const int32_t octaves = std::abs(c.params[ARP_OCTAVE]) + 1;
	const int32_t total = patternLength_ * octaves;
	if (total <= 0) {
		return;
	}
	const int32_t repeat = c.params[ARP_REPEAT];
	if (repeat > 0 && cycles_ >= repeat) {
		return; // REPEAT: done until re-triggered
	}
	const int32_t idx = position_ % total;
	const int32_t octave = idx / patternLength_;
	int32_t entry = pattern_[idx % patternLength_];
	if (entry == -2) {
		entry = random.below(held_.count());
	}
	position_++;
	if (position_ >= total) {
		position_ = 0;
		cycles_++;
	}
	if (entry < 0 || entry >= held_.count()) {
		return; // a rest
	}
	const int32_t direction = c.params[ARP_OCTAVE] < 0 ? -1 : 1;
	const int32_t note = held_[entry].note + direction * 12 * octave;
	if (note < 0 || note > 127) {
		return;
	}
	int32_t velocity = held_[entry].velocity;
	double gate = c.params[ARP_GATE] / 100.0;
	const int32_t human = c.params[ARP_HUMAN];
	if (human > 0) {
		velocity += (random.below(2 * human + 1) - human) * 40 / 100;
		gate *= 1.0 + (random.below(2 * human + 1) - human) / 200.0;
	}
	velocity = std::clamp<int32_t>(velocity, 1, 127);
	const double length = std::max(1.0, stepLength(c) * gate);
	offs_.endNow(note, out); // the same note still sounding (gate > 100%) ends first
	out.noteOn(note, velocity);
	offs_.add(note, at + length);
}

void ArpModule::advance(const Context& c, NoteSink& out) {
	offs_.process(c.now, out);
	if (!running_ || held_.count() == 0) {
		return;
	}
	// RE-TRIG on bars / note values
	const int32_t retrig = c.params[ARP_RETRIG];
	if (retrig >= 2) {
		const double every = c.barTicks * kRetrigBars192[retrig] / 192.0;
		if (std::floor(c.now / every) > std::floor(lastRetrigTime_ / every)) {
			position_ = 0;
			cycles_ = 0;
		}
	}
	lastRetrigTime_ = c.now;

	const double step = stepLength(c);
	if (nextStep_ > c.now + step) {
		nextStep_ = c.now; // time went back (playback restarted / looped): carry on from now
	}
	int32_t guard = 0;
	while (nextStep_ <= c.now && guard++ < 64) {
		playStep(nextStep_, c, out);
		nextStep_ += step;
	}
	if (nextStep_ < c.now - step) {
		nextStep_ = c.now; // fell far behind (tempo change): catch up
	}
	offs_.process(c.now, out);
}

void ArpModule::reset(NoteSink& out) {
	offs_.flush(out);
	for (int32_t i = 0; i < chordCount_; i++) {
		out.noteOff(chordNotes_[i]);
	}
	chordCount_ = 0;
	held_.clear();
	running_ = false;
	position_ = 0;
	cycles_ = 0;
	patternLength_ = 0;
}

// ---- EUCLID

void EuclidModule::noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) {
	if (c.params[EUCL_NOTE] != kEuclNoteIn) {
		out.noteOn(note, velocity); // a fixed note: what comes in goes on unchanged
		return;
	}
	held_.add(note, velocity);
}

void EuclidModule::noteOff(int32_t note, const Context& c, NoteSink& out) {
	if (c.params[EUCL_NOTE] != kEuclNoteIn) {
		out.noteOff(note);
		return;
	}
	held_.remove(note);
}

void EuclidModule::advance(const Context& c, NoteSink& out) {
	offs_.process(c.now, out);
	const int32_t noteParam = c.params[EUCL_NOTE];
	const bool fixed = noteParam != kEuclNoteIn;
	// a fixed note runs with the transport; IN runs while notes are held
	const bool run = fixed ? c.playing : held_.count() > 0;
	if (!run) {
		started_ = false;
		lastTime_ = c.now;
		return;
	}
	const double step = rateTicks(c.params[EUCL_RATE], c.barTicks);
	if (!started_ || c.now < lastTime_) { // (time went back: playback restarted / looped)
		started_ = true;
		lastTime_ = c.now - 1e-6; // a step right at now plays
	}
	const int32_t steps = c.params[EUCL_STEPS];
	const int32_t pulses = std::min<int32_t>(c.params[EUCL_PULSES], steps);
	int64_t first = static_cast<int64_t>(std::floor(lastTime_ / step)) + 1;
	int64_t last = static_cast<int64_t>(std::floor(c.now / step));
	if (last - first > 64) {
		first = last; // fell far behind: don't play a burst
	}
	for (int64_t k = first; k <= last; k++) {
		const int32_t s = static_cast<int32_t>(k % steps);
		if (!euclidHit(s, steps, pulses, c.params[EUCL_ROTATE])) {
			continue;
		}
		const double length = std::max(1.0, step * c.params[EUCL_GATE] / 100.0);
		const double at = static_cast<double>(k) * step;
		if (fixed) {
			const int32_t note = noteParam == kEuclNoteTrack ? c.trackNote : noteParam;
			offs_.endNow(note, out);
			out.noteOn(note, 100);
			offs_.add(note, at + length);
		}
		else {
			for (int32_t i = 0; i < held_.count(); i++) {
				offs_.endNow(held_[i].note, out);
				out.noteOn(held_[i].note, held_[i].velocity);
				offs_.add(held_[i].note, at + length);
			}
		}
	}
	lastTime_ = c.now;
	offs_.process(c.now, out);
}

void EuclidModule::reset(NoteSink& out) {
	offs_.flush(out);
	held_.clear();
	started_ = false;
}

// ---- MONO

void MonoModule::switchTo(int32_t note, int32_t velocity, bool legato, NoteSink& out) {
	const int32_t old = sounding_;
	if (old != kNoNote && !legato) {
		out.noteOff(old);
	}
	out.noteOn(note, velocity);
	if (old != kNoNote && legato && old != note) {
		out.noteOff(old); // LEGATO: the notes overlap slightly
	}
	sounding_ = note;
}

void MonoModule::noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) {
	if (!held_.add(note, velocity)) {
		return;
	}
	bool take = sounding_ == kNoNote;
	if (!take) {
		switch (c.params[MONO_MODE]) {
		case MONO_FIRST:
			take = false;
			break;
		case MONO_LAST:
			take = true;
			break;
		case MONO_RANDOM:
			take = random.below(2) == 0;
			break;
		case MONO_LOW:
			take = note < sounding_;
			break;
		case MONO_HIGH:
			take = note > sounding_;
			break;
		}
	}
	if (take) {
		switchTo(note, velocity, c.params[MONO_LEGATO] != 0, out);
	}
}

void MonoModule::noteOff(int32_t note, const Context& c, NoteSink& out) {
	if (!held_.remove(note)) {
		return;
	}
	if (note != sounding_) {
		return;
	}
	if (c.params[MONO_RETRIG] == 0 || held_.count() == 0) {
		out.noteOff(note);
		sounding_ = kNoNote;
		return;
	}
	// RETRIG: another held note takes over, chosen by MODE
	int32_t pick = 0;
	switch (c.params[MONO_MODE]) {
	case MONO_FIRST:
		pick = 0;
		break;
	case MONO_LAST:
		pick = held_.count() - 1;
		break;
	case MONO_RANDOM:
		pick = random.below(held_.count());
		break;
	case MONO_LOW:
	case MONO_HIGH:
		for (int32_t i = 1; i < held_.count(); i++) {
			const bool better =
			    c.params[MONO_MODE] == MONO_LOW ? held_[i].note < held_[pick].note : held_[i].note > held_[pick].note;
			if (better) {
				pick = i;
			}
		}
		break;
	}
	switchTo(held_[pick].note, held_[pick].velocity, c.params[MONO_LEGATO] != 0, out);
}

void MonoModule::reset(NoteSink& out) {
	if (sounding_ != kNoNote) {
		out.noteOff(sounding_);
	}
	sounding_ = kNoNote;
	held_.clear();
}

// ---- Chain

void Chain::Link::noteOn(int32_t note, int32_t velocity) {
	chain->inject(index, true, note, velocity, *context, *out);
}

void Chain::Link::noteOff(int32_t note) {
	chain->inject(index, false, note, 0, *context, *out);
}

Context Chain::contextFor(int32_t i, const Context& base) const {
	Context c = base;
	c.params = params_[i];
	return c;
}

void Chain::inject(int32_t index, bool on, int32_t note, int32_t velocity, const Context& base, NoteSink& out) {
	if (index >= count_) {
		if (on) {
			out.noteOn(note, velocity);
		}
		else {
			out.noteOff(note);
		}
		return;
	}
	Link next;
	next.chain = this;
	next.index = index + 1;
	next.out = &out;
	next.context = &base;
	const Context c = contextFor(index, base);
	if (c.params == nullptr) { // no parameters (e.g. while resetting): pass through
		if (on) {
			next.noteOn(note, velocity);
		}
		else {
			next.noteOff(note);
		}
		return;
	}
	if (on) {
		modules_[index]->noteOn(note, velocity, c, next);
	}
	else {
		modules_[index]->noteOff(note, c, next);
	}
}

bool Chain::configure(const Rack& rack, NoteSink& out, const Context& c) {
	uint32_t signature = 0;
	for (const Slot& s : rack.slots) {
		const bool processes = s.on && (s.type == Type::ARP || s.type == Type::EUCL || s.type == Type::MONO);
		signature = signature * 7 + (processes ? static_cast<uint32_t>(s.type) : 0);
	}
	if (signature == signature_) {
		return false;
	}
	reset(out);
	signature_ = signature;
	count_ = 0;
	for (const Slot& s : rack.slots) {
		if (!s.on) {
			continue;
		}
		Module* m = s.type == Type::ARP    ? static_cast<Module*>(&arp_)
		            : s.type == Type::EUCL ? static_cast<Module*>(&eucl_)
		            : s.type == Type::MONO ? static_cast<Module*>(&mono_)
		                                   : nullptr;
		if (m != nullptr) {
			types_[count_] = s.type;
			modules_[count_++] = m;
		}
	}
	// keys still held go into the new chain
	for (int32_t i = 0; i < input_.count(); i++) {
		inject(0, true, input_[i].note, input_[i].velocity, c, out);
	}
	return true;
}

void Chain::noteOn(int32_t note, int32_t velocity, const Context& c, NoteSink& out) {
	input_.add(note, velocity);
	inject(0, true, note, velocity, c, out);
}

void Chain::noteOff(int32_t note, const Context& c, NoteSink& out) {
	input_.remove(note);
	inject(0, false, note, 0, c, out);
}

void Chain::advance(const Context& base, NoteSink& out) {
	for (int32_t i = 0; i < count_; i++) {
		Link next;
		next.chain = this;
		next.index = i + 1;
		next.out = &out;
		next.context = &base;
		const Context c = contextFor(i, base);
		if (c.params != nullptr) {
			modules_[i]->advance(c, next);
		}
	}
}

void Chain::reset(NoteSink& out) {
	// each module ends what it started; downstream modules get those note offs before they are reset in turn
	for (int32_t i = 0; i < count_; i++) {
		Link next;
		next.chain = this;
		next.index = i + 1;
		next.out = &out;
		Context dummy;
		next.context = &dummy;
		modules_[i]->reset(next);
	}
}

} // namespace deluge::note_fx
