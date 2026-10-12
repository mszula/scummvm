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
#include "common/textconsole.h"
#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/carlot.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

// ---------------------------------------------------------------- LotPane

LotPane::LotPane() : _numSpots(0), _lastSlot(-1), _hovered(nullptr), _arrowX(99999), _arrow(nullptr) {
	setRect(83, 97, 548, 385);
}

LotPane::~LotPane() {
}

void LotPane::init() {
	Resources *res = g_engine->_res;
	// Sky: Clouds.jpg over the top 35 % of the view, behind everything
	Graphics::ManagedSurface *sky = res->loadImage("Data\\Scenes\\Clouds.jpg", nullptr);
	if (sky) {
		textureFromSurface(*sky, _skyTexture);
		delete sky;
	} else {
		_skyTexture.create(1);
	}
	_skyTexture.name = "Clouds.jpg";
	_skyMaterial.texture = &_skyTexture;
	_skyMaterial.shadeColor = 0;
	_arrow = res->getImage("Overlays\\CarArrow.tga");

	if (!_scene.load(*res, "Data\\Scenes\\", "CarLot.3ds"))
		error("error loading CarLot mesh");
	_walk.init(&_scene, this);
	// The spot markers car_01, car_02, ... are unlinked and remembered
	_numSpots = 0;
	for (int i = 1; _numSpots < kLotSpots; i++) {
		const Common::String name = Common::String::format("car_%s%d", i < 10 ? "0" : "", i);
		SceneMesh *m = _scene.findMesh(name);
		const SceneNode *n = _scene.findNode(name);
		if (!m || !n)
			break;
		m->visible = false;
		_spots[_numSpots++] = *n;
	}
	// The floor gets no light (its light mask is cleared)
	if (_scene.findMesh("Floor"))
		_scene.findMesh("Floor")->lit = false;
	// Shadows: black, 50 % alpha, flat, two-sided
	_shadowMaterial.shadeAlpha = 1;
	_shadowMaterial.shadeColor = 1;
	_shadowMaterial.blendSrc = 4;
	_shadowMaterial.blendDst = 5;
	_shadowMaterial.updateTranslucency();
	_shadowMaterial.flags |= Material3D::kFlagTwoSided;
	_shadowMaterial.color = Vec3(0, 0, 0);
	_shadowMaterial.alpha = 0.5f;
	for (int i = 0; i < kLotSpots; i++) {
		Mesh3D &m = _slots[i].shadow;
		m.vertices.resize(4);
		for (int v = 0; v < 4; v++)
			m.vertices[v].normal = Vec3(0, 1, 0);
		static const int kPolys[2][3] = { { 0, 3, 1 }, { 3, 2, 1 } };
		for (int p = 0; p < 2; p++) {
			MeshPoly poly;
			for (int k = 0; k < 3; k++) {
				poly.v[k] = kPolys[p][k];
				poly.uv[k][0] = poly.uv[k][1] = 0.0f;
			}
			poly.fileMaterial = -1;
			poly.normal = Vec3(0, 1, 0);
			m.polys.push_back(poly);
		}
		_slots[i].shadowMats.resize(2);
		_slots[i].shadowMats[0] = _slots[i].shadowMats[1] = &_shadowMaterial;
	}
}

void LotPane::clear() {
	for (int i = 0; i < kLotSpots; i++) {
		_slots[i].car = nullptr;
		_slots[i].objects.clear();
		_slots[i].materials.clear();
	}
	_hovered = nullptr;
}

void LotPane::addCar(CarInstance *car) {
	const int slot = ++_lastSlot;
	if (slot < 0 || slot >= _numSpots)
		return;
	Slot &s = _slots[slot];
	s.car = car;
	s.objects.clear();
	s.materials.clear();
	if (!car)
		return;
	// The car's visible meshes (COMPLETE view, no gloss) are cloned onto the spot
	car->showRegion(0, false);
	const CarModel *model = car->model();
	float minY = 9999999.0f;
	Vec3 bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
	for (uint i = 0; i < model->_meshes.size(); i++) {
		const Mesh3D *m = model->_meshes[i];
		if (!m || !car->_meshVisible[i])
			continue;
		for (uint v = 0; v < m->vertices.size(); v++) {
			const Vec3 w = m->local.transformPoint(m->vertices[v].pos);
			minY = MIN(minY, w.y);
			bmin = Vec3(MIN(bmin.x, w.x), MIN(bmin.y, w.y), MIN(bmin.z, w.z));
			bmax = Vec3(MAX(bmax.x, w.x), MAX(bmax.y, w.y), MAX(bmax.z, w.z));
		}
		s.materials.push_back(car->_polyMaterials[i]);
	}
	// Root: the spot's position and orientation, the lowest point on the floor
	const SceneNode &spot = _spots[slot];
	Mat4 root = spot.rotation;
	root.setRow(3, Vec3(spot.pos.x, _walk._floor.y - minY, spot.pos.z), 1.0f);
	uint k = 0;
	for (uint i = 0; i < model->_meshes.size(); i++) {
		const Mesh3D *m = model->_meshes[i];
		if (!m || !car->_meshVisible[i])
			continue;
		SceneObject o;
		o.mesh = m;
		o.world = m->local * root;
		o.materials = &s.materials[k++];
		s.objects.push_back(o);
	}
	// Shadow quad under the car's bounding box, 1 unit above the floor
	if (bmin.x <= bmax.x) {
		s.shadow.vertices[0].pos = Vec3(bmin.x, 0, bmin.z);
		s.shadow.vertices[1].pos = Vec3(bmin.x, 0, bmax.z);
		s.shadow.vertices[2].pos = Vec3(bmax.x, 0, bmax.z);
		s.shadow.vertices[3].pos = Vec3(bmax.x, 0, bmin.z);
		Mat4 sw = spot.rotation;
		sw.setRow(3, Vec3(spot.pos.x, _walk._floor.y + 1.0f, spot.pos.z), 1.0f);
		SceneObject o;
		o.mesh = &s.shadow;
		o.world = sw;
		o.materials = &s.shadowMats;
		s.objects.push_back(o);
	}
}

void LotPane::updateHover() {
	const float cx = (_rect.left + _rect.right) * 0.5f;
	float best = 1e11f;
	for (int i = 0; i < _numSpots; i++) {
		if (!_slots[i].car)
			continue;
		Common::Point sp;
		if (!_walk.project(_rect, _spots[i].pos, sp))
			continue;
		if (fabsf(sp.x - cx) < fabsf(best - cx)) {
			best = (float)sp.x;
			_hovered = _slots[i].car;
		}
	}
	if (best >= _rect.right || best <= _rect.left)
		_hovered = nullptr;
	_arrowX = _hovered ? (int)best : 99999;
}

void LotPane::render() {
	Renderer3D *r3d = g_engine->_renderer3d;
	Gfx *gfx = g_engine->_gfx;
	if (!r3d)
		return;
	r3d->beginFrame(_rect, Vec3(0.4f, 0.4f, 0.5f));
	r3d->setCamera(_walk._camera, _walk._zoom, 1.0f, _walk._far);
	// Sky sprite: (83,97)-(548, 97 + 0.35 * 288), depth 10000
	r3d->drawScreenQuad(Common::Rect(_rect.left, _rect.top, _rect.right, _rect.top + (int)(0.35f * _rect.height())),
	                    0.0f, 0.0f, 0.99f, 0.99f, &_skyMaterial, 9990.0f);
	Common::Array<SceneObject> objects;
	for (int i = 0; i < kLotSpots; i++)
		for (uint k = 0; k < _slots[i].objects.size(); k++)
			objects.push_back(_slots[i].objects[k]);
	_walk.render(*r3d, &objects);
	updateHover();
	r3d->endFrame(*gfx);
	// Hover arrow (additive) under the centred car at the bottom of the view
	if (_hovered && _arrow)
		gfx->blitAdditive(kPageFront, *_arrow, _arrowX - 16, _rect.bottom - 32);
}

// ---------------------------------------------------------------- CarLotScreen

CarLotScreen::CarLotScreen() : _background(nullptr), _field(nullptr), _crow1(nullptr), _crow2(nullptr), _depart(nullptr),
	_activation(0), _lastDepart(0), _departDelay(30000), _lastCrow(0), _crowDelay(20000), _crowCount(0), _lastHover(nullptr) {
	setRect(0, 0, 640, 480);
	_back.setOnClick(makeCallback(this, &CarLotScreen::onBack));
	_left.setOnHeld(makeCallback(this, &CarLotScreen::onLeftHeld));
	_right.setOnHeld(makeCallback(this, &CarLotScreen::onRightHeld));
	addChild(&_back);
	addChild(&_lot);
	addChild(&_right);
	addChild(&_left);
	_number.setRect(Common::Rect(57, 424, 162, 451));
	_origCost.setRect(Common::Rect(194, 424, 299, 451));
	_repairTime.setRect(Common::Rect(332, 424, 437, 451));
	_repairCost.setRect(Common::Rect(469, 424, 574, 451));
}

void CarLotScreen::init() {
	Resources *res = g_engine->_res;
	_background = res->getImage("CarLot.tga");
	const Graphics::ManagedSurface &up = *_background;
	const Graphics::ManagedSurface &dn = *res->getImage("CarLotDN.tga");
	_back.setFromSheets(Common::Rect(332, 48, 499, 81), up, dn, up);
	_right.setFromSheets(Common::Rect(566, 352, 600, 385), up, dn, up);
	_left.setFromSheets(Common::Rect(31, 352, 65, 385), up, dn, up);
	_lot.init();
	SoundManager *snd = g_engine->_snd;
	_field = snd->get("ambient\\field1.wav");
	_field->setLoop(true);
	_crow1 = snd->get("ambient\\crow1.wav");
	_crow2 = snd->get("ambient\\crow2.wav");
	_depart = snd->get("ambient\\cardepart.wav");
}

void CarLotScreen::activate() {
	debugC(1, kDebugGame, "Setting CarLot Active..");
	Mechanic *m = g_engine->_mechanic;
	m->saveIfReal();
	_field->play();
	_lot.reset();
	_lot.clear();
	// One spot per owned car; the workshop car keeps its spot empty
	for (uint i = 0; i < m->_cars.size(); i++)
		_lot.addCar(m->_cars[i] == m->_car ? nullptr : m->_cars[i]);
	_activation = g_engine->gameTimeMs();
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageBoth, *_background, 0, 0);
	m->drawName();
	m->drawCash();
	g_engine->_input->activateScreen(this);
	g_engine->_workShop->showToolCursor(false);
	refresh();
	_lot.render();
	drawCarInfo();
	_lastHover = _lot.hovered();
	debugC(1, kDebugGame, "..CarLot Set Active.");
}

void CarLotScreen::updateAmbient() {
	// Quirk: the "last" times are absolute, so the next sound needs now > activation + last + delay
	MsvcRandom &rnd = g_engine->_rnd;
	const uint32 now = g_engine->gameTimeMs();
	if (_activation + _lastDepart + _departDelay < now) {
		_lastDepart = now;
		_departDelay = (uint32)(50000.0f + rnd.frand(40000.0f));
		_depart->play();
	}
	if (_activation + _lastCrow + _crowDelay < now) {
		_lastCrow = now;
		_crowDelay = (uint32)(25000.0f + rnd.frand(30000.0f));
		_crowCount++;
		(_crowCount % 2 == 0 ? _crow2 : _crow1)->play();
	}
}

void CarLotScreen::stopSounds() {
	_field->stop();
	_crow1->stop();
	_crow2->stop();
	_depart->stop();
}

void CarLotScreen::drawCarInfo() {
	CarInstance *car = _lot.hovered();
	Gfx *gfx = g_engine->_gfx;
	if (!car) {
		gfx->restoreFromBack(_number.rect());
		gfx->restoreFromBack(_origCost.rect());
		gfx->restoreFromBack(_repairTime.rect());
		gfx->restoreFromBack(_repairCost.rect());
		return;
	}
	const BlitFont &cash = g_engine->_fonts[kFontCash];
	_number.draw(cash, Common::String::format("%i", car->_careerNum), kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
	_origCost.draw(cash, "$ " + Common::String::format("%4.2f", car->_purchaseCost), kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
	_repairTime.draw(g_engine->_fonts[kFontStandard], formatWorkTime(car->_repairTimeMs / 1000), kAlignCenter, kAlignCenter,
	                 kKeyDefault, kPageFront);
	_repairCost.draw(cash, "$ " + Common::String::format("%4.2f", car->_repairCost), kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
}

void CarLotScreen::onRefresh() {
	updateAmbient();
	_lot._walk.updateBob();
	if (_lot.isDirty()) {
		_lot.setDirty(false);
		_lot.render();
	}
	if (_lot.hovered() != _lastHover)
		drawCarInfo();
	_lastHover = _lot.hovered();
}

void CarLotScreen::onKeyDown(int key) {
	_lot._walk.onKey(key);
	_lot.setDirty(true);
}

void CarLotScreen::onLDown(const Common::Point &p) {
	CarInstance *car = _lot.hovered();
	if (!_lot.contains(p) || !car)
		return;
	Mechanic *m = g_engine->_mechanic;
	g_engine->_workShop->_region = 0;
	m->_car = car;
	m->_bank = m->findBank(car->model()->_id);
	stopSounds();
	_lot.clear();
	g_engine->_workShop->activate();
}

void CarLotScreen::onBack(Button *b) {
	_lot.clear();
	stopSounds();
	g_engine->_workShop->activate();
}

void CarLotScreen::onLeftHeld(Button *b) {
	_lot._walk.move(_lot._walk.right() * -1.0f);
}

void CarLotScreen::onRightHeld(Button *b) {
	_lot._walk.move(_lot._walk.right());
}

} // End of namespace Gearhead
