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

#ifndef GEARHEAD_AUTOSCRIPT_H
#define GEARHEAD_AUTOSCRIPT_H

#include "common/array.h"
#include "common/str.h"

namespace Gearhead {

/**
 * Development aid: drives the engine from a text file given in the
 * "gearhead_script" configuration key, so that runs can be compared with
 * the original game under Wine (tools/wine/ghg.sh). One command per line:
 *   wait MS | frames N | move X Y | click X Y | down | up | key NAME |
 *   type TEXT | shot HOSTPATH.png | log TEXT | quit
 */
class AutoScript {
public:
	bool load(const Common::String &hostPath);
	/** Called once per frame before the events are dispatched */
	void update();
	bool finished() const { return _pc >= _lines.size() && _queue.empty(); }

private:
	struct Action {
		enum Type { kMove, kDown, kUp, kKeyDown, kKeyUp, kFrames };
		Type type;
		int a, b;
	};

	void execute(const Common::String &line);
	void pushKey(int scanCode, int ascii);

	Common::Array<Common::String> _lines;
	uint _pc = 0;
	uint32 _waitUntil = 0;
	Common::Array<Action> _queue;
};

} // End of namespace Gearhead

#endif // GEARHEAD_AUTOSCRIPT_H
