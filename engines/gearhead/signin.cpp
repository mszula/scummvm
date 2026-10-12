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
#include "common/savefile.h"
#include "graphics/managed_surface.h"

#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/resources.h"
#include "gearhead/screens.h"
#include "gearhead/signin.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/**
 * NewMechDlg_init (0x42adfc) / SignIn_initPrefabDlg (0x42d5a0) adjust the
 * rects computed by Dialog_setPos: the title loses 6 pixels at the bottom
 * and the message covers (x+10, y+10)-(R-10, B-40).
 */
static void adjustSignInDialogRects(Dialog &d) {
	const Common::Rect &f = d.frameRect();
	d._titleRect.bottom -= 6;
	d._messageRect = Common::Rect(f.left + 10, f.top + 10, f.right - 10, f.bottom - 40);
}

// ---------------------------------------------------------------- ProfileRow

ProfileRow::ProfileRow() : _highlighted(false), _owner(nullptr), _slot(0), _y(0), _strip(nullptr),
	_stripHighlighted(nullptr), _summary(nullptr) {
}

ProfileRow::~ProfileRow() {
	delete _strip;
	delete _stripHighlighted;
	delete _summary;
}

void ProfileRow::init(SignInScreen *owner, int slot, int y) {
	_owner = owner;
	_slot = slot;
	_y = y;
	setRect(0, y, 640, y + 20);
	_clickRect = Common::Rect(130, y, 629, y + 20);

	Resources *res = g_engine->_res;
	_delete.setImages(res->getImage("Overlays\\SignIn\\DeleteUP.tga"), res->getImage("Overlays\\SignIn\\DeleteDN.tga"),
	                  res->getImage("Overlays\\SignIn\\DeleteDIS.tga"));
	_delete.setPos(11, y);
	_delete.setOnClick(makeCallback(this, &ProfileRow::onDelete));
	_new.setImages(res->getImage("Overlays\\SignIn\\NewUP.tga"), res->getImage("Overlays\\SignIn\\NewDN.tga"),
	               res->getImage("Overlays\\SignIn\\NewDIS.tga"));
	_new.setPos(86, y);
	_new.setOnClick(makeCallback(this, &ProfileRow::onNew));
	addChild(&_delete);
	addChild(&_new);

	static const int kColumns[5][2] = { { 130, 303 }, { 308, 399 }, { 403, 478 }, { 483, 536 }, { 541, 629 } };
	for (int i = 0; i < 5; i++)
		_columns[i].setRect(Common::Rect(kColumns[i][0], y, kColumns[i][1], y + 20));

	// Row strips: crops of the background at (130, y-2)-(629, y+22); the
	// highlighted one gets a 2-pixel frame of colour (0.47, 0.50, 0.84)
	const Common::Rect stripRect(130, y - 2, 629, y + 22);
	const Graphics::ManagedSurface &bg = *owner->_background;
	_strip = new Graphics::ManagedSurface(stripRect.width(), stripRect.height(), bg.format);
	_strip->simpleBlitFrom(bg, stripRect, Common::Point(0, 0));
	_stripHighlighted = new Graphics::ManagedSurface(stripRect.width(), stripRect.height(), bg.format);
	_stripHighlighted->simpleBlitFrom(bg, stripRect, Common::Point(0, 0));
	const uint32 frame = g_engine->_gfx->rgb(0.47f, 0.50f, 0.84f);
	const int w = _stripHighlighted->w, h = _stripHighlighted->h;
	for (int py = 0; py < h; py++)
		for (int px = 0; px < w; px++)
			if (px < 2 || py < 2 || px > w - 3 || py > h - 3)
				_stripHighlighted->setPixel(px, py, frame);

	clearProfile();
}

void ProfileRow::setProfile(const MechSummary &summary) {
	delete _summary;
	_summary = new MechSummary(summary);
	_delete.setState(Button::kStateUp);
	_new.setState(Button::kStateDisabled);
}

void ProfileRow::clearProfile() {
	delete _summary;
	_summary = nullptr;
	_delete.setState(Button::kStateDisabled);
	_new.setState(Button::kStateUp);
}

void ProfileRow::onLUp(const Common::Point &p) {
	if (_summary && p.x >= _clickRect.left && p.x <= _clickRect.right && p.y >= _clickRect.top && p.y <= _clickRect.bottom)
		_owner->signIn(this);
}

void ProfileRow::onHover(const Common::Point &p) {
	_owner->_currentRow = this;
}

void ProfileRow::onRefresh() {
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageFront, _highlighted ? *_stripHighlighted : *_strip, 130, _y - 2);
	if (_summary) {
		const BlitFont &font = g_engine->_fonts[kFontSignIn];
		const Common::String *texts[5] = { &_summary->name, &_summary->skill, &_summary->workTime, &_summary->cars, &_summary->cash };
		for (int i = 0; i < 5; i++)
			font.drawText(*gfx, *texts[i], _columns[i].rect(), kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
	}
}

void ProfileRow::onNew(Button *b) {
	_owner->_newMechDialog->open();
}

void ProfileRow::onDelete(Button *b) {
	_owner->askDelete(this);
}

// ---------------------------------------------------------------- TextInput

void TextInput::reset() {
	_text = "-";
	for (int i = 0; i < 256; i++)
		_keyDown[i] = false;
}

void TextInput::onKeyHook(int scanCode, bool released, int ascii) {
	const byte a = (byte)ascii;
	if (released) {
		_keyDown[a] = false;
		return;
	}
	if (_keyDown[a])
		return; // key repeat is ignored until the key is released
	_keyDown[a] = true;

	if (scanCode == kScanEsc) {
		if (_onCancel)
			(*_onCancel)();
		return;
	}
	if (scanCode == kScanBackspace) {
		_text.chop(MIN<uint>(2, _text.size()));
		_text += '-';
	} else if (scanCode == kScanEnter) {
		if (_onOk)
			(*_onOk)();
		return;
	} else if (a != 0 && _text.size() + 2 < 20) {
		_text.deleteLastChar();
		_text += (char)a;
		_text += '-';
	}
	if (_onChange)
		(*_onChange)();
}

// ---------------------------------------------------------------- NewMechDialog

NewMechDialog::NewMechDialog(SignInScreen *owner) : _owner(owner) {
	_takeFocus = true; // NewMechDlg_ctor (0x42ac70)
	_input._onOk = new Common::Functor0Mem<void, NewMechDialog>(this, &NewMechDialog::ok);
	_input._onCancel = new Common::Functor0Mem<void, NewMechDialog>(this, &NewMechDialog::cancel);
	_input._onChange = new Common::Functor0Mem<void, NewMechDialog>(this, &NewMechDialog::drawField);
}

NewMechDialog::~NewMechDialog() {
	delete _input._onOk;
	delete _input._onCancel;
	delete _input._onChange;
}

void NewMechDialog::init() {
	Resources *res = g_engine->_res;
	setFrame("Overlays\\SignIn\\Frame.tga");
	setPos(206, 122);
	adjustSignInDialogRects(*this);
	const BlitFont *font = &g_engine->_fonts[kFontSignIn];
	setFonts(font, font);
	setTexts("Create New Mechanic", "Enter your name...");

	_ok.setImages(res->getImage("Overlays\\SignIn\\OkUP.tga"), res->getImage("Overlays\\SignIn\\OkDN.tga"),
	              res->getImage("Overlays\\SignIn\\OkUP.tga"));
	_ok.setPos(216, 267);
	_ok.setOnClick(makeCallback(this, &NewMechDialog::onOk));
	_cancel.setImages(res->getImage("Overlays\\SignIn\\CancelUP.tga"), res->getImage("Overlays\\SignIn\\CancelDN.tga"),
	                  res->getImage("Overlays\\SignIn\\CancelUP.tga"));
	_cancel.setPos(345, 267);
	_cancel.setOnClick(makeCallback(this, &NewMechDialog::onCancel));
	clearChildren();
	addChild(&_ok);
	addChild(&_cancel);

	_field.setRect(Common::Rect(243, 238, 407, 256));
}

void NewMechDialog::open() {
	Dialog::open();
	_input.reset();
	g_engine->_input->setKeyHook(&_input);
	drawField();
}

void NewMechDialog::close() {
	g_engine->_input->setKeyHook(nullptr);
	Dialog::close();
}

void NewMechDialog::drawField() {
	_field.draw(g_engine->_fonts[kFontSignIn], _input.text(), kAlignStart, kAlignCenter, kKeyDefault, kPageFront);
}

void NewMechDialog::onOk(Button *b) {
	ok();
}

void NewMechDialog::onCancel(Button *b) {
	cancel();
}

void NewMechDialog::ok() {
	// NewMechDlg_onOk (0x42afe6): at least one character before the cursor
	if (_input.text().size() <= 1)
		return;
	Common::String name = _input.text();
	name.deleteLastChar();
	name.trim();
	_owner->createMechanic(name);
	close();
	_owner->repaint();
}

void NewMechDialog::cancel() {
	close();
}

// ---------------------------------------------------------------- SignInScreen

SignInScreen::SignInScreen() : _currentRow(nullptr), _background(nullptr), _newMechDialog(nullptr),
	_prefab(nullptr), _zipper(nullptr), _active(false) {
}

SignInScreen::~SignInScreen() {
	delete _newMechDialog;
	delete _prefab;
}

void SignInScreen::init() {
	Resources *res = g_engine->_res;
	setRect(0, 0, 640, 480);
	_background = res->getImage("SignIn.tga");

	_credits.setImages(res->getImage("Overlays\\SignIn\\CreditsUP.tga"), res->getImage("Overlays\\SignIn\\CreditsDN.tga"),
	                   res->getImage("Overlays\\SignIn\\CreditsUP.tga"));
	_credits.setPos(449, 53);
	_credits.setOnClick(makeCallback(this, &SignInScreen::onCredits));
	_exit.setImages(res->getImage("Overlays\\SignIn\\ExitUP.tga"), res->getImage("Overlays\\SignIn\\ExitDN.tga"),
	                res->getImage("Overlays\\SignIn\\ExitUP.tga"));
	_exit.setPos(468, 85);
	_exit.setOnClick(makeCallback(this, &SignInScreen::onExit));
	addChild(&_exit);
	addChild(&_credits);

	_zipper = g_engine->_snd->get("misc\\zipper5.wav");

	for (int i = 0; i < kNumMechanicSlots; i++) {
		_rows[i].init(this, i, 162 + 24 * i);
		addChild(&_rows[i]);
	}

	_newMechDialog = new NewMechDialog(this);
	_newMechDialog->init();

	_prefab = new PreFabDialog();
	_prefab->setFrame("Overlays\\SignIn\\Frame.tga");
	_prefab->setPos(206, 122);
	adjustSignInDialogRects(*_prefab);
	const BlitFont *font = &g_engine->_fonts[kFontSignIn];
	_prefab->setFonts(font, font);
	_prefab->addButtonType("Overlays\\SignIn\\OkUP.tga", "Overlays\\SignIn\\OkDN.tga");
	_prefab->addButtonType("Overlays\\SignIn\\CancelUP.tga", "Overlays\\SignIn\\CancelDN.tga");
	_prefab->setButtonPositions(Common::Point(216, 267), Common::Point(345, 267), Common::Point(281, 267));
}

void SignInScreen::activate(bool saveFirst) {
	debugC(1, kDebugGame, "Setting SignIn Active..");
	Mechanic *m = g_engine->_mechanic;
	if (m && m->isDev()) {
		// Signing out with a dev mechanic (CarDev / JobDev) ends the game
		g_engine->requestQuit();
		return;
	}
	g_engine->_jukebox->playTrack(0);
	if (saveFirst && m) {
		Common::WriteStream *ws = g_engine->createMechanic(m->_id);
		if (ws) {
			m->save(*ws);
			ws->finalize();
			delete ws;
		}
	}
	refreshRows();
	g_engine->_gfx->blit(kPageBoth, *_background, 0, 0);
	g_engine->_input->activateScreen(this);
	_active = true;
	repaint();
	debugC(1, kDebugGame, "..SignIn Set Active.");
}

void SignInScreen::refreshRows() {
	for (int i = 0; i < kNumMechanicSlots; i++) {
		Common::SeekableReadStream *s = g_engine->openMechanic(i);
		MechSummary summary;
		if (s && summary.load(*s))
			_rows[i].setProfile(summary);
		else
			_rows[i].clearProfile();
		delete s;
	}
}

void SignInScreen::repaint() {
	refresh();
}

void SignInScreen::onRefresh() {
	// SignIn_onFrame (0x42bc01): pressed-state highlight of the row under the pointer
	const Common::Point &p = g_engine->_input->mouse();
	const bool left = g_engine->_input->buttons() & 1;
	for (int i = 0; i < kNumMechanicSlots; i++) {
		ProfileRow &row = _rows[i];
		const Common::Rect &r = row._clickRect;
		const bool hl = left && row.hasProfile() && p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
		if (hl != row._highlighted) {
			row._highlighted = hl;
			row.refresh();
		}
	}
}

void SignInScreen::signIn(ProfileRow *row) {
	// ProfileRow_onLButtonUp (0x42c5d8)
	g_engine->_jukebox->stopTrack();
	// The original loads Mechanics\<MECH_ID>.mek, not the slot's file
	const int id = g_engine->fixOriginalBugs() ? row->slot() : row->summary()->id;
	Common::SeekableReadStream *s = g_engine->openMechanic(id);
	Mechanic *m = new Mechanic();
	if (!s || !m->load(*s)) {
		delete s;
		delete m;
		warning("Cannot load mechanic %d", id);
		return;
	}
	delete s;
	delete g_engine->_mechanic;
	g_engine->_mechanic = m;
	m->_id = row->summary()->id;
	_zipper->play();
	m->_clockTime = g_engine->gameTimeMs();
	_active = false;
	g_engine->_workShop->activate();
}

void SignInScreen::createMechanic(const Common::String &name) {
	// SignIn_createMechanic (0x42bb0f): slot of the row under the pointer (10 if none)
	int slot = kNumMechanicSlots;
	for (int i = 0; i < kNumMechanicSlots; i++)
		if (_currentRow == &_rows[i])
			slot = i;
	Mechanic m;
	m.init(name, slot, 5000.0f, 0);
	Common::WriteStream *ws = g_engine->createMechanic(slot);
	if (ws) {
		m.save(*ws);
		ws->finalize();
		delete ws;
	}
	refreshRows();
}

void SignInScreen::askDelete(ProfileRow *row) {
	if (!row->hasProfile())
		return;
	if (_prefab->ask("Delete " + row->summary()->name + " ?", "Are you sure you want to delete this mechanic forever?",
	                 kDlgButtonOk, kDlgButtonCancel)) {
		g_engine->deleteMechanic(row->slot());
		row->clearProfile();
		refreshRows();
	}
	repaint();
}

void SignInScreen::onExit(Button *b) {
	if (_prefab->ask("Exit", "Are you sure you want to exit Gearhead Garage?", kDlgButtonOk, kDlgButtonCancel))
		g_engine->requestQuit();
	repaint();
}

void SignInScreen::onCredits(Button *b) {
	_active = false;
	g_engine->_credits->show();
}

} // End of namespace Gearhead
