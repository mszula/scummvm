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
#include "common/system.h"
#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/cardata.h"
#include "gearhead/font.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/paint.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"

namespace Gearhead {

/** MSVC __ftol */
static int ftol(double v) {
	if (!(v > -9.2e18 && v < 9.2e18))
		return 0;
	return (int)(uint32)(uint64)(int64)v;
}

static void surfaceToRGBA(const Graphics::ManagedSurface &s, Common::Array<byte> &out) {
	out.resize(s.w * s.h * 4);
	for (int y = 0; y < s.h; y++)
		for (int x = 0; x < s.w; x++) {
			byte r, g, b;
			s.format.colorToRGB(s.getPixel(x, y), r, g, b);
			byte *p = &out[(y * s.w + x) * 4];
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = 255;
		}
}

/** RGB(A) pixels -> a surface in the screen format (Bitmap_convert) */
static Graphics::ManagedSurface *toScreen(int w, int h, const Common::Array<byte> &px, int stride) {
	const Graphics::PixelFormat &fmt = g_engine->_gfx->format();
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(w, h, fmt);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const byte *p = &px[(y * w + x) * stride];
			s->setPixel(x, y, fmt.RGBToColor(p[0], p[1], p[2]));
		}
	return s;
}

/** Image_setPixelColor in the 16-bit screen format */
static uint32 screenColour(const Vec3 &c) {
	byte r, g, b;
	quantizePaint565(c, r, g, b);
	return g_engine->_gfx->format().RGBToColor(r, g, b);
}

static Graphics::ManagedSurface *cropWorkShop(const Common::Rect &r) {
	const Graphics::ManagedSurface *ws = g_engine->_res->getImage("WorkShop.tga");
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(r.width(), r.height(), ws->format);
	s->simpleBlitFrom(*ws, r, Common::Point(0, 0));
	return s;
}

// ---------------------------------------------------------------- BrushButton / ColorButton

BrushButton::BrushButton() : _maskW(0), _maskH(0), _radius(0), _fill(false) {
	_img[0] = _img[1] = _img[2] = nullptr;
}

BrushButton::~BrushButton() {
	for (int i = 0; i < 3; i++)
		delete _img[i];
}

void BrushButton::init(const Common::Rect &r, const Graphics::ManagedSurface &brush, int radius) {
	_fill = false;
	_radius = radius;
	const int size = r.width();
	// The brush itself (24x24, converted to palette indices) is the mask: dark pixels paint
	Common::Array<byte> px;
	surfaceToRGBA(brush, px);
	_maskW = brush.w;
	_maskH = brush.h;
	_mask.resize(_maskW * _maskH);
	for (int i = 0; i < _maskW * _maskH; i++)
		_mask[i] = (px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2]) / 3 < 0x41 ? 1 : 0;
	// [0] the brush resized to the button, [1] with a 2-pixel yellow frame, [2] the background
	Common::Array<byte> small;
	resizeRGBA(brush.w, brush.h, px, size, size, small);
	_img[0] = toScreen(size, size, small, 4);
	_img[1] = new Graphics::ManagedSurface(size, size, _img[0]->format);
	_img[1]->simpleBlitFrom(*_img[0]);
	const uint32 yellow = screenColour(Vec3(0.99f, 0.99f, 0.0f));
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++)
			if (!(x >= 2 && x <= size - 3 && y >= 2 && y <= size - 3))
				_img[1]->setPixel(x, y, yellow);
	_img[2] = cropWorkShop(r);
	setImages(_img[0], _img[1], _img[2]);
	setPos(r.left, r.top);
}

ColorButton::ColorButton() {
	_img[0] = _img[1] = _img[2] = nullptr;
}

ColorButton::~ColorButton() {
	for (int i = 0; i < 3; i++)
		delete _img[i];
}

void ColorButton::init(const Common::Rect &r, const Vec3 &colour) {
	_colour = colour;
	const Graphics::PixelFormat &fmt = g_engine->_gfx->format();
	const int w = r.width(), h = r.height();
	_img[0] = new Graphics::ManagedSurface(w, h, fmt);
	_img[0]->fillRect(Common::Rect(w, h), screenColour(colour));
	_img[1] = new Graphics::ManagedSurface(w, h, fmt);
	_img[1]->simpleBlitFrom(*_img[0]);
	const uint32 white = screenColour(Vec3(0.99f, 0.99f, 0.99f));
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			if (!(x >= 2 && x <= w - 3 && y >= 2 && y <= h - 3))
				_img[1]->setPixel(x, y, white);
	_img[2] = cropWorkShop(r);
	setImages(_img[0], _img[1], _img[2]);
	setPos(r.left, r.top);
}

// ---------------------------------------------------------------- DecalEdit

// g_DecalOrientOps (0x4e2afc): the operations that rebuild an orientation (4 = none);
// g_DecalOrientNext (0x4e2b3c): the orientation after op 0 mirror, 1 flip, 2 turn right, 3 turn left
static const int kOrientOps[8][2] = { { 4, 4 }, { 2, 4 }, { 2, 2 }, { 3, 4 }, { 1, 4 }, { 1, 2 }, { 0, 4 }, { 0, 2 } };
static const int kOrientNext[8][4] = {
	{ 6, 4, 1, 3 }, { 5, 7, 2, 0 }, { 4, 6, 3, 1 }, { 7, 5, 0, 2 },
	{ 2, 0, 5, 7 }, { 1, 3, 6, 4 }, { 0, 2, 7, 5 }, { 3, 1, 4, 6 }
};

DecalEdit::DecalEdit() : _owned(nullptr), _w(0), _h(0), _sw(0), _sh(0), _ghost(nullptr), _orient(0), _scale(1.0f) {
}

DecalEdit::~DecalEdit() {
	delete _ghost;
}

void DecalEdit::resetImage() {
	_work.clear();
	_w = _h = 0;
	const DecalData *d = _owned ? _owned->data : nullptr;
	if (!d)
		return;
	_w = d->width;
	_h = d->height;
	_work.resize(_w * _h * 3);
	for (int i = 0; i < _w * _h; i++)
		for (int c = 0; c < 3; c++)
			_work[i * 3 + c] = d->rgba[i * 4 + c];
}

void DecalEdit::setDecal(OwnedDecal *od) {
	_owned = od;
	_orient = 0;
	_scale = 1.0f;
	resetImage();
}

void DecalEdit::flip(bool mirror) {
	if (!_owned)
		return;
	Common::Array<byte> out(_work.size());
	for (int y = 0; y < _h; y++)
		for (int x = 0; x < _w; x++) {
			const int sx = mirror ? _w - 1 - x : x, sy = mirror ? y : _h - 1 - y;
			for (int c = 0; c < 3; c++)
				out[(y * _w + x) * 3 + c] = _work[(sy * _w + sx) * 3 + c];
		}
	_work = out;
	_orient = kOrientNext[_orient][mirror ? 0 : 1];
}

void DecalEdit::rotate(bool clockwise) {
	if (!_owned)
		return;
	// The new image is H x W; clockwise (x,y) -> (H-1-y, x), else (x,y) -> (y, W-1-x)
	const int nw = _h, nh = _w;
	Common::Array<byte> out(_work.size());
	for (int y = 0; y < _h; y++)
		for (int x = 0; x < _w; x++) {
			const int dx = clockwise ? _h - 1 - y : y, dy = clockwise ? x : _w - 1 - x;
			for (int c = 0; c < 3; c++)
				out[(dy * nw + dx) * 3 + c] = _work[(y * _w + x) * 3 + c];
		}
	_work = out;
	_w = nw;
	_h = nh;
	_orient = kOrientNext[_orient][clockwise ? 2 : 3];
}

void DecalEdit::reapplyOrient() {
	const int o = _orient;
	_orient = 0;
	for (int i = 0; i < 2; i++) {
		switch (kOrientOps[o][i]) {
		case 0:
			flip(true);
			break;
		case 1:
			flip(false);
			break;
		case 2:
			rotate(true);
			break;
		case 3:
			rotate(false);
			break;
		default:
			break;
		}
	}
}

void DecalEdit::recolor(Vec3 &colour) {
	// Quirk: the swatch's own colour is changed when it is black
	if (colour.x < 0.001f && colour.y < 0.001f && colour.z < 0.001f)
		colour = Vec3(0.1f, 0.1f, 0.1f);
	resetImage();
	const int type = _owned && _owned->data ? _owned->data->colorType : 0;
	for (int i = 0; i < _w * _h; i++) {
		byte *p = &_work[i * 3]; // r, g, b
		if (type == 1) {
			// tint: the grey level times the colour
			const int g = ftol((p[0] + p[1] + p[2]) / 3.0);
			p[0] = (byte)ftol(g * colour.x);
			p[1] = (byte)ftol(g * colour.y);
			p[2] = (byte)ftol(g * colour.z);
		} else if (type == 2 && p[0] > 250 && p[1] > 250 && p[2] > 250) {
			// white replaced by the colour
			p[0] = (byte)ftol(colour.x * 255.0);
			p[1] = (byte)ftol(colour.y * 255.0);
			p[2] = (byte)ftol(colour.z * 255.0);
		}
	}
	reapplyOrient();
}

void DecalEdit::buildCursor() {
	// The stamp: the work image at the chosen size
	_sw = ftol((double)_w * _scale);
	_sh = ftol((double)_h * _scale);
	Common::Array<byte> rgba(_w * _h * 4), scaled;
	for (int i = 0; i < _w * _h; i++) {
		for (int c = 0; c < 3; c++)
			rgba[i * 4 + c] = _work[i * 3 + c];
		rgba[i * 4 + 3] = 255;
	}
	resizeRGBA(_w, _h, rgba, _sw, _sh, scaled);
	_scaled.resize(_sw * _sh * 3);
	for (int i = 0; i < _sw * _sh; i++)
		for (int c = 0; c < 3; c++)
			_scaled[i * 3 + c] = scaled[i * 4 + c];
	// The ghost: only the pixels with odd x and odd y are kept (colour 0 is transparent)
	delete _ghost;
	_ghost = toScreen(_sw, _sh, scaled, 4);
	for (int y = 0; y < _sh; y++)
		for (int x = 0; x < _sw; x++)
			if ((y & 1) == 0 || (x & 1) == 0)
				_ghost->setPixel(x, y, 0);
}

// ---------------------------------------------------------------- SprayCan

SprayCan::SprayCan() : _brush(nullptr), _radius(0), _fill(false), _decal(nullptr), _blocked(false), _cellW(1.0f), _cellH(1.0f),
	_originX(0.0f), _originY(0.0f), _lastDist(0.0f), _gridValid(false), _spray(nullptr), _bead1(nullptr), _bead2(nullptr), _shakeDown(false),
	_ghostPart(nullptr) {
}

SprayCan::~SprayCan() {
	delete _ghostPart;
}

void SprayCan::init() {
	setCursor("Pointers\\paint.tga");
	SoundManager *snd = g_engine->_snd;
	_spray = snd->get("air\\spray.wav");
	_bead1 = snd->get("misc\\bead1.wav");
	_bead2 = snd->get("misc\\bead2.wav");
	_spray->setLoop(true);
	_brush = nullptr;
	_radius = 0;
	_fill = false;
	_decal = nullptr;
}

void SprayCan::setDecal(DecalEdit *edit) {
	_decal = edit;
	_decal->buildCursor();
}

void SprayCan::clearDecal() {
	_decal = nullptr;
}

bool SprayCan::ghostCursor(const Common::Point &mouse, Graphics::ManagedSurface *&img, Common::Point &hotspot) {
	if (!_decal || !_decal->_ghost)
		return false;
	// Pointer_draw: the dragged image centred on the pointer, cut to the car view (5,85)-(385,342)
	const Graphics::ManagedSurface &g = *_decal->_ghost;
	const Common::Point hot(g.w / 2, g.h / 2);
	const Common::Rect view(5, 85, 385, 342);
	delete _ghostPart;
	_ghostPart = new Graphics::ManagedSurface(g.w, g.h, g.format);
	_ghostPart->clear(0);
	for (int y = 0; y < g.h; y++) {
		const int sy = mouse.y - hot.y + y;
		if (sy < view.top || sy >= view.bottom)
			continue;
		for (int x = 0; x < g.w; x++) {
			const int sx = mouse.x - hot.x + x;
			if (sx >= view.left && sx < view.right)
				_ghostPart->setPixel(x, y, g.getPixel(x, y));
		}
	}
	img = _ghostPart;
	hotspot = hot;
	return true;
}

void SprayCan::onHover(const Common::Point &p) {
	Input *input = g_engine->_input;
	if (!(input->buttons() & 1))
		_blocked = false;
	if (!_decal) {
		// Shaking the can: 17 pixels down, then up again
		const int prevY = input->prevMouse().y;
		if (fabs((double)(prevY - p.y)) >= 17.0) {
			if (!_shakeDown && prevY < p.y) {
				_shakeDown = true;
				_bead1->play();
			} else if (_shakeDown && p.y < prevY) {
				_shakeDown = false;
				_bead2->play();
			}
		}
	}
	g_engine->_workShop->updateToolCursor(p);
}

void SprayCan::onLeftUp(const Common::Point &p) {
	_spray->stop();
}

void SprayCan::project(const Vec3 &p, float &sx, float &sy) const {
	// Projector (0x450cd9 / 0x450e38): the point is projected onto a plane at D = h / tan(zoom)
	// from the camera -- tan of the zoom value itself, so the plane lies behind the camera -- and
	// the result is scaled by 1.05 (x mirrored back)
	const CarView &cv = g_engine->_workShop->_carView;
	const Common::Rect &vp = cv.rect();
	const Mat4 &cam = cv.cameraWorld();
	const Vec3 v = p - cam.row(3);
	const float X = v.dot(cam.row(0)), Y = v.dot(cam.row(1)), Z = v.dot(cam.row(2));
	const float D = (float)vp.height() / (float)tan((double)CarView::kZoom);
	const float k = Z != 0.0f ? D / Z : 0.0f;
	sx = (float)vp.width() * 0.5f + (float)vp.left + X * k * -1.05f;
	sy = (float)vp.height() * 0.5f + (float)vp.top + Y * k * 1.05f;
}

void SprayCan::buildGrid() {
	Mechanic *m = g_engine->_mechanic;
	CarInstance *car = m->_car;
	_tris.clear();
	for (int y = 0; y < kGridSize; y++)
		for (int x = 0; x < kGridSize; x++)
			_cells[y][x].clear();
	_gridValid = false;
	if (!car)
		return;
	const CarView &cv = g_engine->_workShop->_carView;
	const Vec3 camPos = cv.cameraWorld().row(3), camFwd = cv.cameraWorld().row(2);
	float minX = 9999.0f, minY = 9999.0f, maxX = -9999.0f, maxY = -9999.0f;
	const CarModel *model = car->model();
	// The front-facing triangles drawn with the car's paint material
	for (uint i = 0; i < model->_meshes.size(); i++) {
		const Mesh3D *mesh = model->_meshes[i];
		if (!mesh || i >= car->_meshVisible.size() || !car->_meshVisible[i])
			continue;
		const Common::Array<const Material3D *> &mats = car->_polyMaterials[i];
		for (uint k = 0; k < mesh->polys.size(); k++) {
			if (k >= mats.size() || mats[k] != &car->_paintMaterial)
				continue;
			const MeshPoly &poly = mesh->polys[k];
			if (mesh->local.transformDir(poly.normal).dot(camFwd) > 0.0f)
				continue;
			if (_tris.size() >= kMaxTris)
				break;
			Tri t;
			for (int v = 0; v < 3; v++) {
				const Vec3 w = mesh->local.transformPoint(mesh->vertices[poly.v[v]].pos);
				project(w, t.x[v], t.y[v]);
				minX = MIN(minX, t.x[v]);
				maxX = MAX(maxX, t.x[v]);
				minY = MIN(minY, t.y[v]);
				maxY = MAX(maxY, t.y[v]);
				t.d[v] = (w - camPos).length();
			}
			t.area2 = (t.y[2] - t.y[0]) * (t.x[1] - t.x[0]) - (t.y[1] - t.y[0]) * (t.x[2] - t.x[0]);
			t.mesh = i;
			t.poly = k;
			_tris.push_back(t);
		}
	}
	// 12x12 cells over the projected bounds with a one-cell margin
	_cellW = (maxX - minX) / (float)(kGridSize - 2);
	_cellH = (maxY - minY) / (float)(kGridSize - 2);
	_originX = minX - _cellW;
	_originY = minY - _cellH;
	if (_tris.empty() || !(_cellW > 0.0f) || !(_cellH > 0.0f))
		return;
	for (uint i = 0; i < _tris.size(); i++) {
		// TriGrid_insert (0x412058): every cell the triangle's bounds touch
		const Tri &t = _tris[i];
		float bx0 = 999.0f, by0 = 999.0f, bx1 = -999.0f, by1 = -999.0f;
		for (int v = 0; v < 3; v++) {
			bx0 = MIN(bx0, t.x[v]);
			bx1 = MAX(bx1, t.x[v]);
			by0 = MIN(by0, t.y[v]);
			by1 = MAX(by1, t.y[v]);
		}
		bx0 = (bx0 - _originX) / _cellW;
		by0 = (by0 - _originY) / _cellH;
		bx1 = (bx1 - _originX) / _cellW;
		by1 = (by1 - _originY) / _cellH;
		int cy = ftol(by0);
		do {
			int cx = ftol(bx0);
			do {
				if (cx >= 0 && cx < kGridSize && cy >= 0 && cy < kGridSize)
					_cells[cy][cx].push_back(&t);
				cx++;
			} while ((float)cx < bx1);
			cy++;
		} while ((float)cy < by1);
	}
	_gridValid = true;
}

void SprayCan::updateGrid() {
	// Rebuilt when the camera turned or zoomed since the last stroke (the target is not compared)
	const CarView &cv = g_engine->_workShop->_carView;
	const bool moved = (cv.angles() - _lastAngles).length() > 0.0001f || fabs((double)(_lastDist - cv.dist())) > 0.001 ||
	                   (g_engine->fixOriginalBugs() && (cv.target() - _lastTarget).length() > 0.0001f) || !_gridValid;
	if (moved)
		buildGrid();
	_lastAngles = cv.angles();
	_lastDist = cv.dist();
	_lastTarget = cv.target();
}

void SprayCan::paintAt(float x, float y, int radius, const Vec3 &colour) {
	if (!_gridValid)
		return;
	// TriGrid_cellAt (0x4122a8), from the first sample only
	const int ix = ftol(x), iy = ftol(y);
	if ((float)ix < _originX || (float)iy < _originY)
		return;
	const int cx = ftol(ftol(ix - _originX) / _cellW), cy = ftol(ftol(iy - _originY) / _cellH);
	if (cx >= kGridSize || cy >= kGridSize || cx < 0 || cy < 0)
		return;
	const Common::Array<const Tri *> &cell = _cells[cy][cx];
	if (cell.empty())
		return;
	CarInstance *car = g_engine->_mechanic->_car;
	const CarModel *model = car->model();
	const int texW = car->_paintTexture.size, texH = car->_paintTexture.size;
	// Two samples, (x, y) and (x + 0.5, y + 0.5); the hits of the first one are kept for the second
	Common::Array<Hit> hits;
	for (int s = 0; s < 2; s++) {
		for (uint k = 0; k < cell.size() && k < 0x41; k++) {
			const Tri &t = *cell[k];
			// Tri2D_barycentric (0x4136ba)
			Hit h;
			h.b[1] = ((t.y[0] - y) * (t.x[2] - x) - (t.y[2] - y) * (t.x[0] - x)) / t.area2;
			if (!(h.b[1] >= 0.0f && h.b[1] <= 1.0f))
				continue;
			h.b[2] = ((t.y[1] - y) * (t.x[0] - x) - (t.y[0] - y) * (t.x[1] - x)) / t.area2;
			if (!(h.b[2] >= 0.0f && h.b[2] <= 1.0f))
				continue;
			h.b[0] = 1.0f - (h.b[1] + h.b[2]);
			if (!(h.b[0] >= 0.0f && h.b[0] <= 1.0f))
				continue;
			h.depth = h.b[2] * t.d[2] + h.b[1] * t.d[1] + h.b[0] * t.d[0];
			h.tri = &t;
			hits.push_back(h);
		}
		if (hits.empty())
			return;
		uint best = 0;
		for (uint i = 0; i < hits.size(); i++)
			if (hits[i].depth <= hits[best].depth)
				best = i;
		const Hit &h = hits[best];
		const MeshPoly &poly = model->_meshes[h.tri->mesh]->polys[h.tri->poly];
		float u = poly.uv[0][0] * h.b[0] + poly.uv[1][0] * h.b[1] + poly.uv[2][0] * h.b[2];
		float v = poly.uv[0][1] * h.b[0] + poly.uv[1][1] * h.b[1] + poly.uv[2][1] * h.b[2];
		if (u < -1.0f)
			u = -1.0f;
		if (v < -1.0f)
			v = -1.0f;
		if (u < 0.0f)
			u += 1.0f;
		if (v < 0.0f)
			v += 1.0f;
		if (u > 1.0f)
			u = 1.0f;
		if (v > 1.0f)
			v = 1.0f;
		const int tx = ftol(u * texW), ty = ftol(v * texH);
		for (int dy = -radius; dy < radius + 1; dy++)
			for (int dx = -radius; dx < radius + 1; dx++)
				if (tx + dx >= 0 && tx + dx < texW && ty + dy >= 0 && ty + dy < texH)
					car->paintTexel(tx + dx, ty + dy, colour);
		x += 0.5f;
		y += 0.5f;
	}
}

void SprayCan::paintDone() {
	g_engine->_workShop->_carView.setDirty(true);
}

bool SprayCan::debugPaintPos(int n, Common::Point &out) {
	updateGrid();
	Common::Array<int> order;
	for (uint i = 0; i < _tris.size(); i++)
		order.push_back(i);
	for (uint i = 0; i < order.size(); i++)
		for (uint j = i + 1; j < order.size(); j++)
			if (fabsf(_tris[order[j]].area2) > fabsf(_tris[order[i]].area2))
				SWAP(order[i], order[j]);
	if (n < 0 || n >= (int)order.size())
		return false;
	const Tri &t = _tris[order[n]];
	out = Common::Point((int)((t.x[0] + t.x[1] + t.x[2]) / 3), (int)((t.y[0] + t.y[1] + t.y[2]) / 3));
	return true;
}

void SprayCan::debugDump() {
	updateGrid();
	int n = 0;
	for (int y = 0; y < kGridSize; y++)
		for (int x = 0; x < kGridSize; x++)
			n += _cells[y][x].size();
	debug("dump spray: %d triangles, origin (%.1f %.1f) cell %.1f x %.1f, %d cell entries, valid %d", _tris.size(), _originX,
	      _originY, _cellW, _cellH, n, _gridValid);
	for (uint i = 0; i < _tris.size() && i < 5; i++)
		debug("dump spray tri %d: (%.1f %.1f) (%.1f %.1f) (%.1f %.1f) area %.1f", i, _tris[i].x[0], _tris[i].y[0], _tris[i].x[1],
		      _tris[i].y[1], _tris[i].x[2], _tris[i].y[2], _tris[i].area2);
	const Common::Point c(195, 200);
	Common::Point sc;
	const CarView &cv = g_engine->_workShop->_carView;
	if (!_tris.empty()) {
		const Tri &t = _tris[0];
		CarInstance *car = g_engine->_mechanic->_car;
		const Mesh3D *m = car->model()->_meshes[t.mesh];
		const Vec3 w = m->local.transformPoint(m->vertices[m->polys[t.poly].v[0]].pos);
		cv.project(w, sc);
		debug("dump spray tri 0 vertex 0 renderer (%d %d) projector (%.1f %.1f)", sc.x, sc.y, t.x[0], t.y[0]);
	}
	float u0 = 9, u1 = -9, v0 = 9, v1 = -9;
	CarInstance *car = g_engine->_mechanic->_car;
	for (uint i = 0; i < _tris.size(); i++) {
		const MeshPoly &poly = car->model()->_meshes[_tris[i].mesh]->polys[_tris[i].poly];
		for (int k = 0; k < 3; k++) {
			u0 = MIN(u0, poly.uv[k][0]);
			u1 = MAX(u1, poly.uv[k][0]);
			v0 = MIN(v0, poly.uv[k][1]);
			v1 = MAX(v1, poly.uv[k][1]);
		}
	}
	debug("dump spray uv range u %.3f..%.3f v %.3f..%.3f, texture %d", u0, u1, v0, v1, car->_paintTexture.size);
	{
		const int cx = ftol(ftol(c.x - _originX) / _cellW), cy = ftol(ftol(c.y - _originY) / _cellH);
		debug("dump spray cell (%d %d) has %d", cx, cy, (cx >= 0 && cy >= 0 && cx < 12 && cy < 12) ? _cells[cy][cx].size() : -1);
		int inside = 0;
		for (uint i = 0; i < _tris.size(); i++) {
			const Tri &t = _tris[i];
			const float b1 = ((t.y[0] - c.y) * (t.x[2] - c.x) - (t.y[2] - c.y) * (t.x[0] - c.x)) / t.area2;
			const float b2 = ((t.y[1] - c.y) * (t.x[0] - c.x) - (t.y[0] - c.y) * (t.x[1] - c.x)) / t.area2;
			if (b1 >= 0 && b1 <= 1 && b2 >= 0 && b2 <= 1 && b1 + b2 <= 1)
				inside++;
		}
		debug("dump spray triangles containing the point: %d", inside);
		int painted = 0, total = 0;
		for (uint i = 0; i < car->_polyMaterials.size(); i++)
			for (uint k = 0; k < car->_polyMaterials[i].size(); k++) {
				total++;
				if (car->_polyMaterials[i][k] == &car->_paintMaterial)
					painted++;
			}
		debug("dump spray paint polys %d of %d", painted, total);
		int best[3] = { -1, -1, -1 };
		for (uint i = 0; i < _tris.size(); i++) {
			const float a = fabsf(_tris[i].area2);
			for (int k = 0; k < 3; k++) {
				if (best[k] < 0 || a > fabsf(_tris[best[k]].area2)) {
					for (int j = 2; j > k; j--)
						best[j] = best[j - 1];
					best[k] = i;
					break;
				}
			}
		}
		for (int k = 0; k < 3; k++)
			if (best[k] >= 0) {
				const Tri &t = _tris[best[k]];
				debug("dump spray big tri %d at (%.0f %.0f) area %.0f", best[k], (t.x[0] + t.x[1] + t.x[2]) / 3,
				      (t.y[0] + t.y[1] + t.y[2]) / 3, t.area2);
			}
	}
}

void SprayCan::onLeftDown(const Common::Point &p) {
	if (!_decal)
		return;
	updateGrid();
	// Every stamp pixel that is not near-black is painted, centred on the pointer
	const DecalEdit &e = *_decal;
	for (int j = 0; j < e._sh; j++)
		for (int i = 0; i < e._sw; i++) {
			const byte *px = &e._scaled[(j * e._sw + i) * 3];
			if ((float)(px[0] + px[1] + px[2]) / 3.0f >= 8.0f)
				paintAt(((float)p.x - (float)e._sw * 0.5f) + (float)i, ((float)p.y - (float)e._sh * 0.5f) + (float)j, 0,
				        Vec3(px[0] / 256.0f, px[1] / 256.0f, px[2] / 256.0f));
		}
	// One copy is used even if nothing was hit; the last one ends decal mode until the button is released
	OwnedDecal *od = _decal->_owned;
	if (od && !g_engine->_mechanic->useDecal(od)) {
		_decal->_owned = nullptr; // the original keeps a dangling reference
		_blocked = true;
		clearDecal();
		g_engine->_workShop->updateToolCursor(p);
	}
	paintDone();
}

void SprayCan::onLeftHeld(const Common::Point &p) {
	Mechanic *m = g_engine->_mechanic;
	if (!m->_car || !_brush || _blocked || _decal)
		return;
	if (!_spray->isPlaying())
		_spray->play();
	CarInstance *car = m->_car;
	if (!_fill) {
		updateGrid();
		// Every dark pixel of the 24x24 brush paints a (2r+1)^2 texel square
		for (int j = 0; j < _brush->_maskH; j++)
			for (int i = 0; i < _brush->_maskW; i++)
				if (_brush->_mask[j * _brush->_maskW + i])
					paintAt(((float)p.x - (float)_brush->_maskW * 0.5f) + (float)i,
					        ((float)p.y - (float)_brush->_maskH * 0.5f) + (float)j, _radius, _colour);
	} else {
		// "Part": the UV box of the paintable faces of the part under the pointer, 16 texels larger
		CarView &cv = g_engine->_workShop->_carView;
		Vec3 o, d;
		cv.pickRay(p, o, d);
		const PickHit hit = cv.pick(o, d, false);
		if (hit.mesh < 0)
			return;
		const Mesh3D &mesh = *car->model()->_meshes[hit.mesh];
		const Common::Array<const Material3D *> &mats = car->_polyMaterials[hit.mesh];
		float u0 = 999.0f, v0 = 999.0f, u1 = -999.0f, v1 = -999.0f;
		for (uint k = 0; k < mesh.polys.size(); k++) {
			if (k >= mats.size() || mats[k] != &car->_paintMaterial)
				continue;
			for (int v = 0; v < 3; v++) {
				u0 = MIN(u0, mesh.polys[k].uv[v][0]);
				u1 = MAX(u1, mesh.polys[k].uv[v][0]);
				v0 = MIN(v0, mesh.polys[k].uv[v][1]);
				v1 = MAX(v1, mesh.polys[k].uv[v][1]);
			}
		}
		const int size = car->_paintTexture.size;
		debugC(2, kDebugGame, "Spray Can: part fill of mesh %d '%s', uv %.3f,%.3f - %.3f,%.3f", hit.mesh, mesh.name.c_str(), u0, v0,
		       u1, v1);
		int left = ftol(u0 * size) - 16, top = ftol(v0 * size) - 16;
		int right = ftol(u1 * size) + 16, bottom = ftol(v1 * size) + 16;
		left = MAX(left, 0);
		top = MAX(top, 0);
		right = MIN(right, size);
		bottom = MIN(bottom, size);
		for (int y = top; y < bottom; y++)
			for (int x = left; x < right; x++)
				car->paintTexel(x, y, _colour);
	}
	paintDone();
}

// ---------------------------------------------------------------- PaintPane

PaintPane::PaintPane() : _selectedBrush(nullptr), _selectedColour(nullptr) {
	setRect(387, 227, 506, 362);
	_decals.setOnClick(makeCallback(this, &PaintPane::onDecals));
	addChild(&_decals);
}

PaintPane::~PaintPane() {
	for (uint i = 0; i < _brushes.size(); i++)
		delete _brushes[i];
	for (uint i = 0; i < _colours.size(); i++)
		delete _colours[i];
}

void PaintPane::init() {
	Resources *res = g_engine->_res;
	// Decals button 80x20 at (left + 19, bottom - 23); disabled = the workshop background
	_decals.setImages(res->getImage("Overlays\\DecalsUP.tga"), res->getImage("Overlays\\DecalsDN.tga"),
	                  res->getImage("Overlays\\DecalsDN.tga"));
	_decals.setPos(_rect.left + 0x13, _rect.bottom - 0x17);
	Graphics::ManagedSurface *off = cropWorkShop(_decals.rect());
	_decals.setImages(res->getImage("Overlays\\DecalsUP.tga"), res->getImage("Overlays\\DecalsDN.tga"), off);
	_decals.setPos(_rect.left + 0x13, _rect.bottom - 0x17);
	// Header (387,227)-(506,255): 4 brushes of 22x22, spacing 3, centred
	const Common::Rect header(_rect.left, _rect.top, _rect.right, _rect.top + 0x1c);
	const int size = header.height() - 3 * 2;
	int x = ftol(ftol(header.left + header.width() * 0.5) - (size + 3) * 0.5 * 4.0);
	static const char *const kBrushes[4] = { "Small.tga", "Medium.tga", "Large.tga", "Part.tga" };
	static const int kRadius[4] = { 0, 1, 2, 0 };
	for (int i = 0; i < 4; i++) {
		BrushButton *b = new BrushButton();
		b->setRadio(this, 1, makeCallback(this, &PaintPane::onBrush));
		addChild(b);
		const Graphics::ManagedSurface *img = res->getImage(Common::String("Pointers\\Brushes\\") + kBrushes[i]);
		if (img)
			b->init(Common::Rect(x, header.top + 3, x + size, header.top + 3 + size), *img, kRadius[i]);
		b->_fill = i == 3;
		_brushes.push_back(b);
		x += size + 3;
	}
	// List (387,255)-(506,342): 27 swatches 3 x 9, margin 2
	const Common::Rect list(_rect.left, header.bottom, _rect.right, _rect.bottom - 0x14);
	const int cw = (list.width() - 2 * 4) / 3, ch = (list.height() - 2 * 10) / 9;
	for (int i = 0; i < 27; i++) {
		ColorButton *c = new ColorButton();
		c->setRadio(this, 2, makeCallback(this, &PaintPane::onColor));
		const int col = i % 3, row = i / 3;
		const int cx = list.left + 2 + col * (cw + 2), cy = list.top + 2 + row * (ch + 2);
		c->init(Common::Rect(cx, cy, cx + cw, cy + ch), paintColor(i));
		_colours.push_back(c);
		addChild(c);
	}
}

void PaintPane::enable() {
	SprayCan *can = g_engine->_workShop->_sprayCan;
	// Quirk: the first brush and colour are taken, but no button shows as selected
	if (!can->_brush && !_brushes.empty()) {
		can->_brush = _brushes[0];
		can->_colour = _colours[0]->_colour;
	}
	for (uint i = 0; i < _brushes.size(); i++)
		_brushes[i]->setStateSilently(_brushes[i] == _selectedBrush ? Button::kStateDown : Button::kStateUp);
	for (uint i = 0; i < _colours.size(); i++)
		_colours[i]->setStateSilently(_colours[i] == _selectedColour ? Button::kStateDown : Button::kStateUp);
	_decals.setStateSilently(Button::kStateUp);
}

void PaintPane::disable() {
	for (uint i = 0; i < _brushes.size(); i++)
		_brushes[i]->setStateSilently(Button::kStateDisabled);
	for (uint i = 0; i < _colours.size(); i++)
		_colours[i]->setStateSilently(Button::kStateDisabled);
	_decals.setStateSilently(Button::kStateDisabled);
	g_engine->_workShop->_sprayCan->clearDecal();
}

void PaintPane::onLHeld(const Common::Point &p) {
	g_engine->_workShop->_sprayCan->clearDecal();
}

void PaintPane::onDecals(Button *b) {
	g_engine->_workShop->_decalBrowser->openBrowser();
}

void PaintPane::onBrush(Button *b) {
	SprayCan *can = g_engine->_workShop->_sprayCan;
	BrushButton *bb = static_cast<BrushButton *>(b);
	can->_brush = bb;
	can->_radius = bb->_radius;
	can->_fill = bb->_fill;
	can->clearDecal();
	_selectedBrush = bb;
}

void PaintPane::onColor(Button *b) {
	SprayCan *can = g_engine->_workShop->_sprayCan;
	can->clearDecal();
	can->_colour = static_cast<ColorButton *>(b)->_colour;
	_selectedColour = static_cast<ColorButton *>(b);
}

// ---------------------------------------------------------------- DecalBrowser

DecalBrowser::DecalBrowser() : _page2(nullptr) {
	_takeFocus = true;
	_done.setOnClick(makeCallback(this, &DecalBrowser::onDone));
	_cancel.setOnClick(makeCallback(this, &DecalBrowser::onCancel));
	_up.setOnClick(makeCallback(this, &DecalBrowser::onUp));
	_down.setOnClick(makeCallback(this, &DecalBrowser::onDown));
	_turnRight.setOnClick(makeCallback(this, &DecalBrowser::onTurnRight));
	_turnLeft.setOnClick(makeCallback(this, &DecalBrowser::onTurnLeft));
	_mirror.setOnClick(makeCallback(this, &DecalBrowser::onMirror));
	_flip.setOnClick(makeCallback(this, &DecalBrowser::onFlip));
	_lastUsed.setOnClick(makeCallback(this, &DecalBrowser::onLastUsed));
	_back.setOnClick(makeCallback(this, &DecalBrowser::onBack));
	_bigger.setRadio(this, 1, makeCallback(this, &DecalBrowser::onBigger));
	_normal.setRadio(this, 1, makeCallback(this, &DecalBrowser::onNormal));
	_smaller.setRadio(this, 1, makeCallback(this, &DecalBrowser::onSmaller));
}

DecalBrowser::~DecalBrowser() {
	for (uint i = 0; i < _swatches.size(); i++)
		delete _swatches[i];
}

void DecalBrowser::init() {
	Resources *res = g_engine->_res;
	setFrame("DBrowser1.tga");
	setPos(120, 90);
	_page2 = res->getImage("DBrowser2UP.tga");
	const Common::Point o(_frameRect.left, _frameRect.top);
	// The buttons are cut out of the 400x300 pages and moved to the dialog
	struct Def {
		ImageButton *b;
		int page;
		Common::Rect r;
	};
	const Def defs[] = {
		{ &_cancel, 1, Common::Rect(301, 261, 389, 294) }, { &_up, 1, Common::Rect(367, 49, 389, 82) },
		{ &_down, 1, Common::Rect(367, 216, 389, 249) }, { &_lastUsed, 1, Common::Rect(8, 266, 72, 290) },
		{ &_mirror, 2, Common::Rect(43, 221, 107, 243) }, { &_flip, 2, Common::Rect(19, 154, 41, 218) },
		{ &_turnRight, 2, Common::Rect(148, 65, 196, 107) }, { &_turnLeft, 2, Common::Rect(19, 65, 67, 107) },
		{ &_done, 2, Common::Rect(195, 261, 283, 294) }, { &_back, 2, Common::Rect(8, 266, 72, 290) },
		{ &_bigger, 2, Common::Rect(174, 131, 227, 153) }, { &_normal, 2, Common::Rect(174, 177, 227, 199) },
		{ &_smaller, 2, Common::Rect(174, 221, 227, 243) }
	};
	const Graphics::ManagedSurface &up1 = *res->getImage("DBrowser1UP.tga"), &dn1 = *res->getImage("DBrowser1DN.tga");
	const Graphics::ManagedSurface &up2 = *res->getImage("DBrowser2UP.tga"), &dn2 = *res->getImage("DBrowser2DN.tga");
	for (uint i = 0; i < ARRAYSIZE(defs); i++) {
		const Def &d = defs[i];
		if (d.page == 1)
			d.b->setFromSheets(d.r, up1, dn1, up1);
		else
			d.b->setFromSheets(d.r, up2, dn2, up2);
		d.b->setPos(d.r.left + o.x, d.r.top + o.y);
	}
	_preview = Common::Rect(60 + o.x, 107 + o.y, 152 + o.x, 199 + o.y);
	// The owned decals, 6 x 2 per page, on a cut of the frame
	const Common::Rect lr(o.x + 13, o.y + 49, o.x + 363, o.y + 249);
	_list.layout(lr, makeCallback(this, &DecalBrowser::onCell));
	if (_frame) {
		Graphics::ManagedSurface *bg = new Graphics::ManagedSurface(lr.width(), lr.height(), _frame->format);
		bg->simpleBlitFrom(*_frame, Common::Rect(13, 49, 363, 249), Common::Point(0, 0));
		_list.setBackground(bg);
	}
	// 27 swatches in (255,66)-(379,239), gap 3, 3 x 9
	const Common::Rect area(255 + o.x, 66 + o.y, 379 + o.x, 239 + o.y);
	const int cw = (area.width() - 3 * 4) / 3, ch = (area.height() - 3 * 10) / 9;
	for (int i = 0; i < 27; i++) {
		ColorButton *c = new ColorButton();
		c->setOnClick(makeCallback(this, &DecalBrowser::onSwatch));
		const int col = i % 3, row = i / 3;
		const int x = area.left + 3 + col * (cw + 3), y = area.top + 3 + row * (ch + 3);
		c->init(Common::Rect(x, y, x + cw, y + ch), paintColor(i));
		_swatches.push_back(c);
	}
}

void DecalBrowser::openBrowser() {
	Mechanic *m = g_engine->_mechanic;
	_list.setOwned(&m->_decals);
	showChooser();
	open();
	if (m->_decals.empty())
		g_engine->_fonts[kFontStandard].drawText(*g_engine->_gfx,
		    "You don't have any Decals! Go to the Decals section of the Catalog to purchase some.", _list.rect(), kAlignCenter,
		    kAlignCenter, kKeyDefault, kPageFront);
}

void DecalBrowser::showChooser() {
	Common::Array<Widget *> own, extra;
	own.push_back(&_list);
	own.push_back(&_cancel);
	own.push_back(&_up);
	own.push_back(&_down);
	// "Last Used" once a decal has been picked
	if (_edit._owned)
		own.push_back(&_lastUsed);
	setButtons(own, extra);
}

void DecalBrowser::showModify() {
	Common::Array<Widget *> own, extra;
	own.push_back(&_done);
	own.push_back(&_cancel);
	own.push_back(&_mirror);
	own.push_back(&_flip);
	own.push_back(&_turnRight);
	own.push_back(&_turnLeft);
	own.push_back(&_back);
	own.push_back(&_bigger);
	own.push_back(&_normal);
	own.push_back(&_smaller);
	// Recolourable decals get the swatches
	if (_edit._owned && _edit._owned->data && _edit._owned->data->colorType != 0)
		for (uint i = 0; i < _swatches.size(); i++)
			own.push_back(_swatches[i]);
	setButtons(own, extra);
	if (_page2)
		g_engine->_gfx->blit(kPageFront, *_page2, _frameRect.left, _frameRect.top);
	_bigger.setStateSilently(Button::kStateUp);
	_normal.setStateSilently(Button::kStateUp);
	_smaller.setStateSilently(Button::kStateUp);
	if (_edit._scale > 1.01f)
		_bigger.setStateSilently(Button::kStateDown);
	else if (_edit._scale >= 0.99f)
		_normal.setStateSilently(Button::kStateDown);
	else
		_smaller.setStateSilently(Button::kStateDown);
	refresh();
	drawPreview();
}

void DecalBrowser::drawPreview() {
	Gfx *gfx = g_engine->_gfx;
	// The preview background is the page under it
	if (_page2)
		gfx->blitRect(kPageFront, *_page2, Common::Rect(_preview.left - _frameRect.left, _preview.top - _frameRect.top,
		              _preview.right - _frameRect.left, _preview.bottom - _frameRect.top), _preview.left, _preview.top);
	if (_edit._w <= 0 || _edit._h <= 0)
		return;
	// The work image resized to 92x92 (aspect not kept), keyed
	Common::Array<byte> rgba(_edit._w * _edit._h * 4), out;
	for (int i = 0; i < _edit._w * _edit._h; i++) {
		for (int c = 0; c < 3; c++)
			rgba[i * 4 + c] = _edit._work[i * 3 + c];
		rgba[i * 4 + 3] = 255;
	}
	resizeRGBA(_edit._w, _edit._h, rgba, _preview.width(), _preview.height(), out);
	Graphics::ManagedSurface *img = toScreen(_preview.width(), _preview.height(), out, 4);
	gfx->blitKeyed(kPageFront, *img, Common::Rect(img->w, img->h), _preview.left, _preview.top);
	delete img;
}

void DecalBrowser::onDone(Button *b) {
	if (_edit._owned)
		g_engine->_workShop->_sprayCan->setDecal(&_edit);
	close();
}

void DecalBrowser::onCancel(Button *b) {
	g_engine->_workShop->_sprayCan->clearDecal();
	close();
}

void DecalBrowser::onUp(Button *b) {
	_list.prevPage();
}

void DecalBrowser::onDown(Button *b) {
	_list.nextPage();
}

void DecalBrowser::onMirror(Button *b) {
	_edit.flip(true);
	drawPreview();
}

void DecalBrowser::onFlip(Button *b) {
	_edit.flip(false);
	drawPreview();
}

void DecalBrowser::onTurnRight(Button *b) {
	_edit.rotate(true);
	drawPreview();
}

void DecalBrowser::onTurnLeft(Button *b) {
	_edit.rotate(false);
	drawPreview();
}

void DecalBrowser::onBigger(Button *b) {
	_edit._scale = 1.5f;
}

void DecalBrowser::onNormal(Button *b) {
	_edit._scale = 1.0f;
}

void DecalBrowser::onSmaller(Button *b) {
	_edit._scale = 0.5f;
}

void DecalBrowser::onLastUsed(Button *b) {
	if (_edit._owned)
		showModify();
}

void DecalBrowser::onBack(Button *b) {
	if (_frame)
		g_engine->_gfx->blit(kPageFront, *_frame, _frameRect.left, _frameRect.top);
	showChooser();
	refresh();
}

void DecalBrowser::onCell(Button *b) {
	DecalCell *c = static_cast<DecalCell *>(b);
	if (!c->_owned)
		return;
	_edit.setDecal(c->_owned);
	showModify();
}

void DecalBrowser::onSwatch(Button *b) {
	_edit.recolor(static_cast<ColorButton *>(b)->_colour);
	drawPreview();
}

} // End of namespace Gearhead
