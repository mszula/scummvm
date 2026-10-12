/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GEARHEAD_RANDOM_H
#define GEARHEAD_RANDOM_H

#include "common/scummsys.h"

namespace Gearhead {

/**
 * The game uses the MSVC C runtime rand() (seeded with GetTickCount at
 * start-up). Reproducing the generator exactly keeps the economy and the
 * auction AI comparable with the original running under Wine.
 */
class MsvcRandom {
public:
	MsvcRandom(uint32 seed = 1) : _seed(seed) {}

	void setSeed(uint32 seed) { _seed = seed; }

	/** rand(): 0..32767 */
	int next() {
		_seed = _seed * 214013 + 2531011;
		return (_seed >> 16) & 0x7FFF;
	}

	/** Rand_float (0x450730): rand() / 32767.0 * max, inclusive of max */
	float frand(float max) {
		return (float)((double)next() / 32767.0 * (double)max);
	}

private:
	uint32 _seed;
};

} // End of namespace Gearhead

#endif // GEARHEAD_RANDOM_H
