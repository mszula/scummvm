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

#ifndef GEARHEAD_CATALOG_H
#define GEARHEAD_CATALOG_H

#include "gearhead/decals.h"
#include "gearhead/partsbin.h"
#include "gearhead/ui.h"

namespace Gearhead {

/** ENGINE / BODY / R GEAR / DECALS (CatalogTabs, vt 0x4d07ac, rect (14,121)-(634,425)): radio buttons */
class CatalogTabs : public Pane {
public:
	CatalogTabs();
	/** CatalogTabs_init (0x41ec53) */
	void init();

	ImageButton _engine, _body, _runningGear, _decals;

private:
	void onEngine(Button *b);
	void onBody(Button *b);
	void onRunningGear(Button *b);
	void onDecals(Button *b);
};

/** The parts and decals catalogue (Catalog, 0x40c bytes, vt 0x4d0764, catalog.cpp) */
class CatalogScreen : public Pane {
public:
	enum {
		kPageEngine = 1,
		kPageBody = 2,
		kPageRunningGear = 3,
		kPageDecals = 666
	};

	CatalogScreen();
	/** Catalog_init (0x41d88a) + Catalog_layoutLists (0x41d745) */
	void init();
	/** Catalog_setActive (0x41de63) */
	void activate();
	/** CatalogTabs_cb* + Catalog_applyPage (0x41edb7) */
	void setPage(int page);

	/** Catalog_onTick (vslot 14, 0x41e002): redraw the 3D part cells when needed */
	void onRefresh() override;

	/** Test helper: the cells of the current spread */
	void debugDump() const;

	int _page;     ///< +0x34
	int _prevPage; ///< +0x38
	PartList _parts;
	DecalList _decalList;
	CatalogTabs _tabs;

private:
	/** Catalog_refresh (0x41d9c3) */
	void refreshPage();
	/** Catalog_updatePageButtons (0x41dd04) */
	void updatePageButtons();
	bool hasChild(Widget *w) const;
	void onBack(Button *b);
	/** Catalog_nextPage (0x41ddb5) / Catalog_prevPage (0x41de0c) */
	void onNextPage(Button *b);
	void onPrevPage(Button *b);
	/** Catalog_onBuyPart (0x41e0d1) */
	void onBuyPart(Button *b);
	/** Catalog_onBuyDecal (0x41e2c9) */
	void onBuyDecal(Button *b);

	Graphics::ManagedSurface *_background;
	ImageButton _back, _next, _prev;
};

} // End of namespace Gearhead

#endif // GEARHEAD_CATALOG_H
