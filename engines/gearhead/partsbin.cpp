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
#include "gearhead/mechanic.h"
#include "gearhead/partsbin.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/** Rect_containsPoint: inclusive on all edges */
static bool rectContains(const Common::Rect &r, const Common::Point &p) {
	return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
}

// ---------------------------------------------------------------- PartButton

PartButton::PartButton() : _part(nullptr), _owner(nullptr), _selected(false) {
}

void PartButton::onHover(const Common::Point &p) {
	// PartButton_update (0x4392cd): matrix *= rotY(frameDt * 4)
	const float a = g_engine->frameDelta() * 4.0f;
	Mat4 rot;
	rot.setRow(0, Vec3(cosf(a), 0.0f, -sinf(a)), 0.0f);
	rot.setRow(2, Vec3(sinf(a), 0.0f, cosf(a)), 0.0f);
	_rotation = _rotation * rot;
	if (_owner)
		_owner->setListDirty(true);
}

void PartButton::onRefresh() {
	if (_owner)
		_owner->setListDirty(true);
}

void PartButton::drawPart(Renderer3D &r3d) {
	// PartButton_renderMesh (0x439424)
	if (!_part || _part->def->meshIndex < 0)
		return;
	CarModel *model = g_engine->_cars.find(_part->modelId, true);
	if (!model || _part->def->meshIndex >= (int)model->_meshes.size() || !model->_meshes[_part->def->meshIndex])
		return;
	const Mesh3D &mesh = *model->_meshes[_part->def->meshIndex];
	float radius = 0.0f;
	for (uint i = 0; i < mesh.vertices.size(); i++)
		radius = MAX(radius, mesh.vertices[i].pos.length());
	if (radius <= 0.0f)
		return;

	r3d.setViewport(_rect);
	r3d.setCamera(Mat4(), 2.4142137f, 0.1f, 3000.0f, 0.5f);
	r3d.setAmbient(Vec3(0.35f, 0.35f, 0.35f));
	r3d.clearLights();
	Light3D light;
	light.type = Light3D::kPoint;
	light.position = Vec3(-50.0f, 10.0f, -25.0f);
	light.k1 = 1.0f;
	light.k2 = 1.0f / 1000.0f;
	r3d.addLight(light);

	Mat4 world = _rotation;
	world.setRow(3, Vec3(0.0f, 0.0f, radius / tanf((float)M_PI / 8.0f)), 1.0f);

	// Perfect parts: the multi-material list; others: the condition body material.
	// The vertex colour is the material colour (it is added to the lighting).
	Common::Array<const Material3D *> mats;
	Common::Array<Vec3> colours;
	mats.resize(mesh.polys.size());
	colours.resize(mesh.vertices.size());
	const PartDef *pd = _part->def;
	uint k = 0, j = 0;
	for (uint c = 0; c < mesh.polys.size(); c++) {
		const Material3D *m;
		if (_part->condition == 3 && !pd->multiMaterials.empty()) {
			const int mi = pd->multiMaterials[MIN<uint>(k, pd->multiMaterials.size() - 1)];
			m = (mi >= 0 && mi < (int)model->_materials.size()) ? model->_materials[mi] : nullptr;
			if (j < pd->switchPolys.size() && (int)c == pd->switchPolys[j] - 1) {
				k++;
				j++;
			}
		} else if (_part->condition == 3) {
			const int fm = mesh.polys[c].fileMaterial;
			m = (fm >= 0 && fm < (int)model->_materials.size()) ? model->_materials[fm] : conditionMaterial(3);
		} else {
			m = conditionMaterial(_part->condition);
		}
		mats[c] = m;
		if (m)
			for (int v = 0; v < 3; v++)
				colours[mesh.polys[c].v[v]] = m->color;
	}
	r3d.drawMesh(mesh, world, mats, &colours);
}

// ---------------------------------------------------------------- PartList

PartList::PartList() : _onClick(nullptr), _onPress(nullptr), _hover(false), _showPageNumber(false), _cols(1), _rows(1), _page(0), _lastPage(0) {
}

PartList::~PartList() {
	for (uint i = 0; i < _buttons.size(); i++)
		delete _buttons[i];
	delete _onPress;
	delete _onClick;
}

void PartList::onButtonPress(Button *b) {
	if (_onPress)
		(*_onPress)(b);
}

void PartList::onButtonClick(Button *b) {
	if (_onClick)
		(*_onClick)(b);
}

void PartList::layoutGrid(const Common::Rect &r, int cols, int rows, int gap, ButtonCallback *onClick, ButtonCallback *onPress) {
	_rect = r;
	delete _onPress;
	_onPress = onPress;
	delete _onClick;
	_onClick = onClick;
	_cols = cols;
	_rows = rows;
	const int cellW = (int)((double)(r.width() - (cols + 1) * gap) / cols);
	const int cellH = (int)((double)(r.height() - (rows + 1) * gap) / rows);
	int x = r.left + gap, y = r.top + gap;
	for (int i = 0; i < cols * rows; i++) {
		PartButton *b = new PartButton();
		b->_owner = this;
		b->setRect(x, y, x + cellW, y + cellH);
		b->_tag = i;
		b->setOnPress(makeCallback(this, &PartList::onButtonPress));
		b->setOnClick(makeCallback(this, &PartList::onButtonClick));
		_buttons.push_back(b);
		addChild(b);
		x += cellW + gap;
		if ((r.width() - gap) - cellW <= x - r.left) {
			x = r.left + gap;
			y += cellH + gap;
		}
	}
}

void PartList::refreshList() {
	_lastPage = ((int)_items.size() - 1) / (_cols * _rows);
	if (_lastPage <= _page)
		_page = _lastPage;
	fillPage();
}

void PartList::fillPage() {
	if (_buttons.empty())
		error("PartList with 0 PartButtons.");
	for (uint i = 0; i < _buttons.size(); i++)
		_buttons[i]->_part = nullptr;
	const int start = _page * _cols * _rows;
	int n = 0;
	for (int i = start; i < (int)_items.size() && n < _cols * _rows; i++)
		_buttons[n++]->_part = _items[i];
	setListDirty(true);
	if (_showPageNumber)
		_pageNumber.draw(g_engine->_fonts[kFontTinyWhite], Common::String::format("%i", _page + 1),
		                 kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
}

bool PartList::nextPage() {
	if (_page >= _lastPage)
		return false;
	_page++;
	fillPage();
	return true;
}

bool PartList::prevPage() {
	if (_page <= 0)
		return false;
	_page--;
	fillPage();
	return true;
}

void PartList::selectByPartId(int id) {
	for (uint i = 0; i < _buttons.size(); i++)
		_buttons[i]->_selected = _buttons[i]->_part && _buttons[i]->_part->def->id == id;
	setListDirty(true);
}

void PartList::clearSelection() {
	for (uint i = 0; i < _buttons.size(); i++)
		_buttons[i]->_selected = false;
	setListDirty(true);
}

void PartList::drawConditionCorner(const PartButton &b) {
	// PartList_drawConditionCorner (0x43b868): a 13.3 x 10 triangle in the top-left corner
	Gfx *gfx = g_engine->_gfx;
	const uint32 colour = gfx->rgb(conditionColour(b._part->condition).x, conditionColour(b._part->condition).y,
	                               conditionColour(b._part->condition).z);
	for (int row = 0; row < 10; row++) {
		const int w = (int)(13.333333f * (10 - row) / 10.0f + 0.5f);
		if (w > 0)
			gfx->fill(kPageFront, Common::Rect(b.rect().left, b.rect().top + row, b.rect().left + w, b.rect().top + row + 1), colour);
	}
}

void PartList::drawButtonFrame(const PartButton &b) {
	// PartList_drawButtonFrame (0x43b5be): outline in the condition colour
	Gfx *gfx = g_engine->_gfx;
	const Vec3 c = conditionColour(b._part->condition);
	const uint32 colour = gfx->rgb(c.x, c.y, c.z);
	const Common::Rect &r = b.rect();
	gfx->fill(kPageFront, Common::Rect(r.left, r.top, r.right + 1, r.top + 1), colour);
	gfx->fill(kPageFront, Common::Rect(r.left, r.bottom, r.right + 1, r.bottom + 1), colour);
	gfx->fill(kPageFront, Common::Rect(r.left, r.top, r.left + 1, r.bottom + 1), colour);
	gfx->fill(kPageFront, Common::Rect(r.right, r.top, r.right + 1, r.bottom + 1), colour);
}

void PartList::draw() {
	// PartList_draw (0x43bc93), "Parts" mode: thumbnails on black, then corners and frames
	Renderer3D *r3d = g_engine->_renderer3d;
	Gfx *gfx = g_engine->_gfx;
	if (r3d) {
		r3d->beginFrame(_rect, Vec3(0.0f, 0.0f, 0.0f));
		for (uint i = 0; i < _buttons.size(); i++)
			if (_buttons[i]->_part)
				_buttons[i]->drawPart(*r3d);
		r3d->setViewport(_rect);
		r3d->endFrame(*gfx);
	} else {
		gfx->fill(kPageFront, _rect, 0);
	}
	for (uint i = 0; i < _buttons.size(); i++) {
		const PartButton &b = *_buttons[i];
		if (b._part)
			g_engine->_fonts[kFontTinyWhite].drawText(*gfx, b._part->def->name, b.rect(), kAlignCenter, kAlignEnd,
			                                          kKeyDefault, kPageFront);
	}
	const Common::Point &mouse = g_engine->_input->mouse();
	for (uint i = 0; i < _buttons.size(); i++) {
		const PartButton &b = *_buttons[i];
		if (!b._part)
			continue;
		drawConditionCorner(b);
		if (b._selected || b.contains(mouse))
			drawButtonFrame(b);
	}
}

// ---------------------------------------------------------------- PartsBin

PartsBin::PartsBin() : _pageSound(nullptr) {
	setRect(2, 368, 513, 484);
	_up.setOnPress(makeCallback(this, &PartsBin::onScrollUp));
	_down.setOnPress(makeCallback(this, &PartsBin::onScrollDown));
	_repair.setOnClick(makeCallback(this, &PartsBin::onRepairClick));
	_scrap.setOnClick(makeCallback(this, &PartsBin::onScrapClick));
	addChild(&_list);
	addChild(&_up);
	addChild(&_down);
	addChild(&_scrap);
	addChild(&_repair);
	_repairRect = Common::Rect(414, 371, 505, 421);
	_scrapRect = Common::Rect(414, 425, 505, 475);
	_repair.setRect(_repairRect.left, _repairRect.top, _repairRect.right, _repairRect.bottom);
	_scrap.setRect(_scrapRect.left, _scrapRect.top, _scrapRect.right, _scrapRect.bottom);
	// PartsBin_initList (0x416c2e)
	_list.layoutGrid(Common::Rect(5, 370, 385, 475), 4, 2, 2, nullptr, makeCallback(this, &PartsBin::onDragPart));
	_list._showPageNumber = true;
	_list._pageNumber.setRect(Common::Rect(390, 413, 410, 433));
}

void PartsBin::init() {
	Resources *res = g_engine->_res;
	const Graphics::ManagedSurface &up = *res->getImage("WorkShopUP.tga");
	const Graphics::ManagedSurface &dn = *res->getImage("WorkShopDN.tga");
	const Graphics::ManagedSurface &off = *res->getImage("WorkShop.tga");
	_up.setFromSheets(Common::Rect(389, 370, 411, 403), up, dn, off);
	_down.setFromSheets(Common::Rect(389, 442, 411, 475), up, dn, off);
	_pageSound = g_engine->_snd->get("rattle\\rattle1.wav");
}

void PartsBin::refill() {
	_list.clearItems();
	Mechanic *m = g_engine->_mechanic;
	const int region = g_engine->_workShop->_region;
	if (m->_bank) {
		for (uint i = 0; i < m->_bank->bin.size(); i++) {
			Part *p = m->_bank->bin[i];
			if (region == 0 || p->def->region == region)
				_list.addPart(p);
		}
	}
	_list.refreshList();
}

void PartsBin::clear() {
	_list.clearItems();
	_list.refreshList();
}

void PartsBin::onScrollUp(Button *b) {
	if (_list.prevPage())
		_pageSound->play();
}

void PartsBin::onScrollDown(Button *b) {
	if (_list.nextPage())
		_pageSound->play();
}

void PartsBin::onRepairClick(Button *b) {
	g_engine->_msgBox->message("Repair Square", "Drag parts from the Parts Bin onto the Repair Square to repair them.", kDlgButtonOk);
}

void PartsBin::onScrapClick(Button *b) {
	g_engine->_msgBox->message("Scrap Square", "Drag parts from the Parts Bin onto the Scrap Square to scrap them.", kDlgButtonOk);
}

Common::Point PartsBin::dragImage(const Common::Rect &rect) {
	// Screen_dragImage (0x454260): the cell follows the pointer until the button is released
	Gfx *gfx = g_engine->_gfx;
	Input *in = g_engine->_input;
	Common::Rect src = rect;
	src.clip(Common::Rect(kScreenWidth, kScreenHeight));
	Graphics::ManagedSurface image(src.width(), src.height(), gfx->format());
	image.simpleBlitFrom(gfx->front(), src, Common::Point(0, 0));
	Graphics::ManagedSurface under(src.width(), src.height(), gfx->format());
	const Common::Point offset(src.left - in->mouse().x, src.top - in->mouse().y);

	Pane modal;
	Widget *savedKey = in->keyTarget();
	in->releaseCapture();
	in->setRoot(&modal);
	in->setKeyTarget(nullptr);

	Common::Rect cur(src.width(), src.height());
	cur.moveTo(in->mouse().x + offset.x, in->mouse().y + offset.y);
	under.simpleBlitFrom(gfx->front(), cur, Common::Point(0, 0));
	Common::Point last = in->mouse();
	while ((in->buttons() & 1) && g_engine->runFrame()) {
		if (in->mouse() == last)
			continue;
		last = in->mouse();
		gfx->blit(kPageFront, under, cur.left, cur.top);
		cur.moveTo(in->mouse().x + offset.x, in->mouse().y + offset.y);
		Common::Rect grab = cur;
		grab.clip(Common::Rect(kScreenWidth, kScreenHeight));
		under.fillRect(Common::Rect(under.w, under.h), 0);
		if (!grab.isEmpty())
			under.simpleBlitFrom(gfx->front(), grab, Common::Point(grab.left - cur.left, grab.top - cur.top));
		gfx->blit(kPageFront, image, cur.left, cur.top);
	}
	gfx->blit(kPageFront, under, cur.left, cur.top);

	// The workshop becomes the modal handler again (hard-coded in the original)
	in->setRoot(g_engine->_workShop);
	in->setKeyTarget(savedKey);
	in->releaseCapture();
	return in->mouse();
}

void PartsBin::onDragPart(Button *b) {
	PartButton *pb = (PartButton *)b;
	if (!pb->_part)
		return;
	Part *part = pb->_part;
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	Common::Rect r = pb->rect();
	r.grow(1);
	const Common::Point drop = dragImage(r);

	if (ws->_carView.contains(drop)) {
		// Onto the car: open the part's region and attach it with the Air Ratchet
		if (ws->_region != part->def->region && !ws->setMode(part->def->region, true))
			return;
		if (ws->tool() != &ws->_airRatchet) {
			const Common::Rect &tr = ws->_toolPalette._airRatchet.rect();
			ws->_toolPalette._airRatchet.onLDown(Common::Point(tr.left + 2, tr.top + 2));
		}
		ws->_airRatchet.beginAttach(part);
		return;
	}
	if (rectContains(_repairRect, drop)) {
		if (part->condition == 3) {
			g_engine->_msgBox->message("Repair Part", "That part's in perfect condition!", kDlgButtonOk);
			return;
		}
		if (part->condition == 0) {
			g_engine->_msgBox->message("Repair Part", "That part's too messed up to repair. The best you can do is scrap it and get a few bucks from the Junkyard.", kDlgButtonOk);
			return;
		}
		const float cost = part->repairCost();
		if (g_engine->_msgBox->ask("Repair Part", "It will cost $" + Common::String::format("%4.2f", cost) + " to fix up this " +
		                           part->def->name + ". Do you want to repair it?", kDlgButtonOk, kDlgButtonCancel)) {
			if (!m->pay((int)part->repairCost(), false))
				return;
			m->repairPart(part);
			refill();
		}
		return;
	}
	if (rectContains(_scrapRect, drop)) {
		if (g_engine->_msgBox->ask("Scrap Part", "The JunkYard will pay $" + Common::String::format("%4.2f", part->junkPrice()) +
		                           " for this " + part->def->name + ". Do you want to scrap it?", kDlgButtonOk, kDlgButtonCancel)) {
			m->sellPart(part);
			refill();
		}
	}
}

} // End of namespace Gearhead
