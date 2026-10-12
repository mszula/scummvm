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
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobs.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/** The template mesh of a part: the car model's mesh of PD_MESHNAME */
static const Mesh3D *partMesh(const Part *part, const CarModel **modelOut = nullptr) {
	CarModel *model = g_engine->_cars.find(part->modelId, true);
	if (modelOut)
		*modelOut = model;
	const int mi = part->def->meshIndex;
	if (!model || mi < 0 || mi >= (int)model->_meshes.size())
		return nullptr;
	return model->_meshes[mi];
}

/** Part_applyBaseMaterials applied to a copy of the template's material table */
static void baseMaterials(const Part *part, const CarModel *model, const Mesh3D &mesh, Common::Array<const Material3D *> &mats) {
	mats.resize(mesh.polys.size());
	const CarInstance *car = g_engine->_mechanic->_car;
	for (uint c = 0; c < mesh.polys.size(); c++) {
		const int fm = mesh.polys[c].fileMaterial;
		if (fm == kPaintMaterialIndex)
			mats[c] = car ? &car->_paintMaterial : conditionMaterial(3);
		else
			mats[c] = (fm >= 0 && fm < (int)model->_materials.size()) ? model->_materials[fm] : nullptr;
	}
	const PartDef *pd = part->def;
	if (part->condition == 3) {
		if (pd->multiMaterials.empty())
			return;
		uint k = 0, j = 0;
		for (uint c = 0; c < mats.size(); c++) {
			const int mi = pd->multiMaterials[MIN<uint>(k, pd->multiMaterials.size() - 1)];
			mats[c] = (mi >= 0 && mi < (int)model->_materials.size()) ? model->_materials[mi] : nullptr;
			if (j < pd->switchPolys.size() && (int)c == pd->switchPolys[j] - 1) {
				k++;
				j++;
			}
		}
	} else {
		for (uint c = 0; c < mats.size(); c++)
			mats[c] = conditionMaterial(part->condition);
	}
}

// ---------------------------------------------------------------- Shelf

void Shelf::init(const SceneMesh &mesh, const SceneNode &node) {
	Vec3 mn(1000000.0f, 1000000.0f, 1000000.0f), mx(-1000000.0f, -1000000.0f, -1000000.0f);
	for (uint i = 0; i < mesh.mesh.vertices.size(); i++) {
		const Vec3 &w = mesh.mesh.vertices[i].pos;
		mn = Vec3(MIN(mn.x, w.x), MIN(mn.y, w.y), MIN(mn.z, w.z));
		mx = Vec3(MAX(mx.x, w.x), MAX(mx.y, w.y), MAX(mx.z, w.z));
	}
	mn.y = mx.y = 0.0f;
	// The shelf runs along its longer horizontal side
	if (mx.x - mn.x <= mx.z - mn.z)
		mn.x = mx.x = (mn.x + mx.x) * 0.5f;
	else
		mn.z = mx.z = (mn.z + mx.z) * 0.5f;
	const Vec3 half = (mx - mn) * 0.5f;
	const Vec3 centre = mn + half;
	_dir = (mn - mx).normalized();
	_start = centre - _dir.mul(half);
	_start += _dir * 15.0f;
	_length = (_start - mn).length();
	_start.y = node.pos.y;
	_full = false;
}

bool Shelf::tryPlace(Part *part) {
	const CarModel *model = nullptr;
	const Mesh3D *tmpl = partMesh(part, &model);
	if (!tmpl)
		return false;
	// The part is turned so that its narrower side lies along the shelf
	const Vec3 axis = (tmpl->boundsMax.x - tmpl->boundsMin.x <= tmpl->boundsMax.z - tmpl->boundsMin.z) ? Vec3(1, 0, 0) : Vec3(0, 0, 1);
	const float angle = acosf(CLIP(axis.dot(_dir), -1.0f, 1.0f));
	Mat4 m;
	m.setRow(0, Vec3(cosf(angle), 0.0f, -sinf(angle)), 0.0f);
	m.setRow(2, Vec3(sinf(angle), 0.0f, cosf(angle)), 0.0f);
	float radius = 0.0f;
	for (uint i = 0; i < tmpl->vertices.size(); i++)
		radius = MAX(radius, tmpl->vertices[i].pos.length());
	// Quirk: consecutive centres are only 10 + r apart (the previous radius is not counted again)
	Vec3 pos = _start;
	for (uint i = 0; i < _items.size(); i++)
		pos += _dir * (_items[i]->radius + 10.0f);
	pos += _dir * radius;
	if ((pos - _start).length() > _length) {
		_full = true;
		return false;
	}
	ShelfItem *item = new ShelfItem();
	item->part = part;
	item->radius = radius;
	baseMaterials(part, model, *tmpl, item->materials);
	float minY = 999999.0f;
	for (uint i = 0; i < tmpl->vertices.size(); i++)
		minY = MIN(minY, m.transformPoint(tmpl->vertices[i].pos).y);
	pos.y -= minY - 2.0f;
	m.setRow(3, pos, 1.0f);
	item->object.mesh = tmpl;
	item->object.world = m;
	item->object.materials = &item->materials;
	_items.push_back(item);
	return true;
}

void Shelf::clear() {
	for (uint i = 0; i < _items.size(); i++)
		delete _items[i];
	_items.clear();
	_full = false;
}

// ---------------------------------------------------------------- YardArea

YardArea::~YardArea() {
	clear();
	for (uint i = 0; i < _shelves.size(); i++)
		delete _shelves[i];
	delete _scene;
}

void YardArea::init(Scene3DS *scene, Widget *listener) {
	_scene = scene;
	_walk.init(scene, listener);
	for (int k = 1;; k++) {
		const Common::String name = Common::String::format("Shelf%s%d", k < 10 ? "0" : "", k);
		const SceneMesh *m = scene->findMesh(name);
		const SceneNode *n = scene->findNode(name);
		if (!m || !n)
			break;
		Shelf *s = new Shelf();
		s->init(*m, *n);
		_shelves.push_back(s);
	}
	// The camera stands at play_01: the walking eye height comes from the scene
	_walk._eyeHeight = _walk._camera.row(3).y - _walk._floor.y;
}

void YardArea::addPart(Part *part) {
	if (_shelves.empty())
		error("No shelves for this JunkYard Area!");
	for (uint i = 0; i < _shelves.size(); i++) {
		if (_shelves[i]->_full)
			continue;
		if (_shelves[i]->tryPlace(part))
			return;
	}
	// every shelf is full: the part is not shown this visit
}

void YardArea::clear() {
	for (uint i = 0; i < _shelves.size(); i++)
		_shelves[i]->clear();
}

// ---------------------------------------------------------------- YardPane

YardPane::YardPane() : _area(1), _current(nullptr), _hoverPart(nullptr), _prevPart(nullptr), _hoverItem(nullptr),
	_prevItem(nullptr) {
	setRect(51, 95, 469, 348);
	_label.setRect(Common::Rect(120, 349, 597, 369));
}

YardPane::~YardPane() {
}

void YardPane::init() {
	static const char *const kFiles[3] = { "JYEngine.3ds", "JYBody.3ds", "JYRGear.3ds" };
	for (int i = 0; i < 3; i++) {
		Scene3DS *s = new Scene3DS();
		if (!s->load(*g_engine->_res, "Data\\Scenes\\", kFiles[i])) {
			if (i == 0)
				error("error loading Junkyard mesh");
			warning("Cannot load %s", kFiles[i]);
		}
		_areas[i].init(s, this);
	}
	_current = &_areas[0];
}

void YardPane::populate() {
	Mechanic *m = g_engine->_mechanic;
	if (m->_bank) {
		for (uint i = 0; i < m->_bank->yard.size(); i++) {
			Part *p = m->_bank->yard[i];
			if (m->_job && !m->_job->isPartAllowed(p->def))
				continue;
			const int region = p->def->region;
			if (region >= 1 && region <= 3)
				_areas[region - 1].addPart(p);
		}
	}
	_hoverPart = _prevPart = nullptr;
	_hoverItem = _prevItem = nullptr;
}

void YardPane::setArea(int area) {
	_area = CLIP(area, 1, 3);
	_current = &_areas[_area - 1];
	setDirty(true);
}

void YardPane::clearAll() {
	for (int i = 0; i < 3; i++)
		_areas[i].clear();
	_hoverPart = _prevPart = nullptr;
	_hoverItem = _prevItem = nullptr;
}

void YardPane::render() {
	Renderer3D *r3d = g_engine->_renderer3d;
	if (!r3d || !_current)
		return;
	r3d->beginFrame(_rect, Vec3(0.4f, 0.4f, 0.5f));
	Common::Array<SceneObject> objects;
	for (uint s = 0; s < _current->_shelves.size(); s++)
		for (uint i = 0; i < _current->_shelves[s]->_items.size(); i++)
			objects.push_back(_current->_shelves[s]->_items[i]->object);
	_current->_walk.render(*r3d, &objects);
	r3d->endFrame(*g_engine->_gfx);
}

void YardPane::pick(const Common::Point &p, Part *&part, ShelfItem *&item) const {
	part = nullptr;
	item = nullptr;
	Vec3 o, d;
	_current->_walk.pickRay(_rect, p, o, d);
	float best = 1e30f;
	bool hit = false;
	const Mat4 identity;
	for (uint i = 0; i < _current->_scene->_meshes.size(); i++) {
		const SceneMesh *m = _current->_scene->_meshes[i];
		float t;
		if (m->visible && rayHitsMesh(m->mesh, identity, o, d, t) && (!hit || t < best)) {
			best = t;
			hit = true;
		}
	}
	for (uint s = 0; s < _current->_shelves.size(); s++) {
		for (uint i = 0; i < _current->_shelves[s]->_items.size(); i++) {
			ShelfItem *it = _current->_shelves[s]->_items[i];
			float t;
			if (it->object.visible && rayHitsMesh(*it->object.mesh, it->object.world, o, d, t) && (!hit || t < best)) {
				best = t;
				hit = true;
				item = it;
			}
		}
	}
	if (hit && item)
		part = item->part;
	else
		item = nullptr;
}

void YardPane::restoreItemMaterials(ShelfItem *item) {
	const CarModel *model = nullptr;
	const Mesh3D *tmpl = partMesh(item->part, &model);
	if (tmpl)
		baseMaterials(item->part, model, *tmpl, item->materials);
}

void YardPane::onHover(const Common::Point &p) {
	if (!_current)
		return;
	pick(p, _hoverPart, _hoverItem);
	if (_hoverPart && _hoverPart != _prevPart) {
		for (uint i = 0; i < _hoverItem->materials.size(); i++)
			_hoverItem->materials[i] = conditionHighlight(_hoverPart->condition);
		setDirty(true);
		_label.draw(g_engine->_fonts[kFontStandard], _hoverPart->def->name + "  ($" +
		            Common::String::format("%4.2f", _hoverPart->junkPrice()) + ")",
		            kAlignStart, kAlignCenter, kKeyDefault, kPageFront);
	}
	if (!_hoverPart && _prevPart)
		g_engine->_gfx->restoreFromBack(_label.rect());
	if (_prevPart && _prevPart != _hoverPart) {
		if (_prevItem)
			restoreItemMaterials(_prevItem);
		setDirty(true);
	}
	_prevPart = _hoverPart;
	_prevItem = _hoverItem;
}

void YardPane::onLUp(const Common::Point &p) {
	if (_hoverPart && g_engine->_junkYard->buyPart(_hoverPart, _hoverItem)) {
		_hoverItem->object.visible = false;
		_hoverPart = nullptr;
		_hoverItem = nullptr;
	}
}

bool YardPane::debugItemScreenPos(int n, Common::Point &out, Common::String &name) const {
	int k = 0;
	for (uint s = 0; s < _current->_shelves.size(); s++) {
		for (uint i = 0; i < _current->_shelves[s]->_items.size(); i++, k++) {
			const ShelfItem *it = _current->_shelves[s]->_items[i];
			if (k != n)
				continue;
			name = it->part->def->name;
			if (!it->object.visible)
				return false;
			const Mesh3D &m = *it->object.mesh;
			for (uint p = 0; p < m.polys.size(); p++) {
				const Vec3 c = (it->object.world.transformPoint(m.vertices[m.polys[p].v[0]].pos) +
				                it->object.world.transformPoint(m.vertices[m.polys[p].v[1]].pos) +
				                it->object.world.transformPoint(m.vertices[m.polys[p].v[2]].pos)) * (1.0f / 3.0f);
				Common::Point sp;
				if (!_current->_walk.project(_rect, c, sp) || !contains(sp))
					continue;
				Part *part;
				ShelfItem *item;
				pick(sp, part, item);
				if (item == it) {
					out = sp;
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

// ---------------------------------------------------------------- YardAreaPane

YardAreaPane::YardAreaPane() {
	setRect(499, 141, 614, 239);
	for (int i = 0; i < 3; i++) {
		_buttons[i]._tag = i + 1;
		_buttons[i].setRadio(this, 1, makeCallback(this, &YardAreaPane::onArea));
	}
	addChild(&_buttons[1]);
	addChild(&_buttons[2]);
	addChild(&_buttons[0]);
}

void YardAreaPane::init() {
	Resources *res = g_engine->_res;
	const Graphics::ManagedSurface &up = *res->getImage("JunkYardUP.tga");
	const Graphics::ManagedSurface &dn = *res->getImage("JunkYardDN.tga");
	_buttons[0].setFromSheets(Common::Rect(499, 141, 614, 172), up, dn, up);
	_buttons[1].setFromSheets(Common::Rect(495, 172, 608, 204), up, dn, up);
	_buttons[2].setFromSheets(Common::Rect(490, 209, 619, 239), up, dn, up);
}

void YardAreaPane::syncButtons(int area) {
	for (int i = 0; i < 3; i++)
		_buttons[i].setStateSilently(area == i + 1 ? Button::kStateDown : Button::kStateUp);
}

void YardAreaPane::onArea(Button *b) {
	g_engine->_junkYard->_yard.setArea(b->_tag);
}

// ---------------------------------------------------------------- JunkYardScreen

JunkYardScreen::JunkYardScreen() : _background(nullptr) {
	setRect(0, 0, 640, 480);
	_back.setOnClick(makeCallback(this, &JunkYardScreen::onBack));
	_left.setOnHeld(makeCallback(this, &JunkYardScreen::onLeftHeld));
	_right.setOnHeld(makeCallback(this, &JunkYardScreen::onRightHeld));
	_up.setOnClick(makeCallback(this, &JunkYardScreen::onCartUp));
	_down.setOnClick(makeCallback(this, &JunkYardScreen::onCartDown));
	// JunkYard_ctor (0x4270c0): child order
	addChild(&_cart);
	addChild(&_back);
	addChild(&_yard);
	addChild(&_right);
	addChild(&_left);
	addChild(&_areaPane);
	addChild(&_up);
	addChild(&_down);
	// JunkYard_initCartList (0x42741f)
	_cart.layoutGrid(Common::Rect(11, 369, 600, 474), 6, 2, 2, makeCallback(this, &JunkYardScreen::onCartItem), nullptr);
}

void JunkYardScreen::init() {
	Resources *res = g_engine->_res;
	_background = res->getImage("JunkYardUP.tga");
	const Graphics::ManagedSurface &up = *_background;
	const Graphics::ManagedSurface &dn = *res->getImage("JunkYardDN.tga");
	_back.setFromSheets(Common::Rect(332, 48, 499, 81), up, dn, up);
	_right.setFromSheets(Common::Rect(472, 315, 506, 348), up, dn, up);
	_left.setFromSheets(Common::Rect(12, 315, 46, 348), up, dn, up);
	_up.setFromSheets(Common::Rect(606, 364, 628, 397), up, dn, up);
	_down.setFromSheets(Common::Rect(606, 441, 628, 474), up, dn, up);
	_areaPane.init();
	_yard.init();
}

void JunkYardScreen::activate() {
	Mechanic *m = g_engine->_mechanic;
	if (!m->_car)
		return;
	debugC(1, kDebugGame, "Setting JunkYard Active..");
	m->saveIfReal();
	m->restockYard();
	_yard.populate();
	// The yard opens on the area of the workshop region (COMPLETE -> ENGINE)
	int area = g_engine->_workShop->_region;
	if (area == 0)
		area = 1;
	_yard.setArea(area);
	_cart.clearItems();
	_cart.refreshList();
	_entries.clear();
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageBoth, *_background, 0, 0);
	m->drawName();
	m->drawCash();
	g_engine->_input->activateScreen(this);
	g_engine->_workShop->showToolCursor(false);
	_areaPane.syncButtons(_yard._area);
	_yard.setDirty(true);
	refresh();
	debugC(1, kDebugGame, "..JunkYard Set Active.");
}

void JunkYardScreen::rebuildCart() {
	_cart.clearItems();
	for (uint i = 0; i < _entries.size(); i++)
		_cart.addPart(_entries[i].part);
	_cart.refreshList();
}

bool JunkYardScreen::buyPart(Part *part, ShelfItem *item) {
	if (!g_engine->_mechanic->buyYardPart(part))
		return false;
	CartEntry e;
	e.part = part;
	e.item = item;
	_entries.push_back(e);
	rebuildCart();
	g_engine->_snd->play("rattle\\rattle12.wav");
	return true;
}

void JunkYardScreen::onCartItem(Button *b) {
	PartButton *pb = (PartButton *)b;
	Part *part = pb->_part;
	if (!part)
		return;
	g_engine->_mechanic->returnYardPart(part);
	for (uint i = 0; i < _entries.size(); i++) {
		if (_entries[i].part == part) {
			if (_entries[i].item)
				_entries[i].item->object.visible = true;
			_entries.remove_at(i);
			break;
		}
	}
	rebuildCart();
	_yard.setDirty(true);
}

void JunkYardScreen::onBack(Button *b) {
	_yard.clearAll();
	if (!_entries.empty())
		g_engine->_snd->play("misc\\cashreg.wav");
	_entries.clear();
	g_engine->_workShop->activate();
}

void JunkYardScreen::onLeftHeld(Button *b) {
	Walkabout &w = _yard.current()->_walk;
	w.move(w.right() * -1.0f);
}

void JunkYardScreen::onRightHeld(Button *b) {
	Walkabout &w = _yard.current()->_walk;
	w.move(w.right());
}

void JunkYardScreen::onCartUp(Button *b) {
	_cart.prevPage();
}

void JunkYardScreen::onCartDown(Button *b) {
	_cart.nextPage();
}

void JunkYardScreen::onKeyDown(int key) {
	_yard.current()->_walk.onKey(key);
	_yard.setDirty(true);
}

void JunkYardScreen::onRefresh() {
	_yard.current()->_walk.updateBob();
	if (!_cart.listDirty() && !_yard.isDirty())
		return;
	_cart.setListDirty(false);
	_yard.setDirty(false);
	_yard.render();
	_cart.draw();
}

} // End of namespace Gearhead
