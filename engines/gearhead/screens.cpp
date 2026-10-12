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

#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/resources.h"
#include "gearhead/screens.h"
#include "gearhead/signin.h"
#include "gearhead/sound.h"

namespace Gearhead {

void drawMechanicHud() {
	Mechanic *m = g_engine->_mechanic;
	if (!m)
		return;
	Gfx *gfx = g_engine->_gfx;
	g_engine->_fonts[kFontStandard].drawText(*gfx, m->_name, Common::Rect(335, 15, 454, 33),
	                                          kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
	TextBox cash;
	cash.setRect(Common::Rect(515, 49, 635, 82));
	cash.draw(g_engine->_fonts[kFontCash], Common::String::format("$%4.2f", m->_cash),
	          kAlignEnd, kAlignCenter, kKeyDefault, kPageFront);
}

// ---------------------------------------------------------------- Credits

CreditsScreen::CreditsScreen() : _background(nullptr) {
}

void CreditsScreen::init() {
	Resources *res = g_engine->_res;
	setRect(0, 0, 640, 480);
	_background = res->getImage("CreditsUP.tga");
	_back.setFromSheets(Common::Rect(483, 445, 628, 475), *_background, *res->getImage("CreditsDN.tga"), *_background);
	_back.setOnClick(makeCallback(this, &CreditsScreen::onBack));
	addChild(&_back);
}

void CreditsScreen::show() {
	g_engine->_jukebox->playTrack(2);
	g_engine->_gfx->blit(kPageBoth, *_background, 0, 0);
	g_engine->_input->activateScreen(this);
	refresh();
}

void CreditsScreen::onKeyDown(int key) {
	if (key == kScanEsc)
		back();
}

void CreditsScreen::onBack(Button *b) {
	back();
}

void CreditsScreen::back() {
	g_engine->_signIn->activate(true);
}

} // End of namespace Gearhead
