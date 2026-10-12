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

#ifndef GEARHEAD_UI_H
#define GEARHEAD_UI_H

#include "common/array.h"
#include "common/func.h"
#include "common/rect.h"
#include "common/str.h"

namespace Graphics {
class ManagedSurface;
}

namespace Gearhead {

class BlitFont;
class Sound;

/** Event types of the original input layer (Input_update 0x45399a) */
enum EventType {
	kEvLHeld = 0x10001,
	kEvLDown = 0x10002,
	kEvLUp = 0x10003,
	kEvRHeld = 0x10004,
	kEvRDown = 0x10005,
	kEvMouseMove = 0x10007,
	kEvHover = 0x10009,
	kEvKeyDown = 0x20001,
	kEvKeyUp = 0x20002,
	kEvRefresh = 0x30001
};

struct Event {
	uint32 type;
	Common::Point pos;
	int key; ///< DirectInput-style (set 1) scan code for key events

	Event(uint32 t = 0, const Common::Point &p = Common::Point(), int k = 0) : type(t), pos(p), key(k) {}
	uint category() const { return type >> 16; }
};

/**
 * Base of every UI object (Widget, vt 0x4d007c). The 15 virtual slots of the
 * original map onto the handlers below; onRefresh() is both "redraw" for
 * ordinary widgets and the per-frame tick of the root pane.
 * Rects keep the original {left, top, right, bottom} values; hit tests are
 * inclusive on all edges.
 */
class Widget {
public:
	Widget();
	virtual ~Widget() {}

	virtual void handleEvent(const Event &ev);
	virtual void setDirty(bool dirty) { _dirty = dirty; }
	bool isDirty() const { return _dirty; }

	virtual void onLHeld(const Common::Point &p) {}
	virtual void onLDown(const Common::Point &p) {}
	virtual void onLUp(const Common::Point &p) {}
	virtual void onRHeld(const Common::Point &p) {}
	virtual void onRDown(const Common::Point &p) {}
	virtual void onMouseMove(const Common::Point &p) {}
	virtual void onHover(const Common::Point &p) {}
	virtual void onKeyDown(int key) {}
	virtual void onKeyUp(int key) {}
	virtual void onRefresh() {}

	/** Post the 0x30001 "refresh yourself" event */
	void refresh() { handleEvent(Event(kEvRefresh)); }

	bool contains(const Common::Point &p) const {
		return p.x >= _rect.left && p.x <= _rect.right && p.y >= _rect.top && p.y <= _rect.bottom;
	}
	void setRect(int l, int t, int r, int b) { _rect = Common::Rect(l, t, r, b); }
	const Common::Rect &rect() const { return _rect; }

	enum Kind {
		kKindWidget = 0,
		kKindButton = 1
	};
	int _kind;

protected:
	bool _dirty;
	Common::Rect _rect;
};

/** Container (Pane, vt 0x4d0200). Events reach every child whose rect contains the point; no z-order, no "handled" stop. */
class Pane : public Widget {
public:
	Pane();
	~Pane() override {}

	void handleEvent(const Event &ev) override;
	void addChild(Widget *w) { _children.push_back(w); }
	void removeChild(Widget *w);
	void clearChildren() { _children.clear(); }
	const Common::Array<Widget *> &children() const { return _children; }

	/** Pane_radioRelease (0x4081f4) */
	void radioRelease(Widget *selected);

protected:
	Common::Array<Widget *> _children; ///< not owned
	bool _propagate;
};

class Button;
typedef Common::Functor1<Button *, void> ButtonCallback;

template<class T>
ButtonCallback *makeCallback(T *obj, void (T::*fn)(Button *)) {
	return new Common::Functor1Mem<Button *, void, T>(obj, fn);
}

/** Push / radio button (Button, vt 0x4d001c) */
class Button : public Widget {
public:
	enum State {
		kStateUp = 0,
		kStateDown = 1,
		kStateDisabled = 2
	};

	Button(ButtonCallback *onClick = nullptr);
	~Button() override;

	virtual void setState(int state);
	int state() const { return _state; }
	uint32 radioMask() const { return _radioMask; }

	void setOnClick(ButtonCallback *cb);
	void setOnPress(ButtonCallback *cb);
	void setOnRelease(ButtonCallback *cb);
	void setOnHeld(ButtonCallback *cb);
	void setSounds(Sound *click, Sound *press, Sound *release) { _clickSound = click; _pressSound = press; _releaseSound = release; }
	void setPressSound(Sound *s) { _pressSound = s; }
	void setRadio(Pane *parent, uint32 mask, ButtonCallback *onPress);

	void onLDown(const Common::Point &p) override;
	void onLHeld(const Common::Point &p) override;
	void onLUp(const Common::Point &p) override;
	void onMouseMove(const Common::Point &p) override;

	int _tag; ///< free for the owner (e.g. the slot index)

protected:
	virtual void onClick();
	virtual void onPress();
	virtual void onRelease();
	virtual void onHeld();

	int _state;
	bool _pressed;
	Sound *_clickSound;
	Sound *_pressSound;
	Sound *_releaseSound;
	uint32 _radioMask;
	Pane *_parent;
	ButtonCallback *_onClick;
	ButtonCallback *_onPress;
	ButtonCallback *_onRelease;
	ButtonCallback *_onHeld;
};

/** Button drawing images[state] (ImageButton, vt 0x4d02d8) to both pages */
class ImageButton : public Button {
public:
	ImageButton(ButtonCallback *onClick = nullptr);
	~ImageButton() override;

	/** ImageButton_setImages (0x44ed1e): three images (not owned); the rect takes the size of the up image */
	void setImages(Graphics::ManagedSurface *up, Graphics::ManagedSurface *down, Graphics::ManagedSurface *disabled);
	/** ImageButton_setFromSheets (0x44ec01): crop the same rect out of three full-screen sheets (owned copies) */
	void setFromSheets(const Common::Rect &r, const Graphics::ManagedSurface &up, const Graphics::ManagedSurface &down, const Graphics::ManagedSurface &off);
	void setPos(int x, int y);

	void onRefresh() override;

private:
	void freeOwned();

	Graphics::ManagedSurface *_images[3];
	bool _owned;
};

/** A text field that restores the background under it before drawing (TextBox) */
class TextBox {
public:
	TextBox() {}
	void setRect(const Common::Rect &r) { _rect = r; }
	const Common::Rect &rect() const { return _rect; }
	/** Restore the back page under the box, then draw */
	void draw(const BlitFont &font, const Common::String &text, int hAlign, int vAlign, int keyMode, int pages);

private:
	Common::Rect _rect;
};

/**
 * Modal dialog (TDialog, dialogbox.cpp): a frame image with title and message,
 * pushed as the input root while open. The screen under the frame is saved
 * on open and restored on close.
 */
class Dialog : public Pane {
public:
	Dialog();
	~Dialog() override;

	void setFrame(const Common::String &image);
	/** Dialog_setPos (0x452ac3): frame, title, message rects and the three button anchors */
	void setPos(int x, int y);
	void setFonts(const BlitFont *message, const BlitFont *title) { _msgFont = message; _titleFont = title; }
	void setTexts(const Common::String &title, const Common::String &message) { _title = title; _message = message; }

	virtual void open();
	virtual void close();
	bool isOpen() const { return _isOpen; }
	const Common::Rect &frameRect() const { return _frameRect; }

	Common::Point _anchorLeft, _anchorRight, _anchorCenter;
	Common::Rect _titleRect, _messageRect;
	int _msgHAlign, _msgVAlign;

protected:
	virtual void drawContents();

	Graphics::ManagedSurface *_frame;
	Graphics::ManagedSurface *_saved[2];
	Common::Rect _frameRect;
	const BlitFont *_msgFont;
	const BlitFont *_titleFont;
	Common::String _title;
	Common::String _message;
	Widget *_savedRoot;
	Widget *_savedKeyTarget;
	bool _isOpen;
};

/** Yes/no style dialog with two buttons (TPreFabDialog, vt 0x4d10bc); ask() runs a modal loop */
class PreFabDialog : public Dialog {
public:
	PreFabDialog();
	~PreFabDialog() override;

	/** Image pairs per button type, e.g. 0 = OK, 1 = CANCEL */
	void addButtonType(const Common::String &up, const Common::String &down);
	void setButtonPositions(const Common::Point &left, const Common::Point &right, const Common::Point &center);

	/** PreFabDialog_ask (0x453054): true iff the left button was pressed */
	bool ask(const Common::String &title, const Common::String &message, int typeLeft, int typeRight);
	/** PreFabDialog_message (0x453247): one centred button */
	void message(const Common::String &title, const Common::String &message, int type);

private:
	void onLeft(Button *b);
	void onRight(Button *b);
	int run();

	ImageButton _left, _right;
	Common::Array<Graphics::ManagedSurface *> _up, _down;
	Common::Point _posLeft, _posRight, _posCenter;
	int _result;
};

} // End of namespace Gearhead

#endif // GEARHEAD_UI_H
