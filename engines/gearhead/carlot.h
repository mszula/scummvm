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

#ifndef GEARHEAD_CARLOT_H
#define GEARHEAD_CARLOT_H

#include "gearhead/scene3ds.h"
#include "gearhead/ui.h"
#include "gearhead/walkabout.h"

namespace Gearhead {

class CarInstance;
class Sound;

enum {
	kLotSpots = 12
};

/** The 3D lot (LotPane, vt 0x4d0a10, rect (83,97)-(548,385)) */
class LotPane : public Widget {
public:
	LotPane();
	~LotPane() override;

	/** LotPane_loadAssets (0x424f10) */
	void init();
	/** LotPane_reset (0x4259ec) */
	void reset() { _lastSlot = -1; }
	/** LotPane_addCar (0x425a0b): the next spot; nullptr keeps it empty */
	void addCar(CarInstance *car);
	/** LotPane_clear (0x425f4d) */
	void clear();
	/** LotPane_render (0x4261d6) */
	void render();

	CarInstance *hovered() const { return _hovered; }
	Walkabout _walk;

private:
	struct Slot {
		CarInstance *car = nullptr;
		Common::Array<SceneObject> objects;          ///< car mesh clones + shadow
		Common::Array<Common::Array<const Material3D *> > materials;
		Mesh3D shadow;
		Common::Array<const Material3D *> shadowMats;
	};
	/** LotPane_updateHover (0x426000): the car nearest to the horizontal centre */
	void updateHover();

	Scene3DS _scene;
	SceneNode _spots[kLotSpots];
	int _numSpots;
	Slot _slots[kLotSpots];
	int _lastSlot;
	CarInstance *_hovered;
	int _arrowX;
	Texture3D _skyTexture;
	Material3D _skyMaterial;
	Material3D _shadowMaterial;
	Graphics::ManagedSurface *_arrow;
};

/** The car lot (CarLot, vt 0x4d09c0): pick one of up to 12 owned cars for the workshop */
class CarLotScreen : public Pane {
public:
	CarLotScreen();
	/** CarLot_loadAssets (0x423c3b) */
	void init();
	/** CarLot_setActive (0x423da1) */
	void activate();

	/** CarLot_onLeftPress (0x42436f): take the centred car */
	void onLDown(const Common::Point &p) override;
	/** CarLot_onKeyHeld (0x424342) */
	void onKeyDown(int key) override;
	/** CarLot_update (0x4242cd) */
	void onRefresh() override;

private:
	void onBack(Button *b);
	void onLeftHeld(Button *b);
	void onRightHeld(Button *b);
	/** CarLot_updateAmbient (0x423fa1) */
	void updateAmbient();
	void stopSounds();
	/** CarLot_drawCarInfo (0x4240c1) */
	void drawCarInfo();

	Graphics::ManagedSurface *_background;
	LotPane _lot;
	ImageButton _back, _left, _right;
	TextBox _number, _origCost, _repairTime, _repairCost;
	Sound *_field, *_crow1, *_crow2, *_depart;
	uint32 _activation;
	uint32 _lastDepart, _departDelay, _lastCrow, _crowDelay;
	int _crowCount;
	CarInstance *_lastHover; ///< g_CarLotLastHoverCar
};

} // End of namespace Gearhead

#endif // GEARHEAD_CARLOT_H
