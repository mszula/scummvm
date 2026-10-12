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
#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/carview.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

static const float kPitchMin = 0.3926991f;  // pi/8
static const float kPitchMax = 2.7488937f;  // 7pi/8
static const float kZoom = 2.41421f;        // 1/tan(pi/8): 45 degree vertical FOV

CarView::CarView() : _mouseOver(false), _inputDisabled(false), _car(nullptr), _angles(1.5707964f, 0, 0),
	_anglesD(1.5707964f, 0, 0), _dist(18.0f), _distD(18.0f), _focus(-1), _assembled(false), _assembledColour(0),
	_useKeys(nullptr) {
	setRect(5, 85, 385, 342);
	_assem[0] = _assem[1] = _assem[2] = nullptr;
}

void CarView::clampPitch() {
	if (_anglesD.x <= kPitchMin)
		_anglesD.x = kPitchMin;
	if (_anglesD.x >= kPitchMax)
		_anglesD.x = kPitchMax;
}

void CarView::onArrowKey(int scanCode, float frameDelta) {
	const float d = frameDelta * 5.0f;
	switch (scanCode) {
	case kScanLeft:
		_anglesD.y -= d;
		break;
	case kScanRight:
		_anglesD.y += d;
		break;
	case kScanUp:
		_anglesD.x -= d;
		break;
	case kScanDown:
		_anglesD.x += d;
		break;
	default:
		return;
	}
	clampPitch();
	autoFrame();
}

// ---------------------------------------------------------------- events (CarView_on* 0x40a550..0x40a765)

void CarView::onHover(const Common::Point &p) {
	if (!_car || _inputDisabled)
		return;
	_mouseOver = true;
	WorkShopScreen *ws = g_engine->_workShop;
	ws->showToolCursor(true);
	if (ws->tool())
		ws->tool()->onHover(p);
}

void CarView::onLUp(const Common::Point &p) {
	if (!_inputDisabled && g_engine->_workShop->tool())
		g_engine->_workShop->tool()->onLeftUp(p);
}

void CarView::onLHeld(const Common::Point &p) {
	if (!_inputDisabled && g_engine->_workShop->tool())
		g_engine->_workShop->tool()->onLeftHeld(p);
}

void CarView::onLDown(const Common::Point &p) {
	if (!_inputDisabled && g_engine->_workShop->tool())
		g_engine->_workShop->tool()->onLeftDown(p);
}

void CarView::onRDown(const Common::Point &p) {
	if (!_inputDisabled && g_engine->_workShop->tool())
		g_engine->_workShop->tool()->onRightDown(p);
}

void CarView::onRHeld(const Common::Point &p) {
	// CarView_onRHeld (0x40a668): orbit by the mouse movement since the last frame
	const Common::Point &prev = g_engine->_input->prevMouse();
	if (_inputDisabled || p == prev)
		return;
	_anglesD.y -= (p.x - prev.x) * 0.03f;
	_anglesD.x -= (p.y - prev.y) * 0.03f;
	clampPitch();
	autoFrame();
	setDirty(true);
}

// ---------------------------------------------------------------- camera

void CarView::updateAssemblyLight() {
	// CarView_updateAssemblyLight (0x40c213)
	_assembled = false;
	const int region = g_engine->_workShop->_region;
	if (_car->isComplete(region)) {
		_assembled = true;
		_assembledColour = CLIP((int)((double)_car->minCondition(region) - 0.5), 0, 2);
	}
	if (!_car->_isTop && _car->isComplete(0) && _car->minCondition(0) == 3) {
		_car->_isTop = true;
		g_engine->_mechanic->msgCarComplete();
	}
}

void CarView::autoFrame() {
	if (!_car)
		return;
	updateAssemblyLight();
	if (_focus >= 0)
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

void CarView::focusPart(const Part *part) {
	// CarView_focusPart (0x40bab3)
	if (!part || !_car)
		return;
	const int mi = part->def->meshIndex;
	if (mi < 0 || !_car->model()->_meshes[mi])
		return;
	g_engine->_snd->play("misc\\zoom1.wav");
	const Mesh3D *m = _car->model()->_meshes[mi];
	_focus = mi;
	Vec3 t = m->local.row(3);
	int n = 1;
	for (uint i = 0; i < part->def->bolts.size(); i++) {
		t += part->def->bolts[i];
		n++;
	}
	_targetD = t * (1.0f / n);
	float maxSq = -1.0f;
	for (uint i = 0; i < m->vertices.size(); i++) {
		const float d = (m->local.transformPoint(m->vertices[i].pos) - _targetD).lengthSq();
		if (d > maxSq)
			maxSq = d;
	}
	_distD = sqrtf(MAX(0.0f, maxSq)) / tanf((float)M_PI / 8.0f);
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

// ---------------------------------------------------------------- picking

void CarView::pickRay(const Common::Point &p, Vec3 &origin, Vec3 &dir) const {
	// Ray_fromScreen: through the pixel, relative to the view rect
	const float w = (float)_rect.width(), h = (float)_rect.height();
	const float nx = ((p.x - _rect.left) + 0.5f) / w * 2.0f - 1.0f;
	const float ny = 1.0f - ((p.y - _rect.top) + 0.5f) / h * 2.0f;
	const Vec3 d(nx * (w / h) / kZoom, ny / kZoom, 1.0f);
	origin = _cameraWorld.row(3);
	dir = _cameraWorld.transformDir(d).normalized();
}

bool CarView::project(const Vec3 &p, Common::Point &out) const {
	const Vec3 v = _cameraWorld.inverse().transformPoint(p);
	if (v.z <= 0.0f)
		return false;
	const float w = (float)_rect.width(), h = (float)_rect.height();
	const float nx = v.x * kZoom / ((w / h) * v.z), ny = v.y * kZoom / v.z;
	out = Common::Point(_rect.left + (int)((nx + 1.0f) * 0.5f * w), _rect.top + (int)((1.0f - ny) * 0.5f * h));
	return true;
}

static bool rayTriangle(const Vec3 &o, const Vec3 &d, const Vec3 &a, const Vec3 &b, const Vec3 &c, float &t) {
	const Vec3 e1 = b - a, e2 = c - a;
	const Vec3 pv = d.cross(e2);
	const float det = e1.dot(pv);
	if (fabsf(det) < 1e-9f)
		return false;
	const float inv = 1.0f / det;
	const Vec3 tv = o - a;
	const float u = tv.dot(pv) * inv;
	if (u < 0.0f || u > 1.0f)
		return false;
	const Vec3 qv = tv.cross(e1);
	const float v = d.dot(qv) * inv;
	if (v < 0.0f || u + v > 1.0f)
		return false;
	t = e2.dot(qv) * inv;
	return t > 0.0f;
}

PickHit CarView::pick(const Vec3 &origin, const Vec3 &dir, bool withExtras) const {
	PickHit best;
	if (!_car)
		return best;
	bool found = false;
	const Common::Array<Mesh3D *> &meshes = _car->model()->_meshes;
	for (uint i = 0; i < meshes.size(); i++) {
		const Mesh3D *m = meshes[i];
		if (!m || i >= _car->_meshVisible.size() || !_car->_meshVisible[i])
			continue;
		for (uint k = 0; k < m->polys.size(); k++) {
			const MeshPoly &poly = m->polys[k];
			float t;
			if (rayTriangle(origin, dir, m->local.transformPoint(m->vertices[poly.v[0]].pos),
			                m->local.transformPoint(m->vertices[poly.v[1]].pos),
			                m->local.transformPoint(m->vertices[poly.v[2]].pos), t) &&
			    (!found || t < best.t)) {
				best.mesh = i;
				best.extra = -1;
				best.t = t;
				found = true;
			}
		}
	}
	if (withExtras) {
		float t;
		const int bolt = g_engine->_workShop->_airRatchet.pickBoltCube(origin, dir, t);
		if (bolt >= 0 && (!found || t < best.t)) {
			best.mesh = -1;
			best.extra = bolt;
			best.t = t;
		}
	}
	return best;
}

// ---------------------------------------------------------------- drawing

void CarView::render() {
	Renderer3D *r3d = g_engine->_renderer3d;
	Gfx *gfx = g_engine->_gfx;
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
	// Bolt cubes and arrows live in the same scene
	g_engine->_workShop->_airRatchet.render3D(*r3d);
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

	// Overlays (screen sprites, additive): "Use arrow keys to rotate view" and the ASSEMBLED light
	if (!_useKeys) {
		Resources *res = g_engine->_res;
		_useKeys = res->getImage("Overlays\\UseKeys.tga");
		_assem[0] = res->getImage("Overlays\\AssemR.tga");
		_assem[1] = res->getImage("Overlays\\AssemY.tga");
		_assem[2] = res->getImage("Overlays\\AssemG.tga");
	}
	gfx->blitAdditive(kPageFront, *_useKeys, _rect.left, _rect.top);
	if (_assembled) {
		const Graphics::ManagedSurface *a = _assem[_assembledColour];
		gfx->blitAdditive(kPageFront, *a, _rect.right - a->w, _rect.top);
	}
	debugC(3, kDebugGraphics, "CarView: %d triangles", r3d->trianglesDrawn());
}

void CarView::draw() {
	Gfx *gfx = g_engine->_gfx;
	if (!_car) {
		// CarView_clearRect + the empty workshop message (not while a job request is pending)
		gfx->fill(kPageFront, _rect, gfx->rgb(0.3f, 0.29f, 0.3f));
		if (!g_engine->_mechanic || !g_engine->_mechanic->_autoJobRequest) {
			Common::Rect box(_rect.left + 16, _rect.top + 16 + 64, _rect.right - 16, _rect.bottom - 16 - 64);
			gfx->fill(kPageFront, box, 0);
			g_engine->_fonts[kFontStandard].drawText(*gfx,
				"Your Workshop is empty. Get a car from the Car Lot or buy one from the Auction.",
				box, kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
		}
		return;
	}
	render();
	setDirty(false);
	// Keep redrawing while the camera converges
	if (fabsf(_distD - _dist) > 0.01f || (_targetD - _target).lengthSq() > 0.01f || (_anglesD - _angles).lengthSq() > 0.01f)
		setDirty(true);
	if (isDirty())
		updateCamera();
}

} // End of namespace Gearhead
