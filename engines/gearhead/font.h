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

#ifndef GEARHEAD_FONT_H
#define GEARHEAD_FONT_H

#include "common/rect.h"
#include "common/str.h"

namespace Graphics {
class ManagedSurface;
}

namespace Gearhead {

class Gfx;
class Resources;

enum TextAlign {
	kAlignStart = 0, ///< left / top
	kAlignEnd = 1,   ///< right / bottom
	kAlignCenter = 2
};

enum TextKeyMode {
	kKeyDefault = 0, ///< font default
	kKeyOpaque = 1,  ///< copy the whole glyph box
	kKeyColor = 2    ///< pixel value 0 is transparent
};

/**
 * Proportional bitmap font cut out of a strip image (BlitFont, blitfont.cpp).
 * Glyphs are found by scanning columns for brightness >= threshold; every
 * glyph gets one guard column on each side, there is no kerning.
 */
class BlitFont {
public:
	BlitFont();
	~BlitFont();

	bool load(Resources &res, const Common::String &path, const Common::String &charset,
	          int spaceWidth, bool invert, bool keyedByDefault, float threshold);

	int lineHeight() const { return _lineH; }
	int averageWidth() const { return _avgW; }
	int textWidth(const Common::String &text) const;

	/** BlitFont_drawText (0x442940), including the word-wrap quirks of the original */
	void drawText(Gfx &gfx, const Common::String &text, const Common::Rect &r,
	              int hAlign, int vAlign, int keyMode, int pages) const;

private:
	int charWidth(byte c) const;

	Graphics::ManagedSurface *_glyphs[256];
	int _avgW;
	int _lineH;
	int _spaceW;
	bool _keyedByDefault;
};

/** The fonts of Fonts_initBlitFonts (0x441e00) */
enum FontId {
	kFontStandard = 0,
	kFontDialog,
	kFontDialogTitle,
	kFontCash,
	kFontSignIn,
	kFontTinyWhite,
	kFontTinyBlack,
	kFontCount
};

bool loadGameFonts(Resources &res, BlitFont *fonts);

} // End of namespace Gearhead

#endif // GEARHEAD_FONT_H
