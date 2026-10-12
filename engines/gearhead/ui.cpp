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
#include "graphics/managed_surface.h"

#include "gearhead/font.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/ui.h"

namespace Gearhead {

// ---------------------------------------------------------------- Widget

Widget::Widget() : _kind(kKindWidget), _dirty(false) {
}

void Widget::handleEvent(const Event &ev) {
	switch (ev.type) {
	case kEvLHeld:
		onLHeld(ev.pos);
		break;
	case kEvLDown:
		onLDown(ev.pos);
		break;
	case kEvLUp:
		onLUp(ev.pos);
		break;
	case kEvRHeld:
		onRHeld(ev.pos);
		break;
	case kEvRDown:
		onRDown(ev.pos);
		break;
	case kEvMouseMove:
		onMouseMove(ev.pos);
		break;
	case kEvHover:
		onHover(ev.pos);
		break;
	case kEvKeyDown:
		onKeyDown(ev.key);
		break;
	case kEvKeyUp:
		onKeyUp(ev.key);
		break;
	case kEvRefresh:
		onRefresh();
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------- Pane

Pane::Pane() : _propagate(true) {
}

void Pane::removeChild(Widget *w) {
	for (uint i = 0; i < _children.size(); i++) {
		if (_children[i] == w) {
			_children.remove_at(i);
			return;
		}
	}
}

void Pane::handleEvent(const Event &ev) {
	Widget::handleEvent(ev);
	// Copy: handlers may change the child list (e.g. by opening a dialog)
	const Common::Array<Widget *> children = _children;
	for (uint i = 0; i < children.size(); i++) {
		Widget *c = children[i];
		if (!c)
			continue;
		if (ev.category() == 1 && !c->contains(ev.pos))
			continue;
		if (ev.category() == 3 && !_propagate)
			continue;
		c->handleEvent(ev);
	}
}

void Pane::radioRelease(Widget *selected) {
	const Button *sel = (Button *)selected;
	for (uint i = 0; i < _children.size(); i++) {
		Widget *c = _children[i];
		if (!c || c == selected || c->_kind != kKindButton)
			continue;
		Button *b = (Button *)c;
		if ((b->radioMask() & sel->radioMask()) != 0 && b->state() != Button::kStateDisabled)
			b->setState(Button::kStateUp);
	}
}

// ---------------------------------------------------------------- Button

Button::Button(ButtonCallback *onClick) : _state(kStateUp), _pressed(false), _clickSound(nullptr),
	_pressSound(nullptr), _releaseSound(nullptr), _radioMask(0), _parent(nullptr), _onClick(onClick),
	_onPress(nullptr), _onRelease(nullptr), _onHeld(nullptr), _tag(0) {
	_kind = kKindButton;
}

Button::~Button() {
	delete _onClick;
	delete _onPress;
	delete _onRelease;
	delete _onHeld;
	if (g_engine && g_engine->_input && g_engine->_input->capture() == this)
		g_engine->_input->releaseCapture();
}

void Button::setOnClick(ButtonCallback *cb) {
	delete _onClick;
	_onClick = cb;
}

void Button::setOnPress(ButtonCallback *cb) {
	delete _onPress;
	_onPress = cb;
}

void Button::setOnRelease(ButtonCallback *cb) {
	delete _onRelease;
	_onRelease = cb;
}

void Button::setOnHeld(ButtonCallback *cb) {
	delete _onHeld;
	_onHeld = cb;
}

void Button::setRadio(Pane *parent, uint32 mask, ButtonCallback *onPress) {
	_parent = parent;
	_radioMask = mask;
	setOnPress(onPress);
}

void Button::setState(int state) {
	if (state != _state) {
		_state = state;
		refresh();
	}
}

void Button::onClick() {
	if (_clickSound)
		_clickSound->play();
	if (_onClick)
		(*_onClick)(this);
}

void Button::onPress() {
	if (_pressSound)
		_pressSound->play();
	if (_onPress)
		(*_onPress)(this);
}

void Button::onRelease() {
	if (_releaseSound)
		_releaseSound->play();
	if (_onRelease)
		(*_onRelease)(this);
}

void Button::onHeld() {
	if (_onHeld)
		(*_onHeld)(this);
}

void Button::onLDown(const Common::Point &p) {
	if (_radioMask) {
		if (_state != kStateDisabled && _state != kStateDown) {
			setState(kStateDown);
			onPress();
			if (_parent)
				_parent->radioRelease(this);
		}
		return;
	}
	if (_state != kStateDisabled) {
		_pressed = true;
		g_engine->_input->setCapture(this);
		setState(kStateDown);
		onPress();
	}
}

void Button::onLHeld(const Common::Point &p) {
	if (_radioMask)
		return;
	if (_state == kStateDown) {
		if (contains(p)) {
			onHeld();
			setState(kStateDown);
		} else {
			setState(kStateUp);
		}
	}
}

void Button::onMouseMove(const Common::Point &p) {
	if (_radioMask)
		return;
	if (_state != kStateDisabled && _pressed)
		setState(contains(p) ? kStateDown : kStateUp);
}

void Button::onLUp(const Common::Point &p) {
	if (_radioMask || !_pressed)
		return;
	_pressed = false;
	if (_state != kStateDisabled) {
		g_engine->_input->releaseCapture();
		setState(kStateUp);
		onRelease();
		if (contains(p))
			onClick();
	} else if (g_engine->fixOriginalBugs()) {
		// Original bug: a button disabled while pressed keeps the mouse captured
		g_engine->_input->releaseCapture();
	}
}

// ---------------------------------------------------------------- ImageButton

ImageButton::ImageButton(ButtonCallback *onClick) : Button(onClick), _owned(false) {
	for (int i = 0; i < 3; i++)
		_images[i] = nullptr;
	// Every image button gets the default press sound (0x44edc7)
	if (g_engine && g_engine->_snd)
		_pressSound = g_engine->_snd->get("button\\button10.wav");
}

ImageButton::~ImageButton() {
	freeOwned();
}

void ImageButton::freeOwned() {
	if (_owned) {
		for (int i = 0; i < 3; i++)
			delete _images[i];
	}
	for (int i = 0; i < 3; i++)
		_images[i] = nullptr;
	_owned = false;
}

void ImageButton::setImages(Graphics::ManagedSurface *up, Graphics::ManagedSurface *down, Graphics::ManagedSurface *disabled) {
	freeOwned();
	_images[0] = up;
	_images[1] = down;
	_images[2] = disabled;
	if (up)
		_rect = Common::Rect(_rect.left, _rect.top, _rect.left + up->w, _rect.top + up->h);
}

void ImageButton::setFromSheets(const Common::Rect &r, const Graphics::ManagedSurface &up,
                                const Graphics::ManagedSurface &down, const Graphics::ManagedSurface &off) {
	freeOwned();
	const Graphics::ManagedSurface *sheets[3] = { &up, &down, &off };
	for (int i = 0; i < 3; i++) {
		_images[i] = new Graphics::ManagedSurface(r.width(), r.height(), sheets[i]->format);
		_images[i]->simpleBlitFrom(*sheets[i], r, Common::Point(0, 0));
	}
	_owned = true;
	_rect = r;
}

void ImageButton::setPos(int x, int y) {
	_rect.moveTo(x, y);
}

void ImageButton::onRefresh() {
	const Graphics::ManagedSurface *img = (_state >= 0 && _state < 3) ? _images[_state] : nullptr;
	if (img)
		g_engine->_gfx->blit(kPageBoth, *img, _rect.left, _rect.top);
}

// ---------------------------------------------------------------- TextBox

void TextBox::draw(const BlitFont &font, const Common::String &text, int hAlign, int vAlign, int keyMode, int pages) {
	if (pages & kPageFront)
		g_engine->_gfx->restoreFromBack(_rect);
	font.drawText(*g_engine->_gfx, text, _rect, hAlign, vAlign, keyMode, pages & kPageFront);
}

// ---------------------------------------------------------------- Dialog

Dialog::Dialog() : _msgHAlign(kAlignCenter), _msgVAlign(kAlignCenter), _takeFocus(false), _framePages(kPageBoth),
	_openSound(true), _closeSound(true), _frame(nullptr), _msgFont(nullptr),
	_titleFont(nullptr), _savedRoot(nullptr), _savedKeyTarget(nullptr), _isOpen(false) {
	_saved[0] = _saved[1] = nullptr;
	_msgFont = &g_engine->_fonts[kFontDialog];
	_titleFont = &g_engine->_fonts[kFontDialogTitle];
}

Dialog::~Dialog() {
	delete _saved[0];
	delete _saved[1];
}

void Dialog::setFrame(const Common::String &image) {
	_frame = g_engine->_res->getImage(image);
}

void Dialog::setPos(int x, int y) {
	const int w = _frame ? _frame->w : 0;
	const int h = _frame ? _frame->h : 0;
	_frameRect = Common::Rect(x, y, x + w, y + h);
	_rect = _frameRect;
	const int lineH = g_engine->_fonts[kFontDialog].lineHeight();
	_titleRect = Common::Rect(x + 20, y + 10, _frameRect.right - 10, y + 10 + lineH);
	_messageRect = Common::Rect(x + 10, _titleRect.bottom + 10, _frameRect.right - 10, _frameRect.bottom - 50);
	_anchorLeft = Common::Point(x + 11, y + 132);
	_anchorRight = Common::Point(x + 128, y + 132);
	_anchorCenter = Common::Point(x + 71, y + 132);
}

void Dialog::open() {
	Input *input = g_engine->_input;
	Gfx *gfx = g_engine->_gfx;
	// Dialog_open (0x452cee): becomes the mouse target and the modal handler
	// (= tick root, so the screen below stops ticking) and takes the key focus
	_savedRoot = input->root();
	_savedKeyTarget = input->keyTarget();
	input->releaseCapture();
	input->setRoot(this);
	if (_takeFocus)
		input->setKeyTarget(nullptr);

	for (int i = 0; i < 2; i++) {
		delete _saved[i];
		Graphics::ManagedSurface &page = i ? gfx->front() : gfx->back();
		Common::Rect r = _frameRect;
		r.clip(Common::Rect(kScreenWidth, kScreenHeight));
		_saved[i] = new Graphics::ManagedSurface(r.width(), r.height(), page.format);
		_saved[i]->simpleBlitFrom(page, r, Common::Point(0, 0));
	}
	if (_frame)
		gfx->blit(_framePages, *_frame, _frameRect.left, _frameRect.top);
	drawContents();
	if (_openSound)
		g_engine->_snd->play("dialog\\dialog8.wav");
	_isOpen = true;
	refresh();
}

void Dialog::setButtons(const Common::Array<Widget *> &own, const Common::Array<Widget *> &extra) {
	clearChildren();
	for (uint i = 0; i < own.size(); i++)
		addChild(own[i]);
	for (uint i = 0; i < extra.size(); i++)
		addChild(extra[i]);
}

void Dialog::drawContents() {
	if (_titleFont && !_title.empty())
		_titleFont->drawText(*g_engine->_gfx, _title, _titleRect, kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
	if (_msgFont && !_message.empty())
		_msgFont->drawText(*g_engine->_gfx, _message, _messageRect, _msgHAlign, _msgVAlign, kKeyDefault, kPageBoth);
}

void Dialog::close() {
	if (!_isOpen)
		return;
	Input *input = g_engine->_input;
	input->setRoot(_savedRoot);
	if (_takeFocus)
		input->setKeyTarget(_savedKeyTarget);
	input->releaseCapture();
	for (int i = 0; i < 2; i++) {
		if (_saved[i])
			g_engine->_gfx->blit(i ? kPageFront : kPageBack, *_saved[i], _frameRect.left, _frameRect.top);
	}
	if (_closeSound)
		g_engine->_snd->play("dialog\\dialog9.wav");
	_isOpen = false;
}

// ---------------------------------------------------------------- PreFabDialog

PreFabDialog::PreFabDialog() : _result(-1) {
	_left.setOnClick(makeCallback(this, &PreFabDialog::onLeft));
	_right.setOnClick(makeCallback(this, &PreFabDialog::onRight));
}

PreFabDialog::~PreFabDialog() {
}

void PreFabDialog::addButtonType(const Common::String &up, const Common::String &down) {
	_up.push_back(g_engine->_res->getImage(up));
	_down.push_back(g_engine->_res->getImage(down));
}

void PreFabDialog::setButtonPositions(const Common::Point &left, const Common::Point &right, const Common::Point &center) {
	_posLeft = left;
	_posRight = right;
	_posCenter = center;
}

void PreFabDialog::onLeft(Button *b) {
	_result = 1;
}

void PreFabDialog::onRight(Button *b) {
	_result = 0;
}

int PreFabDialog::run() {
	_result = -1;
	open();
	while (_result == -1 && g_engine->runFrame()) {
	}
	close();
	return _result;
}

bool PreFabDialog::ask(const Common::String &title, const Common::String &message, int typeLeft, int typeRight) {
	if (_up.size() < 2)
		error("TPreFabDialog button images not loaded.");
	setTexts(title, message);
	_left.setImages(_up[typeLeft], _down[typeLeft], _up[typeLeft]);
	_left.setPos(_posLeft.x, _posLeft.y);
	_left.setState(Button::kStateUp);
	_right.setImages(_up[typeRight], _down[typeRight], _up[typeRight]);
	_right.setPos(_posRight.x, _posRight.y);
	_right.setState(Button::kStateUp);
	clearChildren();
	addChild(&_left);
	addChild(&_right);
	return run() == 1;
}

PreFabDialog *createMessageBox() {
	// MsgBox_initOkCancel (0x40ed40)
	PreFabDialog *d = new PreFabDialog();
	d->setFonts(&g_engine->_fonts[kFontDialog], &g_engine->_fonts[kFontDialogTitle]);
	d->setFrame("Overlays\\Dialog\\Frame.tga");
	d->setPos(200, 135);
	d->addButtonType("Overlays\\Dialog\\OkUP.tga", "Overlays\\Dialog\\OkDN.tga");
	d->addButtonType("Overlays\\Dialog\\CancelUP.tga", "Overlays\\Dialog\\CancelDN.tga");
	d->setButtonPositions(d->_anchorLeft, d->_anchorRight, d->_anchorCenter);
	d->_takeFocus = true;
	return d;
}

void PreFabDialog::message(const Common::String &title, const Common::String &message, int type) {
	setTexts(title, message);
	_left.setImages(_up[type], _down[type], _up[type]);
	_left.setPos(_posCenter.x, _posCenter.y);
	_left.setState(Button::kStateUp);
	clearChildren();
	addChild(&_left);
	run();
}

} // End of namespace Gearhead
