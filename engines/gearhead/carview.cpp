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

#include "gearhead/car.h"
#include "gearhead/carview.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/renderer3d.h"

namespace Gearhead {

static const float kPitchMin = 0.3926991f;  // pi/8
static const float kPitchMax = 2.7488937f;  // 7pi/8
static const float kZoom = 2.41421f;        // 1/tan(pi/8): 45 degree vertical FOV

CarView::CarView() : _car(nullptr), _region(0), _angles(1.5707964f, 0, 0), _anglesD(1.5707964f, 0, 0),
	_dist(18.0f), _distD(18.0f), _haveLastMouse(false) {
	setRect(5, 85, 385, 342);
}

void CarView::setCar(CarInstance *car, int region) {
	_car = car;
	_region = region;
	if (_car)
		_car->showRegion(region, true);
	autoFrame();
	setDirty(true);
}

void CarView::clampPitch() {
	if (_anglesD.x <= kPitchMin)
		_anglesD.x = kPitchMin;
	if (_anglesD.x >= kPitchMax)
		_anglesD.x = kPitchMax;
}

void CarView::onArrowKey(int scanCode, float frameDelta) {
	switch (scanCode) {
	case kScanLeft:
		_anglesD.y -= 5.0f * frameDelta;
		break;
	case kScanRight:
		_anglesD.y += 5.0f * frameDelta;
		break;
	case kScanUp:
		_anglesD.x -= 5.0f * frameDelta;
		break;
	case kScanDown:
		_anglesD.x += 5.0f * frameDelta;
		break;
	default:
		return;
	}
	clampPitch();
	autoFrame();
	setDirty(true);
}

void CarView::onMouseMove(const Common::Point &p) {
	if (!_haveLastMouse) {
		_lastMouse = p;
		_haveLastMouse = true;
	}
}

void CarView::onRHeld(const Common::Point &p) {
	// CarView_onRHeld (0x40a668): orbit while the right button is held and the mouse moved
	if (_haveLastMouse && p != _lastMouse) {
		_anglesD.y -= (p.x - _lastMouse.x) * 0.03f;
		_anglesD.x -= (p.y - _lastMouse.y) * 0.03f;
		clampPitch();
		autoFrame();
		setDirty(true);
	}
	_lastMouse = p;
	_haveLastMouse = true;
}

void CarView::autoFrame() {
	if (!_car)
		return;

	// Bounding box of the visible car meshes
	bool any = false;
	Vec3 bmin, bmax;
	const Common::Array<Mesh3D *> &meshes = _car->model()->_meshes;
	for (uint i = 0; i < meshes.size(); i++) {
		if (!meshes[i] || i >= _car->_meshVisible.size() || !_car->_meshVisible[i] || meshes[i]->vertices.empty())
			continue;
		if (!any) {
			bmin = meshes[i]->boundsMin;
			bmax = meshes[i]->boundsMax;
			any = true;
		} else {
			bmin = Vec3(MIN(bmin.x, meshes[i]->boundsMin.x), MIN(bmin.y, meshes[i]->boundsMin.y), MIN(bmin.z, meshes[i]->boundsMin.z));
			bmax = Vec3(MAX(bmax.x, meshes[i]->boundsMax.x), MAX(bmax.y, meshes[i]->boundsMax.y), MAX(bmax.z, meshes[i]->boundsMax.z));
		}
	}
	if (!any) {
		updateCamera();
		return;
	}

	const float pD = _anglesD.x, yD = _anglesD.y;
	const Vec3 dir = Vec3(sinf(pD) * cosf(yD), -cosf(pD), sinf(pD) * sinf(yD)).normalized();
	const Vec3 side = Vec3(0, 1, 0).cross(dir).normalized();
	const Vec3 up = dir.cross(side).normalized();

	const Vec3 center = (bmin + bmax) * 0.5f;
	const Vec3 bottom(center.x, bmin.y, center.z);
	const float r = (bottom - bmin).length() * 0.85f;

	Vec3 samples[40];
	for (int i = 0; i < 20; i++) {
		const float t = i * 2.0f * (float)M_PI / 20.0f;
		samples[i] = Vec3(center.x + cosf(t) * r, bmin.y, center.z + sinf(t) * r);
		samples[i + 20] = Vec3(samples[i].x, bmax.y, samples[i].z);
	}
	Vec3 front = samples[0];
	for (int i = 1; i < 40; i++)
		if (dir.dot(samples[i]) < dir.dot(front))
			front = samples[i];
	front = front - dir * 100.0f;

	const float k = kZoom;
	const float kv = k * (float)_rect.width() / (float)_rect.height() * 0.8f;
	float a1 = 0, a2 = 0, b1 = 0, b2 = 0;
	for (int i = 0; i < 40; i++) {
		const Vec3 v = samples[i] - front;
		const float x = v.dot(side), y = v.dot(up), z = v.dot(dir);
		const float ta1 = z - k * x, ta2 = z + k * x, tb1 = z - kv * y, tb2 = z + kv * y;
		if (i == 0 || -ta1 / k > -a1 / k)
			a1 = ta1;
		if (i == 0 || ta2 / k < a2 / k)
			a2 = ta2;
		if (i == 0 || -tb1 / kv > -b1 / kv)
			b1 = tb1;
		if (i == 0 || tb2 / kv < b2 / kv)
			b2 = tb2;
	}
	const float cx = ((a1 - a2) * -0.5f) / k;
	float cz = k * cx + a1;
	const float cy = ((b1 - b2) * -0.5f) / kv;
	const float cz2 = kv * cy + b1;
	cz = MIN(cz, cz2);

	const Vec3 pos = side * cx + up * cy + dir * cz + front;
	_targetD = pos + dir;
	_distD = 1.0f;
	updateCamera();
	setDirty(true);
}

void CarView::updateCamera() {
	// CarView_updateCamera (0x40bd7e): ease 75 % towards the desired values
	_dist += (_distD - _dist) * 0.75f;
	_target += (_targetD - _target) * 0.75f;
	_angles += (_anglesD - _angles) * 0.75f;
	const float p = _angles.x, y = _angles.y;
	const Vec3 pos = _target + Vec3(-sinf(p) * cosf(y) * _dist, cosf(p) * _dist, -sinf(p) * sinf(y) * _dist);
	const Vec3 forward = (_target - pos).normalized();
	const Vec3 right = Vec3(0, 1, 0).cross(forward).normalized();
	const Vec3 up = forward.cross(right);
	_cameraWorld = Mat4::fromAxes(right, up, forward, pos);
}

void CarView::render() {
	Renderer3D *r3d = g_engine->_renderer3d;
	Gfx *gfx = g_engine->_gfx;
	if (!_car) {
		// CarView_draw: empty workshop message on a black box inside the view
		gfx->fill(kPageFront, _rect, gfx->rgb(0.3f, 0.29f, 0.3f));
		Common::Rect box(_rect.left + 16, _rect.top + 16 + 64, _rect.right - 16, _rect.bottom - 16 - 64);
		gfx->fill(kPageFront, box, 0);
		g_engine->_fonts[kFontStandard].drawText(*gfx,
			"Your Workshop is empty. Get a car from the Car Lot or buy one from the Auction.",
			box, kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
		return;
	}
	if (!r3d)
		return;

	r3d->beginFrame(_rect, Vec3(0.3f, 0.29f, 0.3f));
	r3d->setCamera(_cameraWorld, kZoom, 1.0f, 1000.0f);
	r3d->setAmbient(Vec3(0.3f, 0.3f, 0.3f));
	r3d->clearLights();
	// Point light attached to the camera at (-400, 1000, -600), range 3000, intensity 2
	Light3D light;
	light.type = Light3D::kPoint;
	light.position = _cameraWorld.transformPoint(Vec3(-400.0f, 1000.0f, -600.0f));
	light.colour = Vec3(1, 1, 1);
	light.k1 = 2.0f;
	light.k2 = 2.0f / 3000.0f;
	r3d->addLight(light);

	const CarModel *model = _car->model();
	for (uint i = 0; i < model->_meshes.size(); i++) {
		if (model->_meshes[i] && i < _car->_meshVisible.size() && _car->_meshVisible[i])
			r3d->drawMesh(*model->_meshes[i], model->_meshes[i]->local, _car->_polyMaterials[i]);
	}
	r3d->flushTranslucent();

	// Second pass (16-bit modes): CD_SPECMESH with the fake specular gloss
	// (CarGloss_mesh 0x45fb38), camera B = camera A moved one unit forward,
	// ambient 0 and no lights, additive "SpecMat"
	Mat4 camB = _cameraWorld;
	camB.setRow(3, _cameraWorld.row(3) + _cameraWorld.row(2), 1.0f);
	r3d->setCamera(camB, kZoom, 1.0f, 1000.0f);
	r3d->setAmbient(Vec3(0, 0, 0));
	r3d->clearLights();
	const Vec3 glossLight = _cameraWorld.transformPoint(Vec3(-400.0f, 1000.0f, -600.0f));
	Common::Array<const Material3D *> mats;
	Common::Array<Vec3> gloss;
	for (uint i = 0; i < model->_specMeshes.size(); i++) {
		const Mesh3D *m = model->_specMeshes[i];
		if (!m || i >= _car->_specVisible.size() || !_car->_specVisible[i])
			continue;
		const Vec3 lObj = m->local.inverse().transformPoint(glossLight);
		gloss.resize(m->vertices.size());
		for (uint v = 0; v < m->vertices.size(); v++) {
			const Vec3 L = lObj - m->vertices[v].pos;
			const float len = L.length();
			float g = len > 0.0f ? m->vertices[v].normal.dot(L) / len : 0.0f;
			g = MAX(0.0f, g);
			float value = powf(g, 15.0f) * 0.55f;
			value = MIN(value, 0.99f);
			gloss[v] = Vec3(value, value, value);
		}
		mats.resize(m->polys.size());
		for (uint k = 0; k < m->polys.size(); k++) {
			const int fm = m->polys[k].fileMaterial;
			mats[k] = (fm >= 0 && fm < (int)model->_materials.size()) ? model->_materials[fm] : nullptr;
		}
		r3d->drawMesh(*m, m->local, mats, &gloss);
	}

	r3d->endFrame(*gfx);
	debugC(3, kDebugGraphics, "CarView: %d triangles", r3d->trianglesDrawn());
}

void CarView::update() {
	if (!isDirty())
		return;
	updateCamera();
	render();
	setDirty(false);
	// Keep redrawing while the camera converges
	if (fabsf(_distD - _dist) > 0.01f || (_targetD - _target).lengthSq() > 0.01f || (_anglesD - _angles).lengthSq() > 0.01f)
		setDirty(true);
}

} // End of namespace Gearhead
