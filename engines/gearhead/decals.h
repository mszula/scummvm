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

#ifndef GEARHEAD_DECALS_H
#define GEARHEAD_DECALS_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/ui.h"

namespace Graphics {
class ManagedSurface;
}

namespace Gearhead {

class Resources;

/** One decal of a .dpk pack (DecalData, 0x18 bytes, DecalData_load 0x43024e); defaults from the ctor */
struct DecalData {
	int id = 0;
	Common::String name = "Unnamed";
	int colorType = 0;     ///< DD_COLORTYPE: 1 recolour dark pixels, 2 recolour white pixels
	int numPerBuy = 5;
	float cost = 10.0f;
	int width = 0, height = 0;
	Common::Array<byte> rgba; ///< DD_IMAGE (24 bpp in the data)
};

/** Every decal of Data\Decals\*.dpk (g_DecalDatas 0x4edab8, GlobDecal_loadAll 0x431578) */
class DecalLibrary {
public:
	~DecalLibrary();
	void loadAll(Resources &res);
	/** GlobDecal_findById */
	const DecalData *find(int id) const;
	const Common::Array<DecalData *> &all() const { return _decals; }

private:
	Common::Array<DecalData *> _decals;
};

/** A decal the mechanic owns (OwnedDecal, 0xc bytes): MECH_DECAL {DECAL_ID, DECAL_COPIES} */
struct OwnedDecal {
	int id = 0;                     ///< kept for saving even when the pack is gone
	const DecalData *data = nullptr;
	int copies = 0;
};

/** One cell of a decal grid (DecalCell, vt 0x4d0d44, 0x70 bytes): 48x48 thumbnail and a label */
class DecalCell : public Button {
public:
	DecalCell();
	~DecalCell() override;

	void setRect(const Common::Rect &r);
	/** DecalCell_setData (0x4305dc): catalog mode */
	void setData(const DecalData *d);
	/** DecalCell_setRef: owned mode */
	void setOwned(OwnedDecal *od);
	/** DecalCell_draw (0x430451) */
	void draw();

	OwnedDecal *_owned;      ///< +0x48
	const DecalData *_data;  ///< +0x4c

private:
	void makeThumbnail(const DecalData *d);

	Graphics::ManagedSurface *_thumb; ///< +0x50
	Common::Rect _label;              ///< +0x58
};

/** Paged grid of decals (DecalList, vt 0x4d0da4, 0x64 bytes): the Catalog's and the Decal Browser's */
class DecalList : public Pane {
public:
	DecalList();
	~DecalList() override;

	/** DecalList_layout (0x430743): 48x72 cells, gap 8, centred in @p r */
	void layout(const Common::Rect &r, ButtonCallback *onClick);
	/** DecalList_layoutTwo (0x430aae): right page, then left page (whose cells come first), rows doubled */
	void layoutTwo(const Common::Rect &left, const Common::Rect &right, ButtonCallback *onClick);
	/** DecalList_setOwned (0x430b85) */
	void setOwned(Common::Array<OwnedDecal *> *list);
	/** DecalList_setCatalog (0x430bd9) */
	void setCatalog(const Common::Array<DecalData *> *list);
	/** DecalList_fillCells (0x430d0a) */
	void fillCells();
	/** DecalList_nextPage / prevPage (0x430c2d / 0x430c80) */
	bool nextPage();
	bool prevPage();
	/** DecalList_pageFlags (0x430ccb): bit 0 a next page, bit 1 a previous one */
	int pageFlags() const;
	/** The background behind the cells (owned), drawn at the list origin */
	void setBackground(Graphics::ManagedSurface *bg);

	/** DecalList_draw (vslot 14, 0x430e84) */
	void onRefresh() override;

	const Common::Array<DecalCell *> &cells() const { return _cells; }

private:
	void onCellClick(Button *b);
	int count() const;

	Graphics::ManagedSurface *_background; ///< +0x34
	Common::Array<OwnedDecal *> *_owned;   ///< +0x38
	const Common::Array<DecalData *> *_catalog; ///< +0x3c
	Common::Array<DecalCell *> _cells;     ///< +0x40 (owned)
	ButtonCallback *_onClick;
	int _cols, _rows, _page, _lastPage;
};

} // End of namespace Gearhead

#endif // GEARHEAD_DECALS_H
