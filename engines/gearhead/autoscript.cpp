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

#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/tokenizer.h"

#include "gearhead/autoscript.h"
#include "gearhead/car.h"
#include "gearhead/catalog.h"
#include "gearhead/console.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobengine.h"
#include "gearhead/jobs.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
#include "gearhead/paint.h"
#include "gearhead/ui.h"
#include "gearhead/workshop.h"

namespace Gearhead {

bool AutoScript::load(const Common::String &hostPath) {
	Common::FSNode node(Common::Path(hostPath, '/'));
	Common::SeekableReadStream *s = node.createReadStream();
	if (!s)
		return false;
	while (!s->eos() && !s->err()) {
		Common::String line = s->readLine();
		line.trim();
		if (!line.empty() && line[0] != '#')
			_lines.push_back(line);
	}
	delete s;
	debugC(1, kDebugScript, "AutoScript: %d commands from %s", _lines.size(), hostPath.c_str());
	return true;
}

static int namedKey(const Common::String &name, int &ascii) {
	static const struct {
		const char *name;
		int scan;
	} keys[] = {
		{ "esc", kScanEsc }, { "enter", kScanEnter }, { "backspace", kScanBackspace }, { "tab", kScanTab },
		{ "up", kScanUp }, { "down", kScanDown }, { "left", kScanLeft }, { "right", kScanRight },
		{ "pageup", kScanPageUp }, { "pagedown", kScanPageDown }, { "insert", kScanInsert }, { "delete", kScanDelete },
		{ "home", kScanHome }, { "end", kScanEnd }, { "space", kScanSpace }, { "f4", kScanF4 }, { "alt", kScanAlt }
	};
	ascii = 0;
	for (uint i = 0; i < ARRAYSIZE(keys); i++) {
		if (name.equalsIgnoreCase(keys[i].name)) {
			if (keys[i].scan == kScanEnter)
				ascii = 13;
			else if (keys[i].scan == kScanEsc)
				ascii = 27;
			else if (keys[i].scan == kScanBackspace)
				ascii = 8;
			else if (keys[i].scan == kScanSpace)
				ascii = ' ';
			return keys[i].scan;
		}
	}
	if (name.size() == 1) {
		ascii = name[0];
		Common::KeyState ks((Common::KeyCode)tolower(name[0]), name[0]);
		return Input::toScanCode(ks);
	}
	return 0;
}

void AutoScript::pushKey(int scanCode, int ascii) {
	Action a;
	a.type = Action::kKeyDown;
	a.a = scanCode;
	a.b = ascii;
	_queue.push_back(a);
	a.type = Action::kFrames;
	a.a = 2;
	_queue.push_back(a);
	a.type = Action::kKeyUp;
	a.a = scanCode;
	a.b = ascii;
	_queue.push_back(a);
	a.type = Action::kFrames;
	a.a = 2;
	_queue.push_back(a);
}

void AutoScript::execute(const Common::String &line) {
	Common::StringTokenizer tok(line, " ");
	const Common::String cmd = tok.nextToken();
	Action a;
	debugC(2, kDebugScript, "AutoScript: %s", line.c_str());

	if (cmd == "wait") {
		_waitUntil = g_system->getMillis() + atoi(tok.nextToken().c_str());
	} else if (cmd == "frames") {
		a.type = Action::kFrames;
		a.a = atoi(tok.nextToken().c_str());
		_queue.push_back(a);
	} else if (cmd == "move" || cmd == "click") {
		a.type = Action::kMove;
		a.a = atoi(tok.nextToken().c_str());
		a.b = atoi(tok.nextToken().c_str());
		_queue.push_back(a);
		if (cmd == "click") {
			Action f;
			f.type = Action::kFrames;
			f.a = 2;
			_queue.push_back(f);
			f.type = Action::kDown;
			_queue.push_back(f);
			f.type = Action::kFrames;
			f.a = 3;
			_queue.push_back(f);
			f.type = Action::kUp;
			_queue.push_back(f);
			f.type = Action::kFrames;
			f.a = 2;
			_queue.push_back(f);
		}
	} else if (cmd == "down" || cmd == "up" || cmd == "rdown" || cmd == "rup") {
		a.type = cmd == "down" ? Action::kDown : cmd == "up" ? Action::kUp : cmd == "rdown" ? Action::kRDown : Action::kRUp;
		_queue.push_back(a);
	} else if (cmd == "keydown" || cmd == "keyup") {
		int ascii;
		a.a = namedKey(tok.nextToken(), ascii);
		a.b = ascii;
		a.type = cmd == "keydown" ? Action::kKeyDown : Action::kKeyUp;
		_queue.push_back(a);
	} else if (cmd == "key") {
		int ascii;
		const int scan = namedKey(tok.nextToken(), ascii);
		pushKey(scan, ascii);
	} else if (cmd == "type") {
		const Common::String text = line.substr(5);
		for (uint i = 0; i < text.size(); i++) {
			Common::KeyState ks((Common::KeyCode)tolower(text[i]), text[i]);
			pushKey(Input::toScanCode(ks), (byte)text[i]);
		}
	} else if (cmd == "shot") {
		Common::String path = line.substr(4); // the rest of the line (paths may contain spaces)
		path.trim();
		if (!g_engine->_gfx->saveScreenshot(path))
			warning("AutoScript: cannot write %s", path.c_str());
	} else if (cmd == "clickpart" || cmd == "clickbolt") {
		// Test helpers: click the installed part with PD_ID n / bolt n wherever it is on screen
		const int n = atoi(tok.nextToken().c_str());
		Common::Point p;
		const bool ok = cmd == "clickpart" ? g_engine->_workShop->debugPartScreenPos(n, p)
		                                   : g_engine->_workShop->_airRatchet.debugBoltScreenPos(n, p);
		if (!ok)
			warning("AutoScript: %s %d not found on screen", cmd.c_str(), n);
		else
			execute(Common::String::format("click %d %d", p.x, p.y));
	} else if (cmd == "savemech" || cmd == "reloadmech") {
		// Test helpers: write the mechanic's .mek to a host file / save and load it back
		Mechanic *m = g_engine->_mechanic;
		if (!m) {
			warning("AutoScript: no mechanic");
		} else if (cmd == "savemech") {
			Common::String path = line.substr(8);
			path.trim();
			Common::DumpFile f;
			if (f.open(Common::Path(path, '/'), true))
				m->save(f);
			else
				warning("AutoScript: cannot write %s", path.c_str());
		} else {
			m->saveToDisk();
			m->reloadFromDisk();
			g_engine->_workShop->activate();
		}
	} else if (cmd == "clickitem") {
		// Test helper for the junkyard: click the n-th part on the shelves of the current area
		Common::Point p;
		Common::String name;
		const int n = atoi(tok.nextToken().c_str());
		if (g_engine->_junkYard->_yard.debugItemScreenPos(n, p, name))
			execute(Common::String::format("click %d %d", p.x, p.y));
		else
			warning("AutoScript: shelf item %d not found on screen", n);
	} else if (cmd == "movepaint") {
		// Test helper: the pointer to the centre of the n-th largest paint triangle on screen
		Common::String arg = line.substr(9);
		arg.trim();
		Common::Point at;
		if (g_engine->_workShop->_sprayCan->debugPaintPos(atoi(arg.c_str()), at))
			execute(Common::String::format("move %d %d", at.x, at.y));
		else
			warning("AutoScript: no paint triangle %s", arg.c_str());
	} else if (cmd == "putmech") {
		// Test helper: "putmech SLOT HOSTFILE" copies a .mek file into a mechanic slot
		Common::String rest = line.substr(7);
		rest.trim();
		const int slot = atoi(rest.c_str());
		const size_t space = rest.findFirstOf(' ');
		Common::String path = space == Common::String::npos ? Common::String() : rest.substr(space + 1);
		path.trim();
		Common::File in;
		Common::WriteStream *out = in.open(Common::FSNode(Common::Path(path, '/'))) ? g_engine->createMechanic(slot) : nullptr;
		if (!out) {
			warning("AutoScript: %s failed", line.c_str());
		} else {
			byte buf[4096];
			uint32 n;
			while ((n = in.read(buf, sizeof(buf))) > 0)
				out->write(buf, n);
			out->finalize();
			delete out;
		}
	} else if (cmd == "scummvmsave" || cmd == "scummvmload") {
		// Test helpers: what ScummVM's menu does for a slot
		const int slot = atoi(tok.nextToken().c_str());
		Common::U32String msg;
		const bool save = cmd == "scummvmsave";
		const bool can = save ? g_engine->canSaveGameStateCurrently(&msg) : g_engine->canLoadGameStateCurrently(&msg);
		Common::Error err = Common::kNoError;
		if (can)
			err = save ? g_engine->saveGameState(slot, "", false) : g_engine->loadGameState(slot);
		debug("AutoScript: %s %d: allowed %d, %s%s", cmd.c_str(), slot, can, can ? err.getDesc().c_str() : "",
		      can ? "" : msg.encode().c_str());
	} else if (cmd == "pushaction") {
		// Test helper: "pushaction up|down|left|right|back start|end", the event the keymapper sends
		static const char *const kNames[] = { "", "up", "down", "left", "right", "back" };
		const Common::String name = tok.nextToken();
		Common::Event e;
		e.type = tok.nextToken() == "end" ? Common::EVENT_CUSTOM_ENGINE_ACTION_END : Common::EVENT_CUSTOM_ENGINE_ACTION_START;
		e.customType = kActionNone;
		for (int i = 1; i <= kActionBack; i++)
			if (name == kNames[i])
				e.customType = i;
		g_system->getEventManager()->pushEvent(e);
	} else if (cmd == "pausetest") {
		// Test helper: "pausetest MS" pauses the engine like ScummVM's menu; the game time must not move
		const uint32 ms = atoi(tok.nextToken().c_str());
		g_engine->tickTimer();
		const uint32 before = g_engine->gameTimeMs();
		PauseToken pt = g_engine->pauseEngine();
		g_system->delayMillis(ms);
		pt.clear();
		g_engine->tickTimer();
		debug("AutoScript: pausetest %u ms: game time %u -> %u", ms, before, g_engine->gameTimeMs());
	} else if (cmd == "campaign") {
		_campaign = true;
		_campaignNext = 0;
	} else if (cmd == "log") {
		debug("AutoScript: %s", line.substr(4).c_str());
	} else if (cmd == "quit") {
		g_engine->requestQuit();
	} else if (!static_cast<Console *>(g_engine->getDebugger())->runLine(line)) {
		// The rest are the console's commands
		warning("AutoScript: unknown command '%s'", line.c_str());
	}
}

void AutoScript::update() {
	Input *input = g_engine->_input;
	// Queued low-level actions take priority; kFrames waits whole frames
	while (!_queue.empty()) {
		Action &a = _queue[0];
		const Action::Type type = a.type;
		switch (type) {
		case Action::kFrames:
			if (a.a-- > 0)
				return;
			break;
		case Action::kMove:
			input->injectMouse(Common::Point(a.a, a.b));
			break;
		case Action::kDown:
			input->injectButton(0, true);
			break;
		case Action::kUp:
			input->injectButton(0, false);
			break;
		case Action::kRDown:
			input->injectButton(1, true);
			break;
		case Action::kRUp:
			input->injectButton(1, false);
			break;
		case Action::kKeyDown:
			input->injectKey(a.a, a.b, true);
			break;
		case Action::kKeyUp:
			input->injectKey(a.a, a.b, false);
			break;
		}
		_queue.remove_at(0);
		if (type != Action::kFrames)
			return; // one action per frame
	}
	if (_campaign) {
		if (_inStep) {
			campaignDialogs();
			return;
		}
		_inStep = true;
		campaignStep();
		_inStep = false;
		return;
	}
	if (g_system->getMillis() < _waitUntil)
		return;
	while (_pc < _lines.size() && _queue.empty() && !_campaign && g_system->getMillis() >= _waitUntil)
		execute(_lines[_pc++]);
}

void AutoScript::pushClick(const Widget *w) {
	const Common::Rect &r = w->rect();
	Action a;
	a.type = Action::kMove;
	a.a = (r.left + r.right) / 2;
	a.b = (r.top + r.bottom) / 2;
	_queue.push_back(a);
	a.type = Action::kFrames;
	a.a = 2;
	_queue.push_back(a);
	a.type = Action::kDown;
	_queue.push_back(a);
	a.type = Action::kFrames;
	a.a = 3;
	_queue.push_back(a);
	a.type = Action::kUp;
	_queue.push_back(a);
}

// ---------------------------------------------------------------- campaign

static bool listHas(const Common::Array<int> &a, int v) {
	for (uint i = 0; i < a.size(); i++)
		if (a[i] == v)
			return true;
	return false;
}

/** The part to take off first so that @p p can come off: @p p itself or one of the parts in its way */
static Part *findRemovable(CarInstance *car, Part *p, int depth) {
	AssemblyError err;
	if (car->canRemove(p, err))
		return p;
	if (depth > 20)
		return nullptr;
	for (uint i = 0; i < err.ids.size(); i++) {
		Part *b = car->findPart(err.ids[i]);
		if (!b)
			b = car->findMutexPart(err.ids[i]);
		if (b && b != p) {
			Part *r = findRemovable(car, b, depth + 1);
			if (r)
				return r;
		}
	}
	return nullptr;
}

bool AutoScript::campaignDialogs() {
	Dialog *d = nullptr;
	if (g_engine->_msgBox && g_engine->_msgBox->isOpen())
		d = g_engine->_msgBox;
	else if (g_engine->_jobEngine && g_engine->_jobEngine->isOpen())
		d = g_engine->_jobEngine;
	if (!d)
		return false;
	const Common::String key = d->title() + "|" + d->text();
	if (key != _lastDialog) {
		_lastDialog = key;
		debug("campaign: dialog '%s' %s", d->title().c_str(), d->text().c_str());
		if (d->title() == "No More Jobs.") {
			_noMoreJobs = true;
			_noMoreSkill = g_engine->_mechanic ? g_engine->_mechanic->_skillIndex : -1;
		}
	}
	const uint32 now = g_system->getMillis();
	if (now < _campaignNext)
		return true;
	// OK is the first button (the left one of a question)
	if (!d->children().empty())
		pushClick(d->children()[0]);
	_campaignNext = now + 500;
	return true;
}

void AutoScript::campaignStep() {
	const uint32 now = g_system->getMillis();
	if (now < _campaignNext)
		return;
	if (campaignDialogs())
		return;
	_lastDialog.clear();
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	if (!m || !ws || g_engine->_input->root() != ws)
		return;
	if (m->_job) {
		_idleSince = 0;
		Job &job = *m->_job;
		if (job.id != _campaignJob) {
			_campaignJob = job.id;
			_campaignOps = 0;
			_completeSince = 0;
			debug("campaign: job %d, car %d '%s', fee $%.2f, skill %d, %d jobs done", job.id, job.carId,
			      m->_car ? m->_car->model()->_name.c_str() : "?", job.fee, m->_skillIndex, m->countCompletedJobs());
		}
		if (job.isComplete()) {
			if (!_completeSince) {
				_completeSince = now;
				debug("campaign: job %d complete after %d operations, $%.2f of the budget left", job.id, _campaignOps, m->_cash);
			} else if (now - _completeSince > 15000) {
				campaignEnd("the job engine does not end the complete job");
			}
			return;
		}
		if (!solveStep(job)) {
			campaignEnd("stuck");
			return;
		}
		_campaignOps++;
		m->updateCurrentCar(ws->_region);
		ws->_bin.refill();
		ws->_carView.setDirty(true);
		_campaignNext = now + 100;
		return;
	}
	int done = 0;
	for (uint i = 0; i < g_engine->_jobs._jobs.size(); i++)
		if (m->isJobCompleted(g_engine->_jobs._jobs[i].id))
			done++;
	if (done == (int)g_engine->_jobs._jobs.size()) {
		campaignEnd("all jobs done");
		return;
	}
	if (!_idleSince) {
		_idleSince = now;
		debug("campaign: cash $%.2f, skill %d", m->_cash, m->_skillIndex);
	}
	// Until the ninth job the job engine offers the next one by itself
	if (m->_autoJobRequest)
		return;
	// Give the skill check (every 2 s, on the cash between jobs) the time to promote
	if (now - _idleSince < 3000)
		return;
	if (_noMoreJobs) {
		if (m->_skillIndex == _noMoreSkill) {
			campaignEnd("no more jobs for this skill");
			return;
		}
		_noMoreJobs = false;
	}
	pushClick(&ws->_actions._getJob);
	_campaignNext = now + 1500;
}

bool AutoScript::buyPart(Job &job, const PartDef *pd) {
	Mechanic *m = g_engine->_mechanic;
	if (pd->region < 1 || pd->region > 3) {
		debug("campaign:   %d '%s' is not in the catalog (region %d)", pd->id, pd->name.c_str(), pd->region);
		return false;
	}
	if (!job.isPartAllowed(pd)) {
		debug("campaign:   %d '%s' is a custom part, not allowed in this job", pd->id, pd->name.c_str());
		return false;
	}
	if (!m->canAfford((int)pd->costMax)) {
		debug("campaign:   cannot afford %d '%s' ($%d, cash $%.2f)", pd->id, pd->name.c_str(), (int)pd->costMax, m->_cash);
		return false;
	}
	if (!g_engine->_catalog->buyPart(pd))
		return false;
	debug("campaign:   buy %d '%s' $%d", pd->id, pd->name.c_str(), (int)pd->costMax);
	return true;
}

bool AutoScript::solveStep(Job &job) {
	Mechanic *m = g_engine->_mechanic;
	CarInstance *car = m->_car;
	PartsBank *bank = m->_bank;
	if (!car || !bank)
		return false;
	const CarModel *model = car->model();
	// The parts the job wants installed, with their attach dependencies
	Common::Array<int> need;
	for (uint i = 0; i < job.completeStats.size(); i++)
		if (job.completeStats[i].type == 0 && job.completeStats[i].condition >= 0)
			need.push_back(job.completeStats[i].subject);
	for (uint i = 0; i < need.size(); i++) {
		const PartDef *pd = model->findPart(need[i]);
		if (!pd)
			continue;
		for (uint k = 0; k < pd->attachDep.size() && k < 16; k++) {
			const int dep = pd->attachDep[k];
			if (dep < 0)
				break;
			if (!listHas(need, dep) && !(pd->amea && car->findMutexPart(dep)))
				need.push_back(dep);
		}
	}
	auto legal = [&job](const PartDef *pd) {
		return pd->region >= 1 && pd->region <= 3 && job.legalModes[pd->region];
	};
	// A part whose place is taken by a needed (or installed) part of its mutex group
	auto displaced = [&](const PartDef *pd) {
		if (!pd->mutexSet || listHas(need, pd->id))
			return false;
		for (uint i = 0; i < need.size(); i++) {
			const PartDef *n = model->findPart(need[i]);
			if (n && n != pd && (n->mutexSet & pd->mutexSet))
				return true;
		}
		const Part *inst = car->findMutexPart(pd->id);
		return inst && inst->def != pd;
	};
	// A part from the bin that should go (back) onto the car
	auto wanted = [&](const Part *p) {
		if (car->isInstalled(p->def->id))
			return false;
		if (listHas(need, p->def->id))
			return true;
		if (displaced(p->def))
			return false;
		return !p->def->custom || p->condition == 3;
	};

	// 1. Take off the damaged parts and the parts a needed part replaces, the parts in their way first
	Common::Array<Part *> goals;
	for (uint i = 0; i < car->_parts.size(); i++)
		if (car->_parts[i]->condition < 3)
			goals.push_back(car->_parts[i]);
	for (uint i = 0; i < need.size(); i++) {
		if (car->isInstalled(need[i]))
			continue;
		Part *c = car->findMutexPart(need[i]);
		if (c && !listHas(need, c->def->id))
			goals.push_back(c);
	}
	for (uint i = 0; i < goals.size(); i++) {
		Part *r = findRemovable(car, goals[i], 0);
		if (!r) {
			debug("campaign:   cannot take off %d '%s'", goals[i]->def->id, goals[i]->def->name.c_str());
			return false;
		}
		if (!legal(r->def)) {
			debug("campaign:   %d '%s' (region %d) is in a region the job locks", r->def->id, r->def->name.c_str(), r->def->region);
			return false;
		}
		if (!m->removePart(r))
			return false;
		debug("campaign:   remove %d '%s' (condition %d)%s", r->def->id, r->def->name.c_str(), r->condition,
		      r == goals[i] ? "" : " in the way");
		return true;
	}
	// 2. Repair what goes back; a scrapped part is bought new
	for (uint i = 0; i < bank->bin.size(); i++) {
		Part *p = bank->bin[i];
		if (p->condition == 3 || !wanted(p))
			continue;
		if (p->condition > 0) {
			const int cost = (int)p->repairCost();
			if (!m->canAfford(cost)) {
				debug("campaign:   cannot afford to repair %d '%s' ($%d, cash $%.2f)", p->def->id, p->def->name.c_str(), cost, m->_cash);
				return false;
			}
			m->pay(cost, false);
			m->repairPart(p);
			debug("campaign:   repair %d '%s' $%d", p->def->id, p->def->name.c_str(), cost);
			return true;
		}
		bool spare = false;
		for (uint k = 0; k < bank->bin.size(); k++)
			if (k != i && bank->bin[k]->def == p->def && bank->bin[k]->condition > 0)
				spare = true;
		if (!spare)
			return buyPart(job, p->def);
	}
	// 3. Buy the needed parts that are neither on the car nor in the bin
	for (uint i = 0; i < need.size(); i++) {
		if (car->isInstalled(need[i]))
			continue;
		bool have = false;
		for (uint k = 0; k < bank->bin.size(); k++)
			if (bank->bin[k]->def->id == need[i] && bank->bin[k]->condition == 3)
				have = true;
		if (have)
			continue;
		const PartDef *pd = model->findPart(need[i]);
		if (!pd) {
			debug("campaign:   the car has no part %d", need[i]);
			return false;
		}
		return buyPart(job, pd);
	}
	// 4. Put the parts on
	for (uint i = 0; i < bank->bin.size(); i++) {
		Part *p = bank->bin[i];
		if (p->condition != 3 || !wanted(p) || !legal(p->def))
			continue;
		AssemblyError err;
		if (car->canInstall(p, err) && m->attachPart(p)) {
			debug("campaign:   attach %d '%s'", p->def->id, p->def->name.c_str());
			return true;
		}
	}
	// 5. Complete the car
	for (uint i = 0; i < model->_parts.size(); i++) {
		const PartDef *pd = model->_parts[i];
		if (pd->custom || car->isInstalled(pd->id) || !car->canAttachId(pd->id, nullptr))
			continue;
		for (uint k = 0; k < bank->bin.size(); k++) {
			if (bank->bin[k]->def == pd && bank->bin[k]->condition == 3) {
				debug("campaign:   %d '%s' (region %d) cannot be attached", pd->id, pd->name.c_str(), pd->region);
				return false;
			}
		}
		return buyPart(job, pd);
	}
	debug("campaign:   nothing left to do, but the job is not complete");
	for (uint i = 0; i < job.completeStats.size(); i++) {
		const JobStat &st = job.completeStats[i];
		const Part *p = st.type == 0 ? car->findPart(st.subject) : nullptr;
		debug("campaign:   stat type %d subject %d condition %d: %s", st.type, st.subject, st.condition,
		      st.type == 0 ? (p ? Common::String::format("installed, condition %d", p->condition).c_str() : "missing")
		                   : Common::String::format("complete %d, min condition %d", car->isComplete(st.subject), car->minCondition(st.subject)).c_str());
	}
	return false;
}

void AutoScript::campaignEnd(const char *why) {
	_campaign = false;
	Mechanic *m = g_engine->_mechanic;
	debug("campaign: end (%s): %d jobs done, cash $%.2f, skill %d, %d cars", why, m ? m->countCompletedJobs() : 0,
	      m ? m->_cash : 0.0f, m ? m->_skillIndex : 0, m ? m->numCars() : 0);
	if (!m)
		return;
	for (uint i = 0; i < g_engine->_jobs._jobs.size(); i++) {
		const Job &j = g_engine->_jobs._jobs[i];
		if (m->isJobCompleted(j.id))
			continue;
		const CarModel *cm = g_engine->_cars.find(j.carId, false);
		debug("campaign: left: job %d, car %d (skill %d), fee $%.2f", j.id, j.carId, cm ? cm->_minSkill : -1, j.fee);
	}
}

} // End of namespace Gearhead
