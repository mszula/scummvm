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

#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobs.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

// Bolt states are bolt colours
#define kBoltIn Vec3(1.0f, 1.0f, 1.0f)       // screwed in
#define kBoltOut Vec3(0.0f, 0.0f, 0.0f)      // removed
#define kBoltHover Vec3(1.0f, 0.75f, 0.0f)   // under the pointer

static bool sameColour(const Vec3 &a, const Vec3 &b) {
	// Vec3f_distance (0x414760) < 0.001
	return (a - b).length() < 0.001f;
}

// ---------------------------------------------------------------- AirRatchetDialog

AirRatchetDialog::AirRatchetDialog(AirRatchet *owner) : _owner(owner) {
	_cancel.setOnClick(makeCallback(this, &AirRatchetDialog::onCancel));
	_title = "Air Ratchet Tool";
}

void AirRatchetDialog::init() {
	Resources *res = g_engine->_res;
	setFrame("Overlays\\Dialog\\Frame.tga");
	_closeSound = false;
	setPos(390, 95);
	_cancel.setImages(res->getImage("Overlays\\Dialog\\CancelUP.tga"), res->getImage("Overlays\\Dialog\\CancelDN.tga"),
	                  res->getImage("Overlays\\Dialog\\CancelUP.tga"));
	_cancel.setPos(_anchorCenter.x, _anchorCenter.y);
	// Dialog_setButtons(dialog, &g_WorkShopBackdrop, &workshop->carView, 0): the bolts are
	// clicked in the car view while the dialog is modal
	WorkShopScreen *ws = g_engine->_workShop;
	Common::Array<Widget *> own, extra;
	own.push_back(&_cancel);
	extra.push_back(&ws->_backdrop);
	extra.push_back(&ws->_carView);
	setButtons(own, extra);
}

void AirRatchetDialog::onCancel(Button *b) {
	_owner->cancel();
}

// ---------------------------------------------------------------- AirRatchet

AirRatchet::AirRatchet() : _dialog(this), _mode(kModeIdle), _hovered(nullptr), _prevHovered(nullptr), _remaining(0),
	_arrowCount(0), _tighten(nullptr), _loosen(nullptr), _attachStart(nullptr), _removeStart(nullptr), _miss(nullptr) {
}

AirRatchet::~AirRatchet() {
}

/** Mesh_createCube (edge 2), flat-shaded */
static void buildCube(Mesh3D &m, float edge) {
	const float h = edge * 0.5f;
	static const int kFaces[6][4] = {
		{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 }
	};
	m.name = "Bolt";
	m.vertices.resize(8);
	static const float kCorners[8][3] = {
		{ -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
	};
	for (int i = 0; i < 8; i++) {
		m.vertices[i].pos = Vec3(kCorners[i][0] * h, kCorners[i][1] * h, kCorners[i][2] * h);
		m.vertices[i].normal = m.vertices[i].pos.normalized();
	}
	for (int f = 0; f < 6; f++) {
		for (int t = 0; t < 2; t++) {
			MeshPoly p;
			p.v[0] = kFaces[f][0];
			p.v[1] = kFaces[f][1 + t];
			p.v[2] = kFaces[f][2 + t];
			for (int k = 0; k < 3; k++)
				p.uv[k][0] = p.uv[k][1] = 0.0f;
			p.fileMaterial = 0;
			const Vec3 &a = m.vertices[p.v[0]].pos, &b = m.vertices[p.v[1]].pos, &c = m.vertices[p.v[2]].pos;
			p.normal = (b - a).cross(c - a).normalized();
			// Keep the faces pointing outwards
			if (p.normal.dot(a) < 0.0f) {
				SWAP(p.v[1], p.v[2]);
				p.normal = p.normal * -1.0f;
			}
			m.polys.push_back(p);
		}
	}
	m.boundsMin = Vec3(-h, -h, -h);
	m.boundsMax = Vec3(h, h, h);
}

void AirRatchet::init() {
	setCursor("Pointers\\Wrench.tga");
	_dialog.init();

	// The "BOLT" arrows: flat, material alpha, additive
	const Graphics::ManagedSurface *img = g_engine->_res->getImage("Overlays\\BoltArrow.tga");
	_arrowTexture.name = "Bolt Arrow";
	_arrowTexture.flags = 9;
	_arrowTexture.create(MIN<int>(kMaxTextureSize, img->w));
	for (int y = 0; y < _arrowTexture.size; y++)
		for (int x = 0; x < _arrowTexture.size; x++) {
			byte r, g, b;
			img->format.colorToRGB(img->getPixel(x * img->w / _arrowTexture.size, y * img->h / _arrowTexture.size), r, g, b);
			_arrowTexture.setPixel(x, y, r, g, b);
		}
	_arrowMaterial.texture = &_arrowTexture;
	_arrowMaterial.shadeColor = 1;
	_arrowMaterial.shadeAlpha = 1;
	_arrowMaterial.blendSrc = 1;
	_arrowMaterial.blendDst = 1;
	_arrowMaterial.updateTranslucency();
	_arrowMaterial.flags |= Material3D::kFlagTwoSided;

	buildCube(_cube, 2.0f);
	for (int i = 0; i < 16; i++) {
		_bolts[i].material.shadeColor = 1;
		_bolts[i].visible = false;
	}
	_arrowCount = 0;

	SoundManager *snd = g_engine->_snd;
	_tighten = snd->get("ratchet\\ratchet23.wav");
	_loosen = snd->get("ratchet\\ratchet24.wav");
	_attachStart = snd->get("button\\button2.wav");
	_removeStart = snd->get("misc\\magnet3.wav");
	_miss = snd->get("air\\air7.wav");
	g_engine->_workShop->setTool(this);
}

// ---------------------------------------------------------------- picking

Part *AirRatchet::pickPart(const Common::Point &p) const {
	// Pick_partUnderPointer (0x45bb38): the nearest object; fixed body meshes hide what is behind
	const CarView &view = g_engine->_workShop->_carView;
	CarInstance *car = g_engine->_mechanic->_car;
	if (!car)
		return nullptr;
	Vec3 o, d;
	view.pickRay(p, o, d);
	const PickHit hit = view.pick(o, d, false);
	if (hit.mesh < 0)
		return nullptr;
	const Mesh3D *m = car->model()->_meshes[hit.mesh];
	if (!m || m->isShell())
		return nullptr;
	for (uint i = 0; i < car->_parts.size(); i++)
		if (car->_parts[i]->def->meshIndex == hit.mesh)
			return car->_parts[i];
	return nullptr;
}

int AirRatchet::pickBoltCube(const Vec3 &origin, const Vec3 &dir, float &tBest) const {
	int best = -1;
	for (int i = 0; i < 16; i++) {
		if (!_bolts[i].visible)
			continue;
		// Slab test against the cube (edge 2) at the bolt position
		const Vec3 lo = _bolts[i].pos - Vec3(1, 1, 1), hi = _bolts[i].pos + Vec3(1, 1, 1);
		float t0 = -1e30f, t1 = 1e30f;
		const float o[3] = { origin.x, origin.y, origin.z }, dd[3] = { dir.x, dir.y, dir.z };
		const float l[3] = { lo.x, lo.y, lo.z }, h[3] = { hi.x, hi.y, hi.z };
		bool miss = false;
		for (int a = 0; a < 3 && !miss; a++) {
			if (fabsf(dd[a]) < 1e-9f) {
				if (o[a] < l[a] || o[a] > h[a])
					miss = true;
				continue;
			}
			float ta = (l[a] - o[a]) / dd[a], tb = (h[a] - o[a]) / dd[a];
			if (ta > tb)
				SWAP(ta, tb);
			t0 = MAX(t0, ta);
			t1 = MIN(t1, tb);
			if (t0 > t1)
				miss = true;
		}
		if (miss || t1 < 0.0f)
			continue;
		const float t = t0 > 0.0f ? t0 : t1;
		if (best < 0 || t < tBest) {
			best = i;
			tBest = t;
		}
	}
	return best;
}

int AirRatchet::pickBolt(const Common::Point &p) const {
	const CarView &view = g_engine->_workShop->_carView;
	Vec3 o, d;
	view.pickRay(p, o, d);
	const float radius = sqrtf(3.0f) * 1.5f; // cube bounding radius x 1.5
	for (int i = 0; i < 16; i++) {
		if (!_bolts[i].visible)
			continue;
		// Ray against the enlarged bounding sphere...
		const Vec3 oc = o - _bolts[i].pos;
		const float b = oc.dot(d);
		const float c = oc.dot(oc) - radius * radius;
		if (b * b - c < 0.0f)
			continue;
		// ...and the bolt must be the nearest thing in the scene along the ray
		const PickHit hit = view.pick(o, d, true);
		if (hit.extra == i)
			return i;
	}
	return -1;
}

int AirRatchet::findBoltUnderCursor(const Common::Point &p) const {
	const int i = pickBolt(p);
	if (i < 0)
		return -1;
	const Vec3 &c = _bolts[i].material.color;
	if (sameColour(c, kBoltHover))
		return i;
	const bool in = sameColour(c, kBoltIn);
	if (_mode == kModeAttaching && in)
		return -1;
	if (_mode != kModeRemoving)
		return i;
	return in ? i : -1;
}

// ---------------------------------------------------------------- bolts

void AirRatchet::setupBolts(const Part *part) {
	for (int i = 0; i < 16; i++)
		_bolts[i].visible = false;
	int n = 0;
	for (uint i = 0; i < part->def->bolts.size() && n < 16; i++, n++) {
		Bolt &b = _bolts[n];
		b.visible = true;
		b.pos = part->def->bolts[i];
		b.material.color = (_mode == kModeRemoving) ? kBoltIn : kBoltOut;
		b.arrowSize = 15.0f;
	}
	_remaining = n;
	_arrowCount = n;
}

void AirRatchet::hideBolts() {
	for (int i = 0; i < 16; i++)
		_bolts[i].visible = false;
	_arrowCount = 0;
}

void AirRatchet::render3D(Renderer3D &r3d) const {
	Common::Array<const Material3D *> mats;
	mats.resize(_cube.polys.size());
	for (int i = 0; i < 16; i++) {
		if (!_bolts[i].visible)
			continue;
		for (uint k = 0; k < mats.size(); k++)
			mats[k] = &_bolts[i].material;
		Mat4 world;
		world.setRow(3, _bolts[i].pos, 1.0f);
		r3d.drawMesh(_cube, world, mats);
	}
	for (int i = 0; i < _arrowCount && i < 16; i++)
		if (_bolts[i].arrowSize > 0.0f)
			r3d.drawBillboard(_bolts[i].pos, _bolts[i].arrowSize, 100.0f, &_arrowMaterial);
}

// ---------------------------------------------------------------- events

void AirRatchet::onHoverOverview(const Common::Point &p) {
	Mechanic *m = g_engine->_mechanic;
	CarInstance *car = m->_car;
	WorkShopScreen *ws = g_engine->_workShop;
	_hovered = nullptr;
	if (!(g_engine->_input->buttons() & 2))
		_hovered = pickPart(p);
	if (_hovered && (!_prevHovered || _hovered->def->region != _prevHovered->def->region)) {
		// A region locked by the job is not highlighted
		if (m->_job && !m->_job->legalModes[CLIP(_hovered->def->region, 0, 3)])
			return;
		ws->_carView.setDirty(true);
		for (uint i = 0; i < car->_parts.size(); i++)
			if (car->_parts[i]->def->region == _hovered->def->region)
				car->setPartMaterial(car->_parts[i], conditionHighlight(car->_parts[i]->condition));
	}
	if (_prevHovered && (!_hovered || _prevHovered->def->region != _hovered->def->region)) {
		ws->_carView.setDirty(true);
		for (uint i = 0; i < car->_parts.size(); i++)
			if (car->_parts[i]->def->region == _prevHovered->def->region)
				car->applyPartMaterials(car->_parts[i]);
		_prevHovered = nullptr;
	}
	_prevHovered = _hovered;
}

void AirRatchet::onHover(const Common::Point &p) {
	// AirRatchet_onHover (0x41ba18)
	Mechanic *m = g_engine->_mechanic;
	CarInstance *car = m->_car;
	WorkShopScreen *ws = g_engine->_workShop;
	if (!car)
		return;
	if (ws->_region == 0) {
		onHoverOverview(p);
		return;
	}
	ws->_carView.setDirty(true);
	if (_mode == kModeIdle) {
		_hovered = nullptr;
		if (!(g_engine->_input->buttons() & 2))
			_hovered = pickPart(p);
		if (_hovered && _hovered != _prevHovered) {
			car->setPartMaterial(_hovered, conditionHighlight(_hovered->condition));
			ws->drawPartInfo(_hovered->def->name);
			ws->_bin._list.selectByPartId(_hovered->def->id);
		}
		if (!_hovered && _prevHovered)
			ws->clearPartInfo();
		if (_prevHovered && _prevHovered != _hovered) {
			car->applyPartMaterials(_prevHovered);
			ws->_bin._list.clearSelection();
			_prevHovered = nullptr;
		}
		_prevHovered = _hovered;
		return;
	}
	// Bolt modes: the bolt under the pointer turns orange
	const Vec3 rest = (_mode == kModeRemoving) ? kBoltIn : kBoltOut;
	for (int i = 0; i < 16; i++)
		if (sameColour(_bolts[i].material.color, kBoltHover))
			_bolts[i].material.color = rest;
	const int b = pickBolt(p);
	if (b >= 0 && sameColour(_bolts[b].material.color, rest)) {
		_bolts[b].material.color = kBoltHover;
		ws->_carView.setDirty(true);
	}
}

void AirRatchet::onLeftUp(const Common::Point &p) {
	// AirRatchet_onLeftUp (0x41adcf): a click in the COMPLETE view opens the part's region
	if (g_engine->_workShop->_region == 0 && _hovered) {
		g_engine->_workShop->setMode(_hovered->def->region, true);
		_prevHovered = nullptr;
	}
}

void AirRatchet::onLeftDown(const Common::Point &p) {
	// AirRatchet_onLeftDown (0x41ae29)
	WorkShopScreen *ws = g_engine->_workShop;
	Mechanic *m = g_engine->_mechanic;
	if (ws->_region == 0)
		return;
	if (_mode == kModeIdle) {
		if (!_hovered)
			_miss->play();
		else
			beginRemove(_hovered);
		return;
	}
	const int i = findBoltUnderCursor(p);
	if (i < 0) {
		_miss->play();
	} else {
		Bolt &b = _bolts[i];
		const Vec3 target = (_mode == kModeRemoving) ? kBoltOut : kBoltIn;
		if (!sameColour(b.material.color, target)) {
			b.material.color = target;
			b.arrowSize = 0.0f;
			_remaining--;
		}
		ws->_carView.setDirty(true);
		(_mode == kModeRemoving ? _loosen : _tighten)->play();
	}
	if (_remaining == 0) {
		const int mode = _mode;
		_mode = kModeIdle;
		if (mode == kModeRemoving)
			m->removePart(_hovered);
		else
			m->attachPart(_hovered);
		_dialog.close();
		hideBolts();
		afterPartChange();
	}
}

void AirRatchet::afterPartChange() {
	WorkShopScreen *ws = g_engine->_workShop;
	g_engine->_mechanic->updateCurrentCar(ws->_region);
	ws->_bin.refill();
	ws->_carView.clearFocus();
	ws->_carView.autoFrame();
}

void AirRatchet::beginRemove(Part *part) {
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	if (!m->_car || !part)
		return;
	_hovered = part;
	m->_car->applyPartMaterials(part);
	AssemblyError err;
	if (!m->_car->canRemove(part, err)) {
		m->msgAssemblyError(err);
		return;
	}
	ws->_carView.setDirty(true);
	_mode = kModeRemoving;
	setupBolts(part);
	if (_remaining == 0) {
		m->removePart(part);
		afterPartChange();
		_mode = kModeIdle;
		return;
	}
	_removeStart->play();
	if (ws->_region != 1)
		ws->_carView.focusPart(part);
	_dialog.setTexts("Air Ratchet Tool", "Use the left mouse button to unscrew the bolts.");
	_dialog.open();
	ws->_carView.setDirty(true);
}

void AirRatchet::beginAttach(Part *part) {
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	if (!m->_car || !part)
		return;
	AssemblyError err;
	if (!m->_car->canInstall(part, err)) {
		m->msgAssemblyError(err);
		return;
	}
	_hovered = part;
	ws->_carView.setDirty(true);
	_mode = kModeAttaching;
	setupBolts(part);
	_attachStart->play();
	if (_remaining == 0) {
		m->attachPart(part);
		afterPartChange();
		_mode = kModeIdle;
		return;
	}
	if (ws->_region != 1)
		ws->_carView.focusPart(part);
	// Show the part's mesh (and gloss) where it will go
	CarInstance *car = m->_car;
	const Common::Array<Mesh3D *> &meshes = car->model()->_meshes;
	for (uint i = 0; i < meshes.size(); i++) {
		if (meshes[i] && part->def->meshName.equalsIgnoreCase(meshes[i]->name)) {
			car->applyPartMaterials(part);
			car->_meshVisible[i] = true;
			if (i < car->model()->_specMeshes.size() && car->model()->_specMeshes[i])
				car->_specVisible[i] = true;
			break;
		}
	}
	_dialog.setTexts("Air Ratchet Tool", "Use the left mouse button to attach the bolts.");
	_dialog.open();
	ws->_carView.setDirty(true);
}

void AirRatchet::cancel() {
	// AirRatchetDialog_onCancel (0x40ef6d): the bolts already done are undone, one sound each
	WorkShopScreen *ws = g_engine->_workShop;
	const int done = (int)(_hovered ? _hovered->def->bolts.size() : 0) - _remaining;
	Sound *s = (_mode == kModeRemoving) ? _tighten : (_mode == kModeAttaching ? _loosen : nullptr);
	for (int i = 0; s && i < done; i++) {
		s->play();
		while (s->isPlaying() && g_engine->pollOnly()) {
		}
	}
	ws->_carView.clearFocus();
	_dialog.close();
	g_engine->_mechanic->updateCurrentCar(ws->_region);
	hideBolts();
	ws->_carView.autoFrame();
	_mode = kModeIdle;
	ws->_carView.setDirty(true);
}

bool AirRatchet::debugBoltScreenPos(int i, Common::Point &out) const {
	const CarView &view = g_engine->_workShop->_carView;
	if (i < 0 || i >= 16 || !_bolts[i].visible)
		return false;
	Common::Point sp;
	if (!view.project(_bolts[i].pos, sp))
		return false;
	// Search a pixel near the projection that really picks this bolt
	for (int r = 0; r <= 6; r++)
		for (int dy = -r; dy <= r; dy++)
			for (int dx = -r; dx <= r; dx++)
				if (pickBolt(Common::Point(sp.x + dx, sp.y + dy)) == i) {
					out = Common::Point(sp.x + dx, sp.y + dy);
					return true;
				}
	out = sp;
	return false;
}

void AirRatchet::debugDumpBolts() const {
	for (int i = 0; i < 16; i++) {
		if (!_bolts[i].visible)
			continue;
		Common::Point sp;
		const bool ok = debugBoltScreenPos(i, sp);
		debug("dump bolt %d at %d %d%s", i, sp.x, sp.y, ok ? "" : " (hidden)");
	}
}

} // End of namespace Gearhead
