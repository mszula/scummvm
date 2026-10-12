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
class Renderer3D;
struct Mesh3D;
struct Part;

/** One triangle hit of a pick ray */
struct PickHit {
	int mesh = -1;      ///< car mesh index, or -1
	int extra = -1;     ///< index of an extra object (bolt cube), or -1
	float t = 0.0f;     ///< ray parameter
};

/**
 * The workshop's 3D car view (CarView, vt 0x4d00dc, rect (5,85)-(385,342)):
 * an orbit camera that eases towards its desired position (75 % per redraw)
 * and frames the whole car automatically (CarView_autoFrame 0x40b03c).
 * Mouse events are passed on to the current workshop tool.
 */
class CarView : public Pane {
public:
	CarView();

	/** The car shown (the mechanic's current car); not owned, nullptr = empty workshop */
	void setCar(CarInstance *car) { _car = car; }
	CarInstance *car() const { return _car; }

	/** CarView_autoFrame (0x40b03c), also refreshes the ASSEMBLED light */
	void autoFrame();
	/** CarView_onKey (0x40aea1), called every frame while an arrow key is held */
	void onArrowKey(int scanCode, float frameDelta);
	/** CarView_focusPart (0x40bab3): zoom onto a part (bolt work) */
	void focusPart(const Part *part);
	void clearFocus() { _focus = -1; }
	/** CarView_updateCamera (0x40bd7e) */
	void updateCamera();
	void setGoalPitch(float pitch) { _anglesD.x = pitch; }

	/** CarView_draw (0x40a7bb) */
	void draw();

	/** Ray through a screen point (Ray_fromScreen), in car space */
	void pickRay(const Common::Point &p, Vec3 &origin, Vec3 &dir) const;
	/** Nearest visible car mesh (and extra object) hit by the ray */
	PickHit pick(const Vec3 &origin, const Vec3 &dir, bool withExtras) const;
	const Mat4 &cameraWorld() const { return _cameraWorld; }
	/** Screen position of a car-space point with the current camera (false if behind) */
	bool project(const Vec3 &p, Common::Point &out) const;

	void onHover(const Common::Point &p) override;
	void onLDown(const Common::Point &p) override;
	void onLUp(const Common::Point &p) override;
	void onLHeld(const Common::Point &p) override;
	void onRDown(const Common::Point &p) override;
	void onRHeld(const Common::Point &p) override;
	void onRefresh() override { setDirty(true); }

	bool _mouseOver;     ///< +0x734
	bool _inputDisabled; ///< +0x735 (job-completion spin)

private:
	void render();
	void updateAssemblyLight();
	void clampPitch();

	CarInstance *_car;
	Vec3 _target, _targetD;
	Vec3 _angles, _anglesD;    ///< x = pitch (from +Y), y = yaw
	float _dist, _distD;
	Mat4 _cameraWorld;
	int _focus;                ///< +0x794 focused mesh, -1 = automatic framing
	bool _assembled;           ///< +0x784
	int _assembledColour;      ///< 0 R, 1 Y, 2 G
	Graphics::ManagedSurface *_useKeys;
	Graphics::ManagedSurface *_assem[3];
};

} // End of namespace Gearhead

#endif // GEARHEAD_CARVIEW_H
