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
#include "common/file.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/tokenizer.h"

#include "gearhead/autoscript.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
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
	} else if (cmd == "givecar") {
		// Test helper: a random car of model N (Mechanic_addNewCar, the dev-mode path)
		const int id = atoi(tok.nextToken().c_str());
		if (g_engine->_mechanic && g_engine->_mechanic->addNewCar(id))
			g_engine->_workShop->activate();
		else
			warning("AutoScript: givecar %d failed", id);
	} else if (cmd == "clickitem" || cmd == "dumpyard") {
		// Test helpers for the junkyard: list / click the parts on the shelves of the current area
		Common::Point p;
		Common::String name;
		if (cmd == "dumpyard") {
			for (int i = 0; i < 64; i++) {
				const bool ok = g_engine->_junkYard->_yard.debugItemScreenPos(i, p, name);
				if (name.empty())
					break;
				debug("dump item %d '%s' %s %d %d", i, name.c_str(), ok ? "at" : "hidden", p.x, p.y);
				name.clear();
			}
		} else {
			const int n = atoi(tok.nextToken().c_str());
			if (g_engine->_junkYard->_yard.debugItemScreenPos(n, p, name))
				execute(Common::String::format("click %d %d", p.x, p.y));
			else
				warning("AutoScript: shelf item %d not found on screen", n);
		}
	} else if (cmd == "dump") {
		// Test helper: screen positions of the car's parts / the bolts / the bin cells
		const Common::String what = tok.nextToken();
		g_engine->_workShop->debugDump(what);
	} else if (cmd == "log") {
		debug("AutoScript: %s", line.substr(4).c_str());
	} else if (cmd == "quit") {
		g_engine->requestQuit();
	} else {
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
	if (g_system->getMillis() < _waitUntil)
		return;
	while (_pc < _lines.size() && _queue.empty() && g_system->getMillis() >= _waitUntil)
		execute(_lines[_pc++]);
}

} // End of namespace Gearhead
