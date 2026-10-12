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

#ifndef GEARHEAD_JUNKYARD_H
#define GEARHEAD_JUNKYARD_H

#include "gearhead/partsbin.h"
#include "gearhead/scene3ds.h"
#include "gearhead/ui.h"
#include "gearhead/walkabout.h"

namespace Gearhead {

struct Part;

/** A part lying on a shelf (TriMesh instance sharing the part's geometry) */
struct ShelfItem {
	Part *part = nullptr;
	SceneObject object;
	Common::Array<const Material3D *> materials;
	float radius = 0.0f;
};

/** Shelf (0x4c bytes, Shelf_ctor 0x428560): parts are lined up along it */
class Shelf {
public:
	/** Shelf_init (0x4285f2) */
	void init(const SceneMesh &mesh, const SceneNode &node);
	/** Shelf_tryPlace (0x4289ff) */
	bool tryPlace(Part *part);
	/** Shelf_clear (0x428eb2) */
	void clear();

	Vec3 _dir, _start;
	float _length = 0.0f;
	bool _full = false;
	Common::Array<ShelfItem *> _items; ///< owned
};

/** One yard scene (YardArea, 0x5c bytes): a walkabout and its shelves */
class YardArea {
public:
	~YardArea();
	/** YardArea_init (0x429013) */
	void init(Scene3DS *scene, Widget *listener);
	/** YardArea_addPart (0x428f16) */
	void addPart(Part *part);
	/** YardArea_clear (0x428fcc) */
	void clear();

	Scene3DS *_scene = nullptr; ///< owned
	Walkabout _walk;
	Common::Array<Shelf *> _shelves; ///< owned
};

/** The 3D view of the junkyard (YardPane, vt 0x4d0af8, rect (51,95)-(469,348)) */
class YardPane : public Widget {
public:
	YardPane();
	~YardPane() override;
	/** YardPane_init (0x429b15) */
	void init();
	/** YardPane_populate (0x429fe0) */
	void populate();
	/** YardPane_setArea (0x42a0f6), 1..3 */
	void setArea(int area);
	/** YardPane_render (0x42a149) */
	void render();
	/** YardPane_clearAll */
	void clearAll();

	/** YardPane_onMouseHover (0x42a213) */
	void onHover(const Common::Point &p) override;
	/** YardPane_onLButtonUp (0x42a45b): buy */
	void onLUp(const Common::Point &p) override;

	YardArea *current() const { return _current; }
	/** Test helper: a screen point whose pick hits shelf item @p n of the current area */
	bool debugItemScreenPos(int n, Common::Point &out, Common::String &name) const;
	YardArea _areas[3];
	int _area;

private:
	/** YardPane_pick (0x42a4b6) */
	void pick(const Common::Point &p, Part *&part, ShelfItem *&item) const;
	void restoreItemMaterials(ShelfItem *item);

	YardArea *_current;
	Part *_hoverPart, *_prevPart;
	ShelfItem *_hoverItem, *_prevItem;
	TextBox _label;
};

/** Engine / Body / Running Gear sign (YardAreaPane, vt 0x4d0a68, rect (499,141)-(614,239)) */
class YardAreaPane : public Pane {
public:
	YardAreaPane();
	void init();
	/** YardAreaPane_syncButtons (0x426e75) */
	void syncButtons(int area);

	ImageButton _buttons[3];

private:
	void onArea(Button *b);
};

/** The junkyard (JunkYard, vt 0x4d0ab0, junkyard.cpp) */
class JunkYardScreen : public Pane {
public:
	JunkYardScreen();
	/** JunkYard_init (0x4272a9) */
	void init();
	/** JunkYard_activate (0x427499) */
	void activate();
	/** JunkYard_buyPart (0x4276b1) */
	bool buyPart(Part *part, ShelfItem *item);

	/** JunkYard_onKeyHeld (0x4279bd) */
	void onKeyDown(int key) override;
	/** JunkYard_onFrame (0x4278bf) */
	void onRefresh() override;

	YardPane _yard;
	YardAreaPane _areaPane;

private:
	struct CartEntry {
		Part *part;
		ShelfItem *item;
	};
	void onBack(Button *b);
	void onLeftHeld(Button *b);
	void onRightHeld(Button *b);
	void onCartUp(Button *b);
	void onCartDown(Button *b);
	/** JunkYard_onCartItemClick (0x427afb) -> JunkYard_returnPart (0x42776b) */
	void onCartItem(Button *b);
	/** JunkYard_rebuildCartList (0x42784a) */
	void rebuildCart();

	Graphics::ManagedSurface *_background;
	ImageButton _back, _left, _right, _up, _down;
	PartList _cart;
	Common::Array<CartEntry> _entries;
};

} // End of namespace Gearhead

#endif // GEARHEAD_JUNKYARD_H
