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

struct Job;
struct Part;
struct PartDef;
class Widget;

/**
 * Development aid: drives the engine from a text file given in the
 * "gearhead_script" configuration key, so that runs can be compared with
 * the original game under Wine (tools/wine/ghg.sh). One command per line:
 *   wait MS | frames N | move X Y | click X Y | down | up | key NAME |
 *   type TEXT | shot HOSTPATH.png | log TEXT | quit
 * "campaign" plays every job of a new mechanic: it answers the dialogs with
 * their first button, takes the offered jobs and does the work on the car
 * with the same operations as the workshop (remove, repair, buy, attach).
 */
class AutoScript {
public:
	bool load(const Common::String &hostPath);
	/** Called once per frame before the events are dispatched */
	void update();
	bool finished() const { return _pc >= _lines.size() && _queue.empty(); }

private:
	struct Action {
		enum Type { kMove, kDown, kUp, kRDown, kRUp, kKeyDown, kKeyUp, kFrames };
		Type type;
		int a, b;
	};

	void execute(const Common::String &line);
	void pushKey(int scanCode, int ascii);
	void pushClick(const Widget *w);

	void campaignStep();
	/** Clicks the first button of an open message box or job dialog */
	bool campaignDialogs();
	/** One operation on the car of the current job; false when the job cannot be finished */
	bool solveStep(Job &job);
	bool buyPart(Job &job, const PartDef *pd);
	void campaignEnd(const char *why);

	Common::Array<Common::String> _lines;
	uint _pc = 0;
	uint32 _waitUntil = 0;
	Common::Array<Action> _queue;

	bool _campaign = false;
	bool _inStep = false;       ///< a campaign step runs (a modal loop inside it only gets the dialogs answered)
	bool _noMoreJobs = false;
	int _noMoreSkill = -1;
	uint32 _idleSince = 0;      ///< no job since then (the skill check runs every 2 s)
	uint32 _campaignNext = 0;
	uint32 _completeSince = 0;
	int _campaignJob = -1;
	int _campaignOps = 0;
	Common::String _lastDialog;
};

} // End of namespace Gearhead

#endif // GEARHEAD_AUTOSCRIPT_H
