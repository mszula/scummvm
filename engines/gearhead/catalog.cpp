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

#include "gearhead/car.h"
#include "gearhead/catalog.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobs.h"
#include "gearhead/mechanic.h"
#include "gearhead/paint.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

// ---------------------------------------------------------------- CatalogTabs

CatalogTabs::CatalogTabs() {
	setRect(14, 121, 634, 425);
	_engine.setRadio(this, 2, makeCallback(this, &CatalogTabs::onEngine));
	_body.setRadio(this, 2, makeCallback(this, &CatalogTabs::onBody));
	_runningGear.setRadio(this, 2, makeCallback(this, &CatalogTabs::onRunningGear));
	_decals.setRadio(this, 2, makeCallback(this, &CatalogTabs::onDecals));
	addChild(&_engine);
	addChild(&_body);
	addChild(&_runningGear);
	addChild(&_decals);
}

void CatalogTabs::init() {
	Resources *res = g_engine->_res;
	const Graphics::ManagedSurface &up = *res->getImage("CatalogUP.tga");
	const Graphics::ManagedSurface &dn = *res->getImage("CatalogDN.tga");
	const Graphics::ManagedSurface &off = *res->getImage("Catalog.tga");
	_engine.setFromSheets(Common::Rect(575, 121, 624, 230), up, dn, off);
	_body.setFromSheets(Common::Rect(580, 239, 628, 314), up, dn, off);
	_runningGear.setFromSheets(Common::Rect(584, 322, 634, 425), up, dn, off);
	_decals.setFromSheets(Common::Rect(14, 122, 64, 232), up, dn, off);
	Sound *s = g_engine->_snd->get("catalog\\catalog9.wav");
	_engine.setPressSound(s);
	_body.setPressSound(s);
	_runningGear.setPressSound(s);
	_decals.setPressSound(s);
}

void CatalogTabs::onEngine(Button *b) {
	g_engine->_catalog->setPage(CatalogScreen::kPageEngine);
}

void CatalogTabs::onBody(Button *b) {
	g_engine->_catalog->setPage(CatalogScreen::kPageBody);
}

void CatalogTabs::onRunningGear(Button *b) {
	g_engine->_catalog->setPage(CatalogScreen::kPageRunningGear);
}

void CatalogTabs::onDecals(Button *b) {
	g_engine->_catalog->setPage(CatalogScreen::kPageDecals);
}

// ---------------------------------------------------------------- CatalogScreen

CatalogScreen::CatalogScreen() : _page(kPageEngine), _prevPage(0), _background(nullptr) {
	setRect(0, 0, 640, 480);
	_back.setOnClick(makeCallback(this, &CatalogScreen::onBack));
	_next.setOnClick(makeCallback(this, &CatalogScreen::onNextPage));
	_prev.setOnClick(makeCallback(this, &CatalogScreen::onPrevPage));
	addChild(&_parts);
	addChild(&_tabs);
	addChild(&_back);
	addChild(&_next);
	addChild(&_prev);
	// The part cells are drawn on the page's paper colour
	_parts._dataMode = true;
	_parts._clearColour = Vec3(0.69f, 0.67f, 0.65f);
}

void CatalogScreen::init() {
	Resources *res = g_engine->_res;
	_background = res->getImage("Catalog.tga");
	_tabs.init();
	const Graphics::ManagedSurface &up = *res->getImage("CatalogUP.tga");
	const Graphics::ManagedSurface &dn = *res->getImage("CatalogDN.tga");
	_back.setFromSheets(Common::Rect(332, 48, 499, 81), up, dn, up);
	_next.setFromSheets(Common::Rect(512, 436, 597, 449), up, dn, *_background);
	_prev.setFromSheets(Common::Rect(42, 436, 123, 449), up, dn, *_background);
	// The page buttons click with catalog19 and have no press sound
	Sound *page = g_engine->_snd->get("catalog\\catalog19.wav");
	_next.setSounds(page, nullptr, nullptr);
	_prev.setSounds(page, nullptr, nullptr);

	// Catalog_layoutLists: the two pages of the book, 2x5 parts or 4x4 decals each
	const Common::Rect left(58, 105, 308, 436), right(331, 105, 581, 436);
	_parts.layoutTwoPages(left, right, 2, 5, 15, makeCallback(this, &CatalogScreen::onBuyPart), nullptr);
	_decalList.layoutTwo(left, right, makeCallback(this, &CatalogScreen::onBuyDecal));
	_decalList.setCatalog(&g_engine->_decals.all());
	// The decal pages are dark: CatalogDC.tga cut to the list rect
	const Graphics::ManagedSurface *dc = res->getImage("CatalogDC.tga");
	if (dc) {
		const Common::Rect &r = _decalList.rect();
		Graphics::ManagedSurface *bg = new Graphics::ManagedSurface(r.width(), r.height(), dc->format);
		bg->simpleBlitFrom(*dc, r, Common::Point(0, 0));
		_decalList.setBackground(bg);
	}
}

bool CatalogScreen::hasChild(Widget *w) const {
	for (uint i = 0; i < _children.size(); i++)
		if (_children[i] == w)
			return true;
	return false;
}

void CatalogScreen::activate() {
	debugC(1, kDebugGame, "Setting Catalog Active..");
	Mechanic *m = g_engine->_mechanic;
	m->saveIfReal();
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageBoth, *_background, 0, 0);
	m->drawName();
	m->drawCash();
	g_engine->_input->activateScreen(this);
	WorkShopScreen *ws = g_engine->_workShop;
	ws->showToolCursor(false);
	_parts.setListDirty(true);
	// The page follows the workshop tool: the Air Ratchet's region (COMPLETE -> BODY), the Spray Can's decals
	if (ws->tool() == &ws->_airRatchet) {
		_page = ws->_region;
		if (_page == 0)
			_page = kPageBody;
	} else if (ws->tool() == ws->_sprayCan) {
		_page = kPageDecals;
	}
	refreshPage();
	refresh();
	debugC(1, kDebugGame, "..Catalog Set Active.");
}

void CatalogScreen::refreshPage() {
	Mechanic *m = g_engine->_mechanic;
	Gfx *gfx = g_engine->_gfx;
	_tabs._engine.setStateSilently(_page == kPageEngine ? Button::kStateDown : Button::kStateUp);
	_tabs._body.setStateSilently(_page == kPageBody ? Button::kStateDown : Button::kStateUp);
	_tabs._runningGear.setStateSilently(_page == kPageRunningGear ? Button::kStateDown : Button::kStateUp);
	_tabs._decals.setStateSilently(_page == kPageDecals ? Button::kStateDown : Button::kStateUp);
	// Nothing is listed without a car in the workshop
	if (!m->_car)
		return;
	if (_page == kPageDecals) {
		if (!hasChild(&_decalList))
			addChild(&_decalList);
		removeChild(&_parts);
		_parts.setListDirty(false);
		gfx->blit(kPageFront, *_background, 0, 0);
		m->drawName();
		m->drawCash();
		refresh();
	} else {
		// Every part of the car's model in this region (custom ones only if the job allows them)
		_parts.clearItems();
		const CarModel *model = m->_car->model();
		for (uint i = 0; i < model->_parts.size(); i++) {
			const PartDef *pd = model->_parts[i];
			if (pd->region != _page)
				continue;
			if (m->_job && !m->_job->isPartAllowed(pd))
				continue;
			_parts.addPartData(model, pd);
		}
		_parts.refreshList();
		if (_prevPage == kPageDecals) {
			if (!hasChild(&_parts))
				addChild(&_parts);
			removeChild(&_decalList);
			gfx->blit(kPageBoth, *_background, 0, 0);
			m->drawName();
			m->drawCash();
			refresh();
		}
	}
	updatePageButtons();
	_prevPage = _page;
}

void CatalogScreen::updatePageButtons() {
	const int flags = _page == kPageDecals ? _decalList.pageFlags() : _parts.pageFlags();
	_prev.setState(flags & 2 ? Button::kStateUp : Button::kStateDisabled);
	_next.setState(flags & 1 ? Button::kStateUp : Button::kStateDisabled);
}

void CatalogScreen::setPage(int page) {
	_page = page;
	refreshPage();
	_tabs.refresh();
}

void CatalogScreen::onRefresh() {
	if (!_parts.listDirty())
		return;
	_parts.setListDirty(false);
	_parts.draw();
}

void CatalogScreen::onBack(Button *b) {
	g_engine->_workShop->activate();
}

void CatalogScreen::onNextPage(Button *b) {
	if (_page == kPageDecals ? _decalList.nextPage() : _parts.nextPage())
		updatePageButtons();
}

void CatalogScreen::onPrevPage(Button *b) {
	if (_page == kPageDecals ? _decalList.prevPage() : _parts.prevPage())
		updatePageButtons();
}

void CatalogScreen::onBuyPart(Button *b) {
	const PartDef *pd = static_cast<PartButton *>(b)->_def;
	if (!pd)
		return;
	const Common::String text = "This " + pd->name + " will cost you $" + Common::String::format("%4.2f", pd->costMax) +
	                            ". Do you want to buy it?";
	if (g_engine->_msgBox->ask("Buy Part", text, kDlgButtonOk, kDlgButtonCancel))
		buyPart(pd);
}

bool CatalogScreen::buyPart(const PartDef *pd) {
	Mechanic *m = g_engine->_mechanic;
	// The price is the top price, whole dollars; the part is new
	if (!m->pay((int)pd->costMax, true))
		return false;
	if (m->_car && m->_bank)
		m->_bank->bin.push_back(createPart(m->_car->model(), pd, 3, g_engine->_rnd));
	return true;
}

void CatalogScreen::onBuyDecal(Button *b) {
	const DecalData *d = static_cast<DecalCell *>(b)->_data;
	if (!d)
		return;
	const Common::String text = Common::String::format("%i", d->numPerBuy) + " \"" + d->name + "\" decals cost $" +
	                            Common::String::format("%4.2f", d->cost) + ". Do you want to buy them?";
	if (g_engine->_msgBox->ask("Buy Decal", text, kDlgButtonOk, kDlgButtonCancel))
		g_engine->_mechanic->buyDecal(d);
}

void CatalogScreen::debugDump() const {
	debug("dump catalog page %d", _page);
	if (_page == kPageDecals) {
		for (uint i = 0; i < _decalList.cells().size(); i++) {
			const DecalCell *c = _decalList.cells()[i];
			if (c->_data)
				debug("dump decal %d '%s' at %d %d", i, c->_data->name.c_str(), c->rect().left + 24, c->rect().top + 24);
		}
	} else {
		for (uint i = 0; i < _parts.children().size(); i++) {
			const PartButton *pb = static_cast<const PartButton *>(_parts.children()[i]);
			if (pb->_def)
				debug("dump catpart '%s' $%.2f at %d %d", pb->_def->name.c_str(), pb->_def->costMax,
				      (pb->rect().left + pb->rect().right) / 2, (pb->rect().top + pb->rect().bottom) / 2);
		}
	}
	const Mechanic *m = g_engine->_mechanic;
	for (uint i = 0; i < m->_decals.size(); i++)
		debug("dump owned decal %d x%d", m->_decals[i]->id, m->_decals[i]->copies);
}

} // End of namespace Gearhead
