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

#include "common/events.h"
#include "common/system.h"

#include "gearhead/input.h"
#include "gearhead/ui.h"

namespace Gearhead {

// The original clamps the pointer to x 0..624, y 0..464 (Pointer_update 0x45cd7b)
static Common::Point clampPointer(const Common::Point &p) {
	return Common::Point(CLIP<int16>(p.x, 0, 624), CLIP<int16>(p.y, 0, 464));
}

Input::Input() : _root(nullptr), _capture(nullptr), _keyTarget(nullptr), _hook(nullptr),
	_buttons(0), _prevButtons(0) {
	for (int i = 0; i < 256; i++)
		_keys[i] = false;
	for (int i = 0; i < 8; i++)
		_prevKeys[i] = 0;
}

int Input::toScanCode(const Common::KeyState &ks) {
	static const struct {
		Common::KeyCode key;
		byte scan;
	} table[] = {
		{ Common::KEYCODE_ESCAPE, 0x01 }, { Common::KEYCODE_1, 0x02 }, { Common::KEYCODE_2, 0x03 },
		{ Common::KEYCODE_3, 0x04 }, { Common::KEYCODE_4, 0x05 }, { Common::KEYCODE_5, 0x06 },
		{ Common::KEYCODE_6, 0x07 }, { Common::KEYCODE_7, 0x08 }, { Common::KEYCODE_8, 0x09 },
		{ Common::KEYCODE_9, 0x0A }, { Common::KEYCODE_0, 0x0B }, { Common::KEYCODE_MINUS, 0x0C },
		{ Common::KEYCODE_EQUALS, 0x0D }, { Common::KEYCODE_BACKSPACE, 0x0E }, { Common::KEYCODE_TAB, 0x0F },
		{ Common::KEYCODE_q, 0x10 }, { Common::KEYCODE_w, 0x11 }, { Common::KEYCODE_e, 0x12 },
		{ Common::KEYCODE_r, 0x13 }, { Common::KEYCODE_t, 0x14 }, { Common::KEYCODE_y, 0x15 },
		{ Common::KEYCODE_u, 0x16 }, { Common::KEYCODE_i, 0x17 }, { Common::KEYCODE_o, 0x18 },
		{ Common::KEYCODE_p, 0x19 }, { Common::KEYCODE_LEFTBRACKET, 0x1A }, { Common::KEYCODE_RIGHTBRACKET, 0x1B },
		{ Common::KEYCODE_RETURN, 0x1C }, { Common::KEYCODE_KP_ENTER, 0x1C }, { Common::KEYCODE_LCTRL, 0x1D },
		{ Common::KEYCODE_RCTRL, 0x1D }, { Common::KEYCODE_a, 0x1E }, { Common::KEYCODE_s, 0x1F },
		{ Common::KEYCODE_d, 0x20 }, { Common::KEYCODE_f, 0x21 }, { Common::KEYCODE_g, 0x22 },
		{ Common::KEYCODE_h, 0x23 }, { Common::KEYCODE_j, 0x24 }, { Common::KEYCODE_k, 0x25 },
		{ Common::KEYCODE_l, 0x26 }, { Common::KEYCODE_SEMICOLON, 0x27 }, { Common::KEYCODE_QUOTE, 0x28 },
		{ Common::KEYCODE_BACKQUOTE, 0x29 }, { Common::KEYCODE_LSHIFT, 0x2A }, { Common::KEYCODE_BACKSLASH, 0x2B },
		{ Common::KEYCODE_z, 0x2C }, { Common::KEYCODE_x, 0x2D }, { Common::KEYCODE_c, 0x2E },
		{ Common::KEYCODE_v, 0x2F }, { Common::KEYCODE_b, 0x30 }, { Common::KEYCODE_n, 0x31 },
		{ Common::KEYCODE_m, 0x32 }, { Common::KEYCODE_COMMA, 0x33 }, { Common::KEYCODE_PERIOD, 0x34 },
		{ Common::KEYCODE_SLASH, 0x35 }, { Common::KEYCODE_RSHIFT, 0x36 }, { Common::KEYCODE_KP_MULTIPLY, 0x37 },
		{ Common::KEYCODE_LALT, 0x38 }, { Common::KEYCODE_RALT, 0x38 }, { Common::KEYCODE_SPACE, 0x39 },
		{ Common::KEYCODE_F1, 0x3B }, { Common::KEYCODE_F2, 0x3C }, { Common::KEYCODE_F3, 0x3D },
		{ Common::KEYCODE_F4, 0x3E }, { Common::KEYCODE_F5, 0x3F }, { Common::KEYCODE_F6, 0x40 },
		{ Common::KEYCODE_F7, 0x41 }, { Common::KEYCODE_F8, 0x42 }, { Common::KEYCODE_F9, 0x43 },
		{ Common::KEYCODE_F10, 0x44 }, { Common::KEYCODE_HOME, 0x47 }, { Common::KEYCODE_UP, 0x48 },
		{ Common::KEYCODE_PAGEUP, 0x49 }, { Common::KEYCODE_LEFT, 0x4B }, { Common::KEYCODE_RIGHT, 0x4D },
		{ Common::KEYCODE_END, 0x4F }, { Common::KEYCODE_DOWN, 0x50 }, { Common::KEYCODE_PAGEDOWN, 0x51 },
		{ Common::KEYCODE_INSERT, 0x52 }, { Common::KEYCODE_DELETE, 0x53 }
	};
	for (uint i = 0; i < ARRAYSIZE(table); i++)
		if (table[i].key == ks.keycode)
			return table[i].scan;
	return 0;
}

void Input::handleKey(int scanCode, int ascii, bool down) {
	if (_hook)
		_hook->onKeyHook(scanCode, !down, ascii);
	if (scanCode <= 0 || scanCode >= 256)
		return;
	_keys[scanCode] = down;
}

void Input::injectMouse(const Common::Point &p) {
	_mouse = clampPointer(p);
	g_system->warpMouse(_mouse.x, _mouse.y);
}

void Input::injectButton(int button, bool down) {
	ButtonChange c;
	c.button = button;
	c.down = down;
	_pending.push_back(c);
}

void Input::injectKey(int scanCode, int ascii, bool down) {
	handleKey(scanCode, ascii, down);
}

bool Input::poll() {
	Common::Event e;
	bool keepRunning = true;
	while (g_system->getEventManager()->pollEvent(e)) {
		switch (e.type) {
		case Common::EVENT_MOUSEMOVE:
			_mouse = clampPointer(e.mouse);
			break;
		case Common::EVENT_LBUTTONDOWN:
		case Common::EVENT_LBUTTONUP:
		case Common::EVENT_RBUTTONDOWN:
		case Common::EVENT_RBUTTONUP:
			_mouse = clampPointer(e.mouse);
			injectButton((e.type == Common::EVENT_LBUTTONDOWN || e.type == Common::EVENT_LBUTTONUP) ? 0 : 1,
			             e.type == Common::EVENT_LBUTTONDOWN || e.type == Common::EVENT_RBUTTONDOWN);
			break;
		case Common::EVENT_KEYDOWN:
			if (!e.kbdRepeat)
				handleKey(toScanCode(e.kbd), e.kbd.ascii, true);
			break;
		case Common::EVENT_KEYUP:
			handleKey(toScanCode(e.kbd), e.kbd.ascii, false);
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END: {
			// The keymapper's actions: the keys the original reads
			static const int kActionScan[] = { 0, kScanUp, kScanDown, kScanLeft, kScanRight, kScanEsc };
			if (e.customType > kActionNone && e.customType <= kActionBack) {
				const int scan = kActionScan[e.customType];
				handleKey(scan, scan == kScanEsc ? 27 : 0, e.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START);
			}
			break;
		}
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			keepRunning = false;
			break;
		default:
			break;
		}
	}

	// The original samples the button state once per frame; apply at most one
	// change per button so that a quick click still yields down + up frames.
	for (int b = 0; b < 2; b++) {
		for (uint i = 0; i < _pending.size(); i++) {
			if (_pending[i].button != b)
				continue;
			if (_pending[i].down)
				_buttons |= (1 << b);
			else
				_buttons &= ~(1 << b);
			_pending.remove_at(i);
			break;
		}
	}
	return keepRunning;
}

void Input::send(Widget *target, const Event &ev) {
	if (target)
		target->handleEvent(ev);
}

void Input::dispatchFrame() {
	const bool lNow = _buttons & 1, lPrev = _prevButtons & 1;
	const bool rNow = _buttons & 2, rPrev = _prevButtons & 2;

#define MOUSE_TARGET mouseTarget()
	if (rNow)
		send(MOUSE_TARGET, Event(kEvRHeld, _mouse));
	if (rNow && !rPrev)
		send(MOUSE_TARGET, Event(kEvRDown, _mouse));
	send(MOUSE_TARGET, Event(kEvHover, _mouse));
	if (_mouse != _prevMouse)
		send(MOUSE_TARGET, Event(kEvMouseMove, _mouse));
	if (!lNow && lPrev)
		send(MOUSE_TARGET, Event(kEvLUp, _mouse));
	if (lNow)
		send(MOUSE_TARGET, Event(kEvLHeld, _mouse));
	if (lNow && !lPrev)
		send(MOUSE_TARGET, Event(kEvLDown, _mouse));
#undef MOUSE_TARGET

	// EventMgr_pump: the first 8 keys down in scan-code order get a key-held
	// event; keys of last frame's list that are gone get a key-up
	if (_keyTarget) {
		int down[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		int n = 0;
		for (int k = 0; k < 0x80 && n < 8; k++)
			if (_keys[k])
				down[n++] = k;
		for (int i = 0; i < n; i++)
			send(_keyTarget, Event(kEvKeyDown, _mouse, down[i]));
		for (int i = 0; i < 8 && _prevKeys[i] != 0; i++) {
			bool gone = true;
			for (int j = 0; j < 8; j++)
				if (_prevKeys[i] == down[j])
					gone = false;
			if (gone)
				send(_keyTarget, Event(kEvKeyUp, _mouse, _prevKeys[i]));
		}
		for (int i = 0; i < 8; i++)
			_prevKeys[i] = down[i];
	}

	_prevMouse = _mouse;
	_prevButtons = _buttons;

	if (_root)
		_root->onRefresh();
}

} // End of namespace Gearhead
