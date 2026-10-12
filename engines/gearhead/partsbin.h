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

#ifndef GEARHEAD_PARTSBIN_H
#define GEARHEAD_PARTSBIN_H

#include "gearhead/math3d.h"
#include "gearhead/ui.h"

namespace Gearhead {

class PartList;
class Renderer3D;
class Sound;
struct Part;

/** One cell of a part list (PartButton, vt 0x4d0e1c): a spinning 3D thumbnail and the name */
class PartButton : public Button {
public:
	PartButton();

	/** PartButton_update (0x4392cd): hovered cells spin at 4 rad/s */
	void onHover(const Common::Point &p) override;
	/** PartButton_onClick (vslot 14): the list redraws */
	void onRefresh() override;

	/** PartButton_drawPart (0x439362) */
	void drawPart(Renderer3D &r3d);

	Part *_part;      ///< +0x48 (not owned)
	Mat4 _rotation;   ///< +0x50
	PartList *_owner; ///< +0x90
	bool _selected;   ///< +0x94
};

/** Paged grid of parts (PartList, vt 0x4d0e80, partlist.cpp), here only in "Parts" mode */
class PartList : public Pane {
public:
	PartList();
	~PartList() override;

	/** PartList_layoutGrid (0x43af91): cell callbacks cbA (click) and cbB (press) */
	void layoutGrid(const Common::Rect &r, int cols, int rows, int gap, ButtonCallback *onClick, ButtonCallback *onPress);
	void clearItems() { _items.clear(); }
	void addPart(Part *p) { _items.push_back(p); }
	/** PartList_refresh (0x43b213) */
	void refreshList();
	/** PartList_fillPage (0x43b29f) */
	void fillPage();
	/** PartList_nextPage / prevPage (0x43b4fa / 0x43b53c) */
	bool nextPage();
	bool prevPage();
	/** PartList_selectByPartId (0x43bb7d) */
	void selectByPartId(int id);
	void clearSelection();
	/** PartList_draw (0x43bc93) */
	void draw();

	bool listDirty() const { return isDirty(); }
	void setListDirty(bool d) { setDirty(d); }
	/** The callbacks shared by all cells (bin: press = drag; junkyard cart: click = return) */
	ButtonCallback *_onClick;
	ButtonCallback *_onPress;

	/** PartList_vslot11: the pointer is over the list */
	void onHover(const Common::Point &p) override { _hover = true; }

	bool _hover;              ///< +0x90
	bool _showPageNumber;     ///< +0x91
	TextBox _pageNumber;      ///< +0x94

private:
	void onButtonPress(Button *b);
	void onButtonClick(Button *b);
	void drawConditionCorner(const PartButton &b);
	void drawButtonFrame(const PartButton &b);

	Common::Array<PartButton *> _buttons;
	Common::Array<Part *> _items;
	int _cols, _rows, _page, _lastPage;
};

/** The workshop's Parts Bin (PartsBin, vt 0x4d05fc, rect (2,368)-(513,484)) */
class PartsBin : public Pane {
public:
	PartsBin();
	void init();
	/** PartsBin_refill (0x416ddc): the bank's bin parts of the current region */
	void refill();
	void clear();
	void render() { _list.draw(); }
	void onRefresh() override { _list.setListDirty(true); }

	PartList _list;
	ImageButton _up, _down;
	Button _repair, _scrap;

private:
	/** PartsBin_onDragPart (0x416e97) */
	void onDragPart(Button *b);
	void onScrollUp(Button *b);
	void onScrollDown(Button *b);
	void onRepairClick(Button *b);
	void onScrapClick(Button *b);
	/** Screen_dragImage (0x454260): drag a screen area while the left button is held */
	Common::Point dragImage(const Common::Rect &r);

	Sound *_pageSound;
	Common::Rect _repairRect, _scrapRect;
};

} // End of namespace Gearhead

#endif // GEARHEAD_PARTSBIN_H
