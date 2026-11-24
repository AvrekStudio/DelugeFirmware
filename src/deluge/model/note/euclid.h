/*
 * Copyright © 2026 AvrekStudio
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

#include <algorithm>
#include <cstdint>

/// EUC, Elektron style, for one note row: two Euclidean generators (PL1 / RO1, PL2 / RO2) combined by OP, the result
/// rotated by TRO, over LEN steps of 1/16
namespace deluge::euclid {

constexpr int32_t kMinSteps = 1;
constexpr int32_t kMaxSteps = 128;

enum class Op : uint8_t { OR, AND, XOR };
constexpr int32_t kNumOps = 3;
constexpr const char* opName(Op op) {
	return op == Op::AND ? "AND" : op == Op::XOR ? "XOR" : "OR";
}

struct Settings {
	bool enabled = false;
	uint8_t pulses1 = 4;
	uint8_t pulses2 = 0;
	int8_t rotate1 = 0;
	int8_t rotate2 = 0;
	int8_t trackRotate = 0;
	Op op = Op::OR;

	[[nodiscard]] bool isDefault() const {
		return !enabled && pulses1 == 4 && pulses2 == 0 && rotate1 == 0 && rotate2 == 0 && trackRotate == 0
		       && op == Op::OR;
	}
};

constexpr int32_t wrap(int32_t i, int32_t n) {
	i %= n;
	return i < 0 ? i + n : i;
}

/// One generator: `pulses` hits spread as evenly as possible over `steps`, the first on step 0 (E(3,8) = x..x..x.)
constexpr bool generatorFires(int32_t step, int32_t steps, int32_t pulses) {
	if (steps <= 0 || pulses <= 0) {
		return false;
	}
	pulses = std::min(pulses, steps);
	return (wrap(step, steps) * pulses) % steps < pulses;
}

/// The rotations go from -steps/2 to +steps/2 (a positive rotation moves the pattern to the right)
constexpr int32_t maxRotation(int32_t steps) {
	return steps / 2;
}
constexpr int32_t minRotation(int32_t steps) {
	return -((steps - 1) / 2);
}
/// A rotation turned past one end comes back from the other
constexpr int32_t wrapRotation(int32_t value, int32_t steps) {
	const int32_t lo = minRotation(steps);
	const int32_t span = maxRotation(steps) - lo + 1;
	return lo + wrap(value - lo, span);
}

/// Whether step `step` of the row fires
constexpr bool fires(const Settings& s, int32_t step, int32_t steps) {
	if (steps <= 0) {
		return false;
	}
	const int32_t i = wrap(step - s.trackRotate, steps);
	const bool a = generatorFires(i - s.rotate1, steps, s.pulses1);
	const bool b = generatorFires(i - s.rotate2, steps, s.pulses2);
	switch (s.op) {
	case Op::AND:
		return a && b;
	case Op::XOR:
		return a != b;
	default:
		return a || b;
	}
}

/// The settings kept inside a row of `steps` steps (pulses at most LEN, rotations within their range)
constexpr void fitTo(Settings& s, int32_t steps) {
	steps = std::clamp(steps, kMinSteps, kMaxSteps);
	s.pulses1 = static_cast<uint8_t>(std::min<int32_t>(s.pulses1, steps));
	s.pulses2 = static_cast<uint8_t>(std::min<int32_t>(s.pulses2, steps));
	s.rotate1 = static_cast<int8_t>(std::clamp<int32_t>(s.rotate1, minRotation(steps), maxRotation(steps)));
	s.rotate2 = static_cast<int8_t>(std::clamp<int32_t>(s.rotate2, minRotation(steps), maxRotation(steps)));
	s.trackRotate = static_cast<int8_t>(std::clamp<int32_t>(s.trackRotate, minRotation(steps), maxRotation(steps)));
}

/// The number of whole steps of `stepTicks` closest to `lengthTicks` (at least 1, at most kMaxSteps)
constexpr int32_t stepsForLength(int32_t lengthTicks, int32_t stepTicks) {
	if (stepTicks <= 0) {
		return kMinSteps;
	}
	return std::clamp((lengthTicks + stepTicks / 2) / stepTicks, kMinSteps, kMaxSteps);
}

} // namespace deluge::euclid
