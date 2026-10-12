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

#ifndef GEARHEAD_CARVIEW_H
#define GEARHEAD_CARVIEW_H

#include "gearhead/math3d.h"
#include "gearhead/ui.h"

namespace Gearhead {

class CarInstance;

/**
 * The workshop's 3D car view (CarView, vt 0x4d00dc, rect (5,85)-(385,342)):
 * an orbit camera that eases towards its desired position (75 % per redraw)
 * and frames the whole car automatically (CarView_autoFrame 0x40b03c).
 */
class CarView : public Pane {
public:
	CarView();

	/** Not owned; nullptr = empty workshop */
	void setCar(CarInstance *car, int region);
	CarInstance *car() const { return _car; }

	/** CarView_autoFrame (0x40b03c) */
	void autoFrame();
	/** Arrow keys (CarView_onKey 0x40aea1), called every frame while held */
	void onArrowKey(int scanCode, float frameDelta);

	/** Render if dirty (CarView_draw 0x40a7bb); the camera keeps converging over several redraws */
	void update();

	void onRHeld(const Common::Point &p) override;
	void onMouseMove(const Common::Point &p) override;

private:
	void updateCamera();
	void render();
	void clampPitch();

	CarInstance *_car;
	int _region;
	Vec3 _target, _targetD;
	Vec3 _angles, _anglesD;    ///< x = pitch (from +Y), y = yaw
	float _dist, _distD;
	Mat4 _cameraWorld;
	Common::Point _lastMouse;
	bool _haveLastMouse;
};

} // End of namespace Gearhead

#endif // GEARHEAD_CARVIEW_H
