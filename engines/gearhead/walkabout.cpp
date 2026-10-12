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

#include "common/textconsole.h"

#include "gearhead/gearhead.h"
#include "gearhead/input.h"
#include "gearhead/renderer3d.h"
#include "gearhead/sound.h"
#include "gearhead/ui.h"
#include "gearhead/walkabout.h"

namespace Gearhead {

// The bob state is global in the original (g_WalkBobPhase, g_WalkBobPrev), shared by every walkabout
static float g_bobPhase = 0.0f;
static float g_bobPrev = 0.0f;

Walkabout::Walkabout() : _scene(nullptr), _zoom(1.0f), _far(10000.0f), _eyeHeight(100.0f), _bob(8.0f),
	_moving(false), _listener(nullptr), _ambient(0.6f, 0.6f, 0.6f), _footstep(nullptr) {
}

void Walkabout::init(Scene3DS *scene, Widget *listener) {
	_scene = scene;
	_listener = listener;
	_footstep = g_engine->_snd->get("misc\\footstep1.wav");
	_ambient = Vec3(0.6f, 0.6f, 0.6f);
	// The file's first camera, without its target, far 10000
	_zoom = scene->_camera.valid ? scene->_camera.zoom : 1.0f;
	_far = 10000.0f;
	const SceneNode *floor = scene->findNode("Floor");
	if (!scene->findMesh("Floor") || !floor)
		error("No floor mesh found in %s", scene->_name.c_str());
	_floor = floor->pos;
	const SceneNode *a = scene->findNode("play_01");
	const SceneNode *b = scene->findNode("play_02");
	_railA = a ? a->pos : Vec3();
	_railB = b ? b->pos : Vec3();
	// The rail and focus markers are unlinked from the scene
	if (scene->findMesh("play_01"))
		scene->findMesh("play_01")->visible = false;
	if (scene->findMesh("play_02"))
		scene->findMesh("play_02")->visible = false;
	const SceneNode *focus = scene->findNode("CamFocus");
	if (!focus)
		error("Couldn't find CamFocus Mesh in %s", scene->_name.c_str());
	if (scene->findMesh("CamFocus"))
		scene->findMesh("CamFocus")->visible = false;
	// Node_lookAtPoint from rail point A
	const Vec3 eye = _railA;
	Vec3 fwd = (focus->pos - eye).normalized();
	if (fwd.lengthSq() == 0.0f)
		fwd = Vec3(1, 0, 0);
	const Vec3 rgt = Vec3(0, 1, 0).cross(fwd).normalized();
	const Vec3 up = fwd.cross(rgt).normalized();
	_camera = Mat4::fromAxes(rgt, up, fwd, eye);
}

void Walkabout::onKey(int scanCode) {
	switch (scanCode) {
	case kScanUp:
		move(forward());
		break;
	case kScanLeft:
		move(right() * -1.0f);
		break;
	case kScanRight:
		move(right());
		break;
	case kScanDown:
		move(forward() * -1.0f);
		break;
	default:
		break;
	}
}

void Walkabout::move(const Vec3 &dir) {
	if (!_listener)
		return;
	_moving = true;
	// Project onto the rail and move at 500 units per second
	const Vec3 d = _railA - _railB;
	const float t = dir.dot(d) / d.dot(d);
	const Vec3 v = (d * t).normalized();
	setCameraPos(cameraPos() + v * (g_engine->frameDelta() * 500.0f));
	_listener->setDirty(true);
	const float len = (_railA - _railB).length();
	if ((cameraPos() - _railA).length() < len) {
		if ((cameraPos() - _railB).length() >= len) {
			setCameraPos(_railA);
			_moving = false;
			_listener->setDirty(false);
		}
	} else {
		setCameraPos(_railB);
		_moving = false;
		_listener->setDirty(false);
	}
}

void Walkabout::updateBob() {
	if (!_listener)
		return;
	const float rate = g_engine->frameDelta() * 15.0f;
	const float pi = 3.1415927f, twoPi = 6.2831855f;
	if (!_moving) {
		// Ease back to the nearest of 0 and pi
		while (g_bobPhase > twoPi)
			g_bobPhase -= twoPi;
		while (g_bobPhase < -twoPi)
			g_bobPhase += twoPi;
		if (fabsf(g_bobPhase - pi) > 0.0001f && fabsf(g_bobPhase) > 0.0001f) {
			if (g_bobPhase < 1.5707964f) {
				g_bobPhase = MAX(0.0f, g_bobPhase - rate);
			} else if (g_bobPhase < pi) {
				g_bobPhase = MIN(pi, g_bobPhase + rate);
			} else if (g_bobPhase < 4.712389f) {
				g_bobPhase = MAX(pi, g_bobPhase - rate);
			} else {
				g_bobPhase += rate;
				if (g_bobPhase > twoPi)
					g_bobPhase = 0.0f;
			}
		}
	} else {
		g_bobPhase += rate;
	}
	const float prevY = cameraPos().y;
	float bob = sinf(g_bobPhase) * _bob;
	if (bob < 0.0f)
		bob *= 0.5f;
	if (g_bobPrev > 0.0f && g_bobPrev * bob < 0.0f)
		_footstep->play();
	g_bobPrev = bob;
	Vec3 p = cameraPos();
	p.y = _floor.y + _eyeHeight + bob;
	setCameraPos(p);
	if (fabsf(prevY - p.y) > 0.1f)
		_listener->setDirty(true);
	_moving = false;
}

void Walkabout::render(Renderer3D &r3d, const Common::Array<SceneObject> *extra) {
	r3d.setCamera(_camera, _zoom, 1.0f, _far);
	r3d.setAmbient(_ambient);
	// The sun: directional, colour (1, 1, 0.8), Euler (0, -45, 0) then (0, 0, 45) degrees
	Light3D sun;
	sun.type = Light3D::kDirectional;
	sun.colour = Vec3(1.0f, 1.0f, 0.8f);
	sun.direction = Vec3(-0.70710677f, 0.0f, 0.70710677f);
	const Mat4 identity;
	for (uint i = 0; i < _scene->_meshes.size(); i++) {
		const SceneMesh *m = _scene->_meshes[i];
		if (!m->visible)
			continue;
		r3d.clearLights();
		if (m->lit)
			r3d.addLight(sun);
		r3d.drawMesh(m->mesh, identity, m->materials);
	}
	if (extra) {
		for (uint i = 0; i < extra->size(); i++) {
			const SceneObject &o = (*extra)[i];
			if (!o.visible || !o.mesh || !o.materials)
				continue;
			r3d.clearLights();
			if (o.lit)
				r3d.addLight(sun);
			r3d.drawMesh(*o.mesh, o.world, *o.materials);
		}
	}
	r3d.flushTranslucent();
}

void Walkabout::pickRay(const Common::Rect &view, const Common::Point &p, Vec3 &origin, Vec3 &dir) const {
	const float w = (float)view.width(), h = (float)view.height();
	const float nx = ((p.x - view.left) + 0.5f) / w * 2.0f - 1.0f;
	const float ny = 1.0f - ((p.y - view.top) + 0.5f) / h * 2.0f;
	origin = _camera.row(3);
	dir = _camera.transformDir(Vec3(nx * (w / h) / _zoom, ny / _zoom, 1.0f)).normalized();
}

bool Walkabout::project(const Common::Rect &view, const Vec3 &p, Common::Point &out) const {
	const Vec3 v = _camera.inverse().transformPoint(p);
	if (v.z <= 0.0f)
		return false;
	const float w = (float)view.width(), h = (float)view.height();
	const float nx = v.x * _zoom / ((w / h) * v.z), ny = v.y * _zoom / v.z;
	out = Common::Point(view.left + (int)((nx + 1.0f) * 0.5f * w), view.top + (int)((1.0f - ny) * 0.5f * h));
	return true;
}

} // End of namespace Gearhead
