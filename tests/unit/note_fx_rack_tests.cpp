#include "CppUTest/TestHarness.h"
#include "model/note_fx/note_fx_rack.h"

using namespace deluge::note_fx;

TEST_GROUP(NoteFxRackTests){};

TEST(NoteFxRackTests, startsEmptyAndOffersEveryModule) {
	Rack r;
	for (const Slot& s : r.slots) {
		CHECK(s.empty());
	}
	Type list[8];
	CHECK_EQUAL(1, r.available(list, 8));
	CHECK(list[0] == Type::DIR);
}

TEST(NoteFxRackTests, addOnceOnlyAndIntoEmptySlots) {
	Rack r;
	CHECK(r.add(2, Type::DIR));
	CHECK(r.slots[2].type == Type::DIR);
	CHECK(r.slots[2].on);
	CHECK_FALSE(r.add(3, Type::DIR)); // no duplicate
	CHECK_FALSE(r.add(2, Type::DIR)); // slot taken
	CHECK_FALSE(r.add(8, Type::DIR)); // out of range
	CHECK_FALSE(r.add(4, Type::NONE));
	Type list[8];
	CHECK_EQUAL(0, r.available(list, 8)); // DIR is used: nothing left to offer
}

TEST(NoteFxRackTests, moveSwapsAcrossRowsAndStopsAtEnds) {
	Rack r;
	r.add(3, Type::DIR);
	CHECK_EQUAL(4, r.move(3, +1)); // top row's last slot -> bottom row's first
	CHECK(r.slots[4].type == Type::DIR);
	CHECK(r.slots[3].empty());
	CHECK_EQUAL(3, r.move(4, -1));
	r.remove(3);
	r.add(0, Type::DIR);
	CHECK_EQUAL(-1, r.move(0, -1));
	r.remove(0);
	r.add(7, Type::DIR);
	CHECK_EQUAL(-1, r.move(7, +1));
	CHECK_EQUAL(-1, r.move(5, +1)); // empty slot doesn't move
}

TEST(NoteFxRackTests, toggleRemoveAndActive) {
	Rack r;
	CHECK_FALSE(r.active(Type::DIR));
	r.add(1, Type::DIR);
	CHECK(r.active(Type::DIR));
	CHECK(r.toggle(1));
	CHECK_FALSE(r.active(Type::DIR));
	CHECK(r.contains(Type::DIR));
	CHECK_FALSE(r.toggle(0));
	CHECK(r.remove(1));
	CHECK_FALSE(r.contains(Type::DIR));
	CHECK_FALSE(r.remove(1));
}

TEST(NoteFxRackTests, saveRoundTripAndRejectsGarbage) {
	Rack r;
	r.add(5, Type::DIR);
	r.toggle(5);
	r.dirMode = 2;
	uint8_t b[Rack::kNumSaveBytes];
	r.toBytes(b);
	Rack back;
	back.fromBytes(b, Rack::kNumSaveBytes);
	CHECK(back.slots[5].type == Type::DIR);
	CHECK_FALSE(back.slots[5].on);
	CHECK_EQUAL(2, back.dirMode);

	uint8_t bad[Rack::kNumSaveBytes] = {1, 1, 1, 1, 99, 1}; // DIR twice, unknown type
	bad[Rack::kNumSaveBytes - 1] = 7;                       // invalid direction
	Rack g;
	g.fromBytes(bad, Rack::kNumSaveBytes);
	CHECK(g.slots[0].type == Type::DIR);
	CHECK(g.slots[1].empty());
	CHECK(g.slots[2].empty());
	CHECK_EQUAL(0, g.dirMode);
}
