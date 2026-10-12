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

#ifndef GEARHEAD_INPUT_H
#define GEARHEAD_INPUT_H

#include "common/array.h"
#include "common/keyboard.h"
#include "common/rect.h"

namespace Gearhead {

class Widget;
struct Event;

/** Scan codes (PC set 1, as delivered by the original keyboard hook) */
enum ScanCode {
	kScanEsc = 0x01,
	kScanBackspace = 0x0E,
	kScanTab = 0x0F,
	kScanP = 0x19,
	kScanEnter = 0x1C,
	kScanCtrl = 0x1D,
	kScanLShift = 0x2A,
	kScanRShift = 0x36,
	kScanAlt = 0x38,
	kScanSpace = 0x39,
	kScanF4 = 0x3E,
	kScanHome = 0x47,
	kScanUp = 0x48,
	kScanPageUp = 0x49,
	kScanLeft = 0x4B,
	kScanRight = 0x4D,
	kScanEnd = 0x4F,
	kScanDown = 0x50,
	kScanPageDown = 0x51,
	kScanInsert = 0x52,
	kScanDelete = 0x53
};

/** Text-entry hook (ALIBI KeyState callback): called for every key event as it arrives */
class KeyHook {
public:
	virtual ~KeyHook() {}
	virtual void onKeyHook(int scanCode, bool released, int ascii) = 0;
};

/**
 * The original input layer: keeps the mouse and keyboard state and, once
 * per frame, turns it into widget events in a fixed order, then ticks the
 * root pane (slot 14).
 */
class Input {
public:
	Input();

	/** Pump the ScummVM event queue. Returns false if the engine should quit. */
	bool poll();
	/** Generate this frame's events and tick the root */
	void dispatchFrame();

	void setRoot(Widget *w) { _root = w; }
	Widget *root() const { return _root; }
	void setCapture(Widget *w) { _capture = w; }
	void releaseCapture() { _capture = nullptr; }
	Widget *capture() const { return _capture; }
	void setKeyTarget(Widget *w) { _keyTarget = w; }
	Widget *keyTarget() const { return _keyTarget; }
	void setKeyHook(KeyHook *hook) { _hook = hook; }
	KeyHook *keyHook() const { return _hook; }

	const Common::Point &mouse() const { return _mouse; }
	/** bit 0 = left, bit 1 = right */
	uint buttons() const { return _buttons; }
	bool isKeyDown(int scanCode) const { return scanCode >= 0 && scanCode < 256 && _keys[scanCode]; }

	// Injection (automation scripts)
	void injectMouse(const Common::Point &p);
	void injectButton(int button, bool down);
	void injectKey(int scanCode, int ascii, bool down);

	static int toScanCode(const Common::KeyState &ks);

private:
	void handleKey(int scanCode, int ascii, bool down);
	void send(Widget *target, const Event &ev);

	struct ButtonChange {
		int button;
		bool down;
	};

	Widget *_root;
	Widget *_capture;
	Widget *_keyTarget;
	KeyHook *_hook;
	Common::Point _mouse, _prevMouse;
	uint _buttons, _prevButtons;
	Common::Array<ButtonChange> _pending;
	bool _keys[256];
	Common::Array<int> _heldOrder;
	Common::Array<int> _releasedThisFrame;
};

} // End of namespace Gearhead

#endif // GEARHEAD_INPUT_H
