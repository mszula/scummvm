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

#ifndef GEARHEAD_GFX_H
#define GEARHEAD_GFX_H

#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Gearhead {

enum {
	kScreenWidth = 640,
	kScreenHeight = 480
};

/**
 * Like the original video system the screen has two pages: page 1 is the
 * visible surface, page 0 keeps a clean copy of the static 2D UI (buttons and
 * backgrounds are always pasted to both), used to restore areas.
 */
enum PageMask {
	kPageBack = 1,
	kPageFront = 2,
	kPageBoth = kPageBack | kPageFront
};

class Gfx {
public:
	Gfx();
	~Gfx();

	void init(const Graphics::PixelFormat &format);
	const Graphics::PixelFormat &format() const { return _format; }

	Graphics::ManagedSurface &front() { return _pages[1]; }
	Graphics::ManagedSurface &back() { return _pages[0]; }

	/** Opaque copy (Screen_blit 0x45c6f3: no colour key, no alpha) */
	void blit(int pages, const Graphics::ManagedSurface &src, int x, int y);
	void blitRect(int pages, const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y);
	/** Colour-keyed copy: pixel value 0 (black in RGB565) is transparent */
	void blitKeyed(int pages, const Graphics::ManagedSurface &src, const Common::Rect &srcRect, int x, int y);
	void fill(int pages, const Common::Rect &r, uint32 color);
	void clear(int pages);
	/** Copy a rect of the back page to the front page */
	void restoreFromBack(const Common::Rect &r);

	/** Colour from 0..1 floats, like the original's Vec3 colours */
	uint32 rgb(float r, float g, float b) const;

	void setCursor(const Graphics::ManagedSurface *image, int hotspotX, int hotspotY);
	void showCursor(bool show);

	void present();
	bool saveScreenshot(const Common::String &hostPath);

private:
	Graphics::PixelFormat _format;
	Graphics::ManagedSurface _pages[2];
};

} // End of namespace Gearhead

#endif // GEARHEAD_GFX_H
