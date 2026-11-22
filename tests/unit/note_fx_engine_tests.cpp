#include "CppUTest/TestHarness.h"
#include "model/note_fx/note_fx_engine.h"
#include <string>
#include <vector>

using namespace deluge::note_fx;

namespace {
struct Recorder : NoteSink {
	std::vector<std::string> events;
	int on[128]{};
	void noteOn(int32_t note, int32_t velocity) override {
		events.push_back("+" + std::to_string(note));
		on[note]++;
	}
	void noteOff(int32_t note) override {
		events.push_back("-" + std::to_string(note));
		if (on[note] > 0) {
			on[note]--;
		}
	}
	std::string ons() const {
		std::string s;
		for (auto& e : events) {
			if (e[0] == '+') {
				s += e.substr(1) + " ";
			}
		}
		return s;
	}
	int sounding() const {
		int n = 0;
		for (int x : on) {
			n += x;
		}
		return n;
	}
};

HeldNotes held(std::initializer_list<int> notes) {
	HeldNotes h;
	int v = 10;
	for (int n : notes) {
		h.add(n, v);
		v += 10;
	}
	return h;
}

std::string pattern(int style, std::initializer_list<int> notes) {
	HeldNotes h = held(notes);
	int16_t out[64];
	int len = buildArpPattern(style, h, out, 64);
	std::string s;
	for (int i = 0; i < len; i++) {
		s += out[i] < 0 ? std::string("-") : std::to_string(h[out[i]].note);
		s += " ";
	}
	return s;
}

int16_t arpParams[kMaxParams];
void arpDefaults() {
	Values v;
	for (int p = 0; p < kMaxParams; p++) {
		arpParams[p] = v.get(Type::ARP, p);
	}
}
} // namespace

TEST_GROUP(NoteFxEngineTests){};

TEST(NoteFxEngineTests, arpPatterns) {
	STRCMP_EQUAL("60 64 67 ", pattern(STYLE_UP, {64, 60, 67}).c_str());
	STRCMP_EQUAL("64 60 67 ", pattern(STYLE_ORDER, {64, 60, 67}).c_str());
	STRCMP_EQUAL("67 64 60 ", pattern(STYLE_DOWN, {64, 60, 67}).c_str());
	STRCMP_EQUAL("60 64 67 64 ", pattern(STYLE_UP_DOWN, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 64 67 67 64 60 ", pattern(STYLE_UP_AND_DOWN, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 71 64 67 ", pattern(STYLE_CONVERGE_UP, {60, 64, 67, 71}).c_str());
	STRCMP_EQUAL("60 67 64 67 ", pattern(STYLE_PINKY_UP, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 64 60 67 ", pattern(STYLE_THUMB_UP, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 64 64 67 67 71 ", pattern(STYLE_REGRESS2_UP, {60, 64, 67, 71}).c_str());
	STRCMP_EQUAL("60 64 64 67 ", pattern(STYLE_DOUBLE_1_2, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 64 - 67 ", pattern(STYLE_SILENCE_1_2, {60, 64, 67}).c_str());
	STRCMP_EQUAL("60 64 67 ", pattern(STYLE_VELOCITY_UP, {60, 64, 67}).c_str()); // velocities rise with press order
	STRCMP_EQUAL("67 64 60 ", pattern(STYLE_VELOCITY_DOWN, {60, 64, 67}).c_str());
}

TEST(NoteFxEngineTests, euclidSpreadsPulses) {
	std::string s;
	for (int i = 0; i < 8; i++) {
		s += euclidHit(i, 8, 3, 0) ? "x" : ".";
	}
	STRCMP_EQUAL("x..x..x.", s.c_str());
	s.clear();
	for (int i = 0; i < 8; i++) {
		s += euclidHit(i, 8, 3, 1) ? "x" : ".";
	}
	STRCMP_EQUAL(".x..x..x", s.c_str());
	CHECK(euclidHit(5, 4, 9, 0)); // more pulses than steps: every step
}

TEST(NoteFxEngineTests, arpStepsThroughHeldNotesAndEndsThem) {
	arpDefaults(); // UP, 1/16, gate 50%, sync ON
	ArpModule arp;
	Recorder r;
	Context c;
	c.barTicks = 192; // 1/16 = 12 ticks
	c.params = arpParams;
	arp.noteOn(64, 100, c, r);
	arp.noteOn(60, 100, c, r);
	arp.noteOn(67, 100, c, r);
	for (int t = 1; t <= 48; t++) {
		c.now = t;
		arp.advance(c, r);
	}
	STRCMP_EQUAL("60 64 67 60 64 ", r.ons().c_str()); // a chord played on one tick starts on its UP order
	arp.noteOff(60, c, r);
	arp.noteOff(64, c, r);
	arp.noteOff(67, c, r);
	c.now = 60;
	arp.advance(c, r);
	CHECK_EQUAL(0, r.sounding());
}

TEST(NoteFxEngineTests, arpStepsOnOffAndOffsets) {
	arpDefaults();
	arpParams[ARP_OCTAVE] = -1; // the note, then an octave lower
	arpParams[ARP_LEN] = 4;
	ArpSteps steps;
	steps.toggle(1);       // step 2 OFF
	steps.setOffset(2, 2); // step 3: +2 semitones
	ArpModule arp;
	Recorder r;
	Context c;
	c.barTicks = 192; // 12 ticks per step
	c.params = arpParams;
	c.arpSteps = &steps;
	arp.noteOn(60, 100, c, r);
	for (int t = 0; t <= 95; t++) { // 8 steps
		c.now = t;
		arp.advance(c, r);
	}
	// arpeggio 60 48 60 48 ...; step 2 silent (its 48 is skipped, the arpeggio moves on), step 3 +2, LEN 4 loops
	STRCMP_EQUAL("60 62 48 60 62 48 ", r.ons().c_str());
	CHECK_EQUAL(-1, steps.offset[0] - 1);
	steps.setOffset(5, 99);
	CHECK_EQUAL(kArpOffsetMax, steps.offset[5]);
}

TEST(NoteFxEngineTests, monoLastLegatoAndRetrig) {
	int16_t p[kMaxParams] = {MONO_LAST, 0, 1};
	MonoModule mono;
	Recorder r;
	Context c;
	c.params = p;
	mono.noteOn(60, 100, c, r);
	mono.noteOn(64, 100, c, r);
	mono.noteOff(64, c, r); // RETRIG: 60 comes back
	mono.noteOff(60, c, r);
	std::string s;
	for (auto& e : r.events) {
		s += e + " ";
	}
	STRCMP_EQUAL("+60 -60 +64 -64 +60 -60 ", s.c_str());
	CHECK_EQUAL(0, r.sounding());

	p[MONO_LEGATO] = 1;
	p[MONO_MODE] = MONO_HIGH;
	r.events.clear();
	mono.noteOn(60, 100, c, r);
	mono.noteOn(55, 100, c, r); // lower: ignored in HIGH
	mono.noteOn(67, 100, c, r); // higher: takes over, overlapping (legato)
	s.clear();
	for (auto& e : r.events) {
		s += e + " ";
	}
	STRCMP_EQUAL("+60 +67 -60 ", s.c_str());
	mono.reset(r);
	CHECK_EQUAL(0, r.sounding());
}

TEST(NoteFxEngineTests, euclidFixedNoteRunsWithTheTransport) {
	Values v;
	int16_t p[kMaxParams];
	for (int i = 0; i < kMaxParams; i++) {
		p[i] = v.get(Type::EUCL, i);
	}
	p[EUCL_STEPS] = 4;
	p[EUCL_PULSES] = 2; // x.x.
	EuclidModule e;
	Recorder r;
	Context c;
	c.barTicks = 192; // 1/16 = 12 ticks
	c.params = p;
	c.trackNote = 48;
	c.now = 0;
	e.advance(c, r); // stopped: nothing
	CHECK_EQUAL(0, (int)r.events.size());
	c.playing = true;
	for (int t = 0; t < 48; t++) {
		c.now = t;
		e.advance(c, r);
	}
	STRCMP_EQUAL("48 48 ", r.ons().c_str()); // steps 0 and 2 of 0..3
	e.reset(r);
	CHECK_EQUAL(0, r.sounding());
}

TEST(NoteFxEngineTests, chainRunsModulesInOrderAndCleansUp) {
	Rack rack;
	rack.add(0, Type::ARP);
	rack.add(1, Type::MONO);
	Values values;
	Chain chain;
	Recorder r;
	Context c;
	c.barTicks = 192;
	chain.configure(rack, r, c);
	CHECK_EQUAL(2, chain.count());
	int16_t arp[kMaxParams];
	int16_t mono[kMaxParams];
	for (int i = 0; i < kMaxParams; i++) {
		arp[i] = values.get(Type::ARP, i);
		mono[i] = values.get(Type::MONO, i);
	}
	arp[ARP_GATE] = 150; // overlapping arp notes ...
	chain.setParams(0, arp);
	chain.setParams(1, mono); // ... made monophonic
	chain.noteOn(60, 100, c, r);
	chain.noteOn(64, 100, c, r);
	for (int t = 1; t <= 48; t++) {
		c.now = t;
		chain.advance(c, r);
		CHECK(r.sounding() <= 1);
	}
	// switching MONO off rebuilds the chain: everything ends, the held keys go into the new chain
	rack.toggle(1);
	chain.configure(rack, r, c);
	CHECK_EQUAL(1, chain.count());
	chain.setParams(0, arp);
	chain.noteOff(60, c, r);
	chain.noteOff(64, c, r);
	c.now = 200;
	chain.advance(c, r);
	CHECK_EQUAL(0, r.sounding());
}

TEST(NoteFxEngineTests, arpStyleTurnedWhilePlayingTakesEffect) {
	arpDefaults(); // UP
	ArpModule arp;
	Recorder r;
	Context c;
	c.barTicks = 192; // 12 ticks per step
	c.params = arpParams;
	arp.noteOn(60, 100, c, r);
	arp.noteOn(64, 100, c, r);
	arp.noteOn(67, 100, c, r);
	for (int t = 1; t <= 24; t++) { // steps at 0, 12, 24: 60 64 67
		c.now = t;
		arp.advance(c, r);
	}
	arpParams[ARP_STYLE] = STYLE_DOWN; // turned while the keys are held
	for (int t = 25; t <= 60; t++) {
		c.now = t;
		arp.advance(c, r);
	}
	STRCMP_EQUAL("60 64 67 67 64 60 ", r.ons().c_str()); // UP cycle done, then DOWN from its start
}

TEST(NoteFxEngineTests, euclidGateFollowsTheTrackLen) {
	Values v;
	int16_t p[kMaxParams];
	for (int i = 0; i < kMaxParams; i++) {
		p[i] = v.get(Type::EUCL, i);
	}
	CHECK_EQUAL(kEuclGateTrack, p[EUCL_GATE]); // GATE follows the track's LEN by default
	p[EUCL_STEPS] = 4;
	p[EUCL_PULSES] = 1; // x...
	auto offTickOfFirstNote = [&](int32_t lenEighths) {
		EuclidModule e;
		Recorder r;
		Context c;
		c.barTicks = 192; // 12 ticks per step
		c.params = p;
		c.playing = true;
		c.trackLenEighths = lenEighths;
		for (int t = 0; t < 47; t++) {
			c.now = t;
			e.advance(c, r);
			for (auto& ev : r.events) {
				if (ev[0] == '-') {
					return t;
				}
			}
		}
		return -1;
	};
	CHECK_EQUAL(6, offTickOfFirstNote(4));   // LEN 0.5: half a step
	CHECK_EQUAL(24, offTickOfFirstNote(16)); // LEN 2: two steps
	CHECK_EQUAL(-1, offTickOfFirstNote(0));  // LEN INF: lasts up to the next pulse (4 steps away)
}
