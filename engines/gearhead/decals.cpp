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
#include "common/stream.h"
#include "graphics/managed_surface.h"

#include "gearhead/cardata.h"
#include "gearhead/decals.h"
#include "gearhead/font.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/resources.h"
#include "gearhead/tags.h"
#include "gearhead/tlv.h"

namespace Gearhead {

// ---------------------------------------------------------------- DecalLibrary

DecalLibrary::~DecalLibrary() {
	for (uint i = 0; i < _decals.size(); i++)
		delete _decals[i];
}

void DecalLibrary::loadAll(Resources &res) {
	const Common::StringArray files = res.list("Data\\Decals", "*.dpk");
	for (uint f = 0; f < files.size(); f++) {
		Common::SeekableReadStream *s = res.open("Data\\Decals\\" + files[f]);
		if (!s)
			continue;
		TLVChunk *root = TLVReader::parseFile(*s, kTag_DECALFILE);
		delete s;
		if (!root)
			continue;
		const Common::Array<const TLVChunk *> list = root->findAll(kTag_DECALDATA);
		for (uint i = 0; i < list.size(); i++) {
			const TLVChunk &c = *list[i];
			DecalData *d = new DecalData();
			d->id = c.getInt(kTag_DD_ID, d->id);
			d->name = c.getString(kTag_DD_NAME, d->name);
			d->numPerBuy = c.getInt(kTag_DD_NUMPERBUY, d->numPerBuy);
			d->cost = c.getFloat(kTag_DD_COST, d->cost);
			d->colorType = c.getInt(kTag_DD_COLORTYPE, d->colorType);
			if (const TLVChunk *bm = c.find(kTag_DD_IMAGE))
				decodeBitmapPixels(*bm, d->width, d->height, d->rgba);
			_decals.push_back(d);
		}
		delete root;
		debugC(1, kDebugGame, "Decals: %s, %d so far", files[f].c_str(), _decals.size());
	}
}

const DecalData *DecalLibrary::find(int id) const {
	for (uint i = 0; i < _decals.size(); i++)
		if (_decals[i]->id == id)
			return _decals[i];
	return nullptr;
}

// ---------------------------------------------------------------- DecalCell

DecalCell::DecalCell() : _owned(nullptr), _data(nullptr), _thumb(nullptr) {
}

DecalCell::~DecalCell() {
	delete _thumb;
}

void DecalCell::setRect(const Common::Rect &r) {
	// DecalCell_setRect (0x430687): the label is the 48x24 strip under the thumbnail
	_rect = r;
	_label = Common::Rect(r.left, r.top + 48, r.left + 48, r.top + 72);
}

void DecalCell::makeThumbnail(const DecalData *d) {
	delete _thumb;
	_thumb = nullptr;
	if (!d || d->rgba.empty())
		return;
	// clone, resize to 48x48 (whatever the aspect), convert to the screen format
	Common::Array<byte> px;
	resizeRGBA(d->width, d->height, d->rgba, 48, 48, px);
	const Graphics::PixelFormat &fmt = g_engine->_gfx->format();
	_thumb = new Graphics::ManagedSurface(48, 48, fmt);
	for (int y = 0; y < 48; y++)
		for (int x = 0; x < 48; x++) {
			const byte *p = &px[(y * 48 + x) * 4];
			_thumb->setPixel(x, y, fmt.RGBToColor(p[0], p[1], p[2]));
		}
}

void DecalCell::setData(const DecalData *d) {
	_data = d;
	_owned = nullptr;
	makeThumbnail(d);
}

void DecalCell::setOwned(OwnedDecal *od) {
	_owned = od;
	_data = nullptr;
	makeThumbnail(od ? od->data : nullptr);
}

void DecalCell::draw() {
	if (!_owned && !_data)
		return;
	Gfx *gfx = g_engine->_gfx;
	const BlitFont &font = g_engine->_fonts[kFontTinyWhite];
	if (_owned) {
		// copies in the top-left corner, the name under the thumbnail
		font.drawText(*gfx, Common::String::format("%i", _owned->copies), _rect, kAlignStart, kAlignStart, kKeyDefault, kPageFront);
		if (_owned->data)
			font.drawText(*gfx, _owned->data->name, _label, kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
	} else {
		font.drawText(*gfx, _data->name, _label, kAlignCenter, kAlignCenter, kKeyDefault, kPageFront);
	}
	if (_thumb)
		gfx->blitKeyed(kPageFront, *_thumb, Common::Rect(48, 48), _rect.left, _rect.top);
}

// ---------------------------------------------------------------- DecalList

DecalList::DecalList() : _background(nullptr), _owned(nullptr), _catalog(nullptr), _onClick(nullptr), _cols(1), _rows(1),
	_page(0), _lastPage(0) {
}

DecalList::~DecalList() {
	for (uint i = 0; i < _cells.size(); i++)
		delete _cells[i];
	delete _onClick;
	delete _background;
}

void DecalList::onCellClick(Button *b) {
	if (_onClick)
		(*_onClick)(b);
}

void DecalList::layout(const Common::Rect &r, ButtonCallback *onClick) {
	if (onClick != _onClick) {
		delete _onClick;
		_onClick = onClick;
	}
	_rect = r;
	const int w = r.width(), h = r.height();
	_cols = (int)(w / 56.0);
	_rows = (int)(h / 80.0);
	const int x0 = (int)(r.left + w / 2.0 - _cols * 28 - 4);
	const int y0 = (int)(r.top + h / 2.0 - _rows * 40 - 4);
	int x = x0 + 8, y = y0 + 8;
	for (int i = 0; i < _cols * _rows; i++) {
		DecalCell *c = new DecalCell();
		c->setRect(Common::Rect(x, y, x + 48, y + 72));
		c->setOnClick(makeCallback(this, &DecalList::onCellClick));
		// CellVec_setAt: the new cell takes slot i, a cell already there moves to the end
		if (i < (int)_cells.size()) {
			DecalCell *old = _cells[i];
			_cells[i] = c;
			_cells.push_back(old);
		} else {
			_cells.push_back(c);
		}
		addChild(c);
		x += 48 + 8;
		if ((w - 8) - 48 <= x - x0) {
			x = x0 + 8;
			y += 72 + 8;
		}
	}
	_page = 0;
}

void DecalList::layoutTwo(const Common::Rect &left, const Common::Rect &right, ButtonCallback *onClick) {
	layout(right, onClick);
	layout(left, onClick);
	_rows *= 2;
	_rect = Common::Rect(MIN(left.left, right.left), MIN(left.top, right.top), MAX(left.right, right.right),
	                     MAX(left.bottom, right.bottom));
}

int DecalList::count() const {
	if (_catalog)
		return _catalog->size();
	return _owned ? _owned->size() : 0;
}

void DecalList::setOwned(Common::Array<OwnedDecal> *list) {
	_owned = list;
	_catalog = nullptr;
	_page = 0;
	_lastPage = (count() - 1) / (_cols * _rows);
	fillCells();
}

void DecalList::setCatalog(const Common::Array<DecalData *> *list) {
	_catalog = list;
	_owned = nullptr;
	_page = 0;
	_lastPage = (count() - 1) / (_cols * _rows);
	fillCells();
}

void DecalList::fillCells() {
	const int start = _page * _cols * _rows;
	for (uint k = 0; k < _cells.size(); k++) {
		const int i = start + k;
		if (_catalog)
			_cells[k]->setData(i < (int)_catalog->size() ? (*_catalog)[i] : nullptr);
		else
			_cells[k]->setOwned(_owned && i < (int)_owned->size() ? &(*_owned)[i] : nullptr);
	}
}

bool DecalList::nextPage() {
	if (_page >= _lastPage)
		return false;
	_page++;
	fillCells();
	onRefresh();
	return true;
}

bool DecalList::prevPage() {
	if (_page <= 0)
		return false;
	_page--;
	fillCells();
	onRefresh();
	return true;
}

int DecalList::pageFlags() const {
	return (_page < _lastPage ? 1 : 0) | (_page > 0 ? 2 : 0);
}

void DecalList::setBackground(Graphics::ManagedSurface *bg) {
	delete _background;
	_background = bg;
}

void DecalList::onRefresh() {
	if (_background)
		g_engine->_gfx->blit(kPageFront, *_background, _rect.left, _rect.top);
	for (uint i = 0; i < _cells.size(); i++)
		_cells[i]->draw();
}

} // End of namespace Gearhead
