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

#include "common/file.h"
#include "common/system.h"
#include "graphics/cursorman.h"
#include "image/png.h"

#include "gearhead/gfx.h"

namespace Gearhead {

Gfx::Gfx() {
}

Gfx::~Gfx() {
	for (int i = 0; i < 2; i++)
		_pages[i].free();
}

void Gfx::init(const Graphics::PixelFormat &format) {
	_format = format;
	for (int i = 0; i < 2; i++) {
		_pages[i].create(kScreenWidth, kScreenHeight, format);
		_pages[i].clear(0);
	}
}

static bool clipBlit(const Graphics::ManagedSurface &dst, const Graphics::ManagedSurface &src, Common::Rect &srcRect, int &x, int &y) {
	srcRect.clip(Common::Rect(src.w, src.h));
	if (x < 0) {
		srcRect.left -= x;
		x = 0;
	}
	if (y < 0) {
		srcRect.top -= y;
		y = 0;
	}
	if (x + srcRect.width() > dst.w)
		srcRect.right = srcRect.left + dst.w - x;
	if (y + srcRect.height() > dst.h)
		srcRect.bottom = srcRect.top + dst.h - y;
	return srcRect.isValidRect() && !srcRect.isEmpty();
}

void Gfx::blit(int pages, const Graphics::ManagedSurface &src, int x, int y) {
	blitRect(pages, src, Common::Rect(src.w, src.h), x, y);
}

void Gfx::blitRect(int pages, const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y) {
	for (int i = 0; i < 2; i++) {
		if (!(pages & (1 << i)))
			continue;
		Common::Rect r = srcRect;
		int dx = x, dy = y;
		if (clipBlit(_pages[i], src, r, dx, dy))
			_pages[i].simpleBlitFrom(src, r, Common::Point(dx, dy));
	}
}

void Gfx::blitKeyed(int pages, const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y) {
	for (int i = 0; i < 2; i++) {
		if (!(pages & (1 << i)))
			continue;
		Common::Rect r = srcRect;
		int dx = x, dy = y;
		if (clipBlit(_pages[i], src, r, dx, dy))
			_pages[i].transBlitFrom(src, r, Common::Point(dx, dy), 0);
	}
}

void Gfx::fill(int pages, const Common::Rect &r, uint32 color) {
	for (int i = 0; i < 2; i++) {
		if (!(pages & (1 << i)))
			continue;
		Common::Rect c = r;
		c.clip(Common::Rect(kScreenWidth, kScreenHeight));
		if (c.isValidRect() && !c.isEmpty())
			_pages[i].fillRect(c, color);
	}
}

void Gfx::clear(int pages) {
	fill(pages, Common::Rect(kScreenWidth, kScreenHeight), 0);
}

void Gfx::restoreFromBack(const Common::Rect &r) {
	Common::Rect c = r;
	c.clip(Common::Rect(kScreenWidth, kScreenHeight));
	if (c.isValidRect() && !c.isEmpty())
		_pages[1].simpleBlitFrom(_pages[0], c, Common::Point(c.left, c.top));
}

uint32 Gfx::rgb(float r, float g, float b) const {
	return _format.RGBToColor((byte)CLIP<int>((int)(r * 255.0f), 0, 255),
	                          (byte)CLIP<int>((int)(g * 255.0f), 0, 255),
	                          (byte)CLIP<int>((int)(b * 255.0f), 0, 255));
}

void Gfx::setCursor(const Graphics::ManagedSurface *image, int hotspotX, int hotspotY) {
	if (image)
		CursorMan.replaceCursor(image->rawSurface(), hotspotX, hotspotY, 0);
}

void Gfx::showCursor(bool show) {
	CursorMan.showMouse(show);
}

void Gfx::present() {
	g_system->copyRectToScreen(_pages[1].getPixels(), _pages[1].pitch, 0, 0, kScreenWidth, kScreenHeight);
	g_system->updateScreen();
}

bool Gfx::saveScreenshot(const Common::String &hostPath) {
	Common::DumpFile f;
	if (!f.open(Common::Path(hostPath, '/'), true))
		return false;
	return Image::writePNG(f, _pages[1].rawSurface());
}

} // End of namespace Gearhead
