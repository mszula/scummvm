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

#include "common/tokenizer.h"
#include "engines/metaengine.h"

#include "gearhead/auction.h"
#include "gearhead/car.h"
#include "gearhead/catalog.h"
#include "gearhead/console.h"
#include "gearhead/gearhead.h"
#include "gearhead/input.h"
#include "gearhead/jobengine.h"
#include "gearhead/jobs.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
#include "gearhead/workshop.h"

namespace Gearhead {

void dumpLine(const char *format, ...) {
	va_list va;
	va_start(va, format);
	const Common::String s = Common::String::vformat(format, va);
	va_end(va);
	debug("%s", s.c_str());
	GUI::Debugger *console = g_engine->getDebugger();
	if (console && console->isActive())
		console->debugPrintf("%s\n", s.c_str());
}

Console::Console() : GUI::Debugger(), _fromScript(false) {
	registerCmd("cash", WRAP_METHOD(Console, cmdCash));
	registerCmd("skill", WRAP_METHOD(Console, cmdSkill));
	registerCmd("jobs", WRAP_METHOD(Console, cmdJobs));
	registerCmd("job", WRAP_METHOD(Console, cmdJob));
	registerCmd("completejob", WRAP_METHOD(Console, cmdCompleteJob));
	registerCmd("givecar", WRAP_METHOD(Console, cmdGiveCar));
	registerCmd("buypart", WRAP_METHOD(Console, cmdBuyPart));
	registerCmd("mechanics", WRAP_METHOD(Console, cmdMechanics));
	registerCmd("dump", WRAP_METHOD(Console, cmdDump));
}

bool Console::runLine(const Common::String &line) {
	Common::StringTokenizer tok(line, " ");
	Common::StringArray words;
	while (!tok.empty())
		words.push_back(tok.nextToken());
	if (words.empty())
		return false;
	Common::Array<const char *> argv;
	for (uint i = 0; i < words.size(); i++)
		argv.push_back(words[i].c_str());
	bool keepRunning = true;
	_fromScript = true;
	const bool known = handleCommand(argv.size(), argv.data(), keepRunning);
	_fromScript = false;
	return known;
}

void Console::print(const char *format, ...) {
	va_list va;
	va_start(va, format);
	Common::String s = Common::String::vformat(format, va);
	va_end(va);
	debugPrintf("%s", s.c_str());
	if (_fromScript) {
		s.trim();
		debug("console: %s", s.c_str());
	}
}

static Mechanic *mechanicOrComplain(Console *console) {
	if (!g_engine->_mechanic)
		console->print("No mechanic is signed in\n");
	return g_engine->_mechanic;
}

bool Console::cmdCash(int argc, const char **argv) {
	Mechanic *m = mechanicOrComplain(this);
	if (!m)
		return true;
	if (argc > 1) {
		m->_cash = (float)atof(argv[1]);
		m->drawCash();
	}
	print("Cash: $%.2f%s\n", m->_cash, m->_job ? " (the job's budget)" : "");
	return true;
}

bool Console::cmdSkill(int argc, const char **argv) {
	Mechanic *m = mechanicOrComplain(this);
	if (!m)
		return true;
	if (argc > 1)
		m->_skillIndex = CLIP(atoi(argv[1]), 0, 4);
	print("Skill: %d (%s)\n", m->_skillIndex, skillLevel(m->_skillIndex).name);
	return true;
}

bool Console::cmdJobs(int argc, const char **argv) {
	const Mechanic *m = g_engine->_mechanic;
	const Job *current = g_engine->_jobEngine ? g_engine->_jobEngine->current() : nullptr;
	for (uint i = 0; i < g_engine->_jobs._jobs.size(); i++) {
		const Job &j = g_engine->_jobs._jobs[i];
		const CarModel *model = g_engine->_cars.find(j.carId, false);
		print("%3d  car %2d %-24s skill %d  $%9.2f  %s\n", j.id, j.carId, model ? model->_name.c_str() : "?",
		            model ? model->_minSkill : -1, j.fee,
		            &j == current ? "running" : (m && m->isJobCompleted(j.id) ? "done" : ""));
	}
	return true;
}

bool Console::cmdJob(int argc, const char **argv) {
	if (argc < 2) {
		print("Usage: job ID (the request appears when the console is closed)\n");
		return true;
	}
	Mechanic *m = mechanicOrComplain(this);
	if (!m)
		return true;
	if (m->_id == kMechanicIdCarDev || m->_job) {
		print("Not for CarDev, and not while a job is running\n");
		return true;
	}
	const int id = atoi(argv[1]);
	if (!g_engine->_jobs.findById(id)) {
		print("No job %d\n", id);
		return true;
	}
	g_engine->requestJob(id);
	print("Job %d is offered in the workshop\n", id);
	return true;
}

bool Console::cmdCompleteJob(int argc, const char **argv) {
	if (!g_engine->_jobEngine || !g_engine->_jobEngine->current()) {
		print("No job is running\n");
		return true;
	}
	g_engine->_jobEngine->requestCompletion();
	print("The job ends as completed when the console is closed\n");
	return true;
}

bool Console::cmdGiveCar(int argc, const char **argv) {
	Mechanic *m = mechanicOrComplain(this);
	if (!m)
		return true;
	if (argc < 2 || g_engine->_input->root() != g_engine->_workShop) {
		print("Usage: givecar MODEL_ID, in the workshop (a random car of the model, into the lot)\n");
		return true;
	}
	if (m->addNewCar(atoi(argv[1])))
		g_engine->_workShop->activate();
	else
		print("No car %s, or the lot is full\n", argv[1]);
	return true;
}

bool Console::cmdBuyPart(int argc, const char **argv) {
	Mechanic *m = mechanicOrComplain(this);
	if (!m)
		return true;
	const PartDef *pd = argc > 1 && m->_car ? m->_car->model()->findPart(atoi(argv[1])) : nullptr;
	if (!pd || !m->_bank) {
		print("Usage: buypart PART_ID, with a car in the workshop (bought from the catalog, into the bin)\n");
		return true;
	}
	if (g_engine->_catalog->buyPart(pd))
		g_engine->_workShop->_bin.refill();
	return true;
}

bool Console::cmdMechanics(int argc, const char **argv) {
	const SaveStateList saves = g_engine->getMetaEngine()->listSaves(g_engine->targetName().c_str());
	for (uint i = 0; i < saves.size(); i++)
		dumpLine("slot %d: %s", saves[i].getSaveSlot(), saves[i].getDescription().c_str());
	return true;
}

bool Console::cmdDump(int argc, const char **argv) {
	const Common::String what = argc > 1 ? argv[1] : "";
	if (what == "yard") {
		// The parts on the shelves of the current JunkYard area
		Common::Point p;
		Common::String name;
		for (int i = 0; i < 64; i++) {
			const bool ok = g_engine->_junkYard->_yard.debugItemScreenPos(i, p, name);
			if (name.empty())
				break;
			dumpLine("dump item %d '%s' %s %d %d", i, name.c_str(), ok ? "at" : "hidden", p.x, p.y);
			name.clear();
		}
	} else if (what == "catalog") {
		g_engine->_catalog->debugDump();
	} else if (what == "auction") {
		g_engine->_auction->debugDump();
	} else if (what == "parts" || what == "bolts" || what == "bin" || what == "spray" || what == "view") {
		g_engine->_workShop->debugDump(what);
	} else {
		print("Usage: dump parts|bolts|bin|spray|view|yard|catalog|auction\n");
	}
	return true;
}

} // End of namespace Gearhead
