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

#ifndef GEARHEAD_WALKABOUT_H
#define GEARHEAD_WALKABOUT_H

#include "common/rect.h"

#include "gearhead/scene3ds.h"

namespace Gearhead {

class Renderer3D;
class Sound;
class Widget;

/**
 * First-person rail camera of the car lot and the junkyard (Walkabout,
 * 0x48 bytes, main_video §3.8): the camera slides between the nodes play_01
 * and play_02 at 500 units/s, bobs while walking and plays a footstep.
 */
class Walkabout {
public:
	Walkabout();

	/** Walkabout_init (0x4633c1) + Walkabout_loadAssets (0x46339d) */
	void init(Scene3DS *scene, Widget *listener);
	/** Walkabout_onKey (0x463c72) */
	void onKey(int scanCode);
	/** Walkabout_move (0x46373e) */
	void move(const Vec3 &dir);
	/** Walkabout_updateBob (0x46392c), every frame */
	void updateBob();
	/**
	 * Walkabout_render (0x463be7): the scene's meshes plus @p extra objects,
	 * lit by the sun and the scene ambient. The viewport must already be set up.
	 */
	void render(Renderer3D &r3d, const Common::Array<SceneObject> *extra = nullptr);
	/** A pick ray through a point of the viewport @p view */
	void pickRay(const Common::Rect &view, const Common::Point &p, Vec3 &origin, Vec3 &dir) const;
	/** Screen position of a world point in the viewport @p view (false when behind) */
	bool project(const Common::Rect &view, const Vec3 &p, Common::Point &out) const;

	Vec3 right() const { return _camera.row(0); }
	Vec3 forward() const { return _camera.row(2); }

	Scene3DS *_scene;
	Mat4 _camera;        ///< camera world matrix (rows right, up, forward, position)
	float _zoom;
	float _far;
	float _eyeHeight;    ///< +0x10 (100)
	float _bob;          ///< +0x14 (8)
	bool _moving;        ///< +0x18
	Widget *_listener;   ///< +0x1c, told when the view must be redrawn
	Vec3 _floor;         ///< +0x20
	Vec3 _railA, _railB; ///< +0x2c / +0x38
	Vec3 _ambient;       ///< scene ambient (0.6)

private:
	Vec3 cameraPos() const { return _camera.row(3); }
	void setCameraPos(const Vec3 &p) { _camera.setRow(3, p, 1.0f); }
	Sound *_footstep;
};

} // End of namespace Gearhead

#endif // GEARHEAD_WALKABOUT_H
