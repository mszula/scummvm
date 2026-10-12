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

#include "common/array.h"
#include "common/textconsole.h"
#include "graphics/managed_surface.h"

#include "gearhead/font.h"
#include "gearhead/gfx.h"
#include "gearhead/resources.h"

namespace Gearhead {

BlitFont::BlitFont() : _avgW(0), _lineH(0), _spaceW(6), _keyedByDefault(false) {
	for (int i = 0; i < 256; i++)
		_glyphs[i] = nullptr;
}

BlitFont::~BlitFont() {
	for (int i = 0; i < 256; i++)
		delete _glyphs[i];
}

bool BlitFont::load(Resources &res, const Common::String &path, const Common::String &charset,
                    int spaceWidth, bool invert, bool keyedByDefault, float threshold) {
	const Graphics::PixelFormat rgb24 = Graphics::PixelFormat::createFormatRGB24();
	Graphics::ManagedSurface *img = res.loadImage(path, &rgb24);
	if (!img)
		error("ERROR: Couldn't load font file: %s", path.c_str());

	_spaceW = spaceWidth;
	_keyedByDefault = keyedByDefault;

	// v(x) = max over rows of the (optionally inverted) brightness
	Common::Array<float> column(img->w);
	for (int x = 0; x < img->w; x++) {
		float best = 0.0f;
		for (int y = 0; y < img->h; y++) {
			const byte *p = (const byte *)img->getBasePtr(x, y);
			float v = (float)(p[0] + p[1] + p[2]) / 765.0f;
			if (invert)
				v = 1.0f - v;
			if (v > best)
				best = v;
		}
		column[x] = best;
	}

	// Glyph runs, in order
	Common::Array<Common::Rect> runs;
	bool inside = false;
	int start = 0;
	for (int x = 0; x < img->w; x++) {
		if (!inside && column[x] >= threshold) {
			inside = true;
			start = x;
		} else if (inside && column[x] <= threshold) {
			inside = false;
			runs.push_back(Common::Rect(start - 1, 0, x + 1, img->h));
		}
	}

	if (runs.size() < charset.size())
		error("Font Error: Not enough chars in the font source image. Stopped at char %d (%c)", runs.size(), charset[runs.size()]);

	int sumW = 0, sumH = 0;
	for (uint i = 0; i < charset.size(); i++) {
		const Common::Rect &r = runs[i];
		// Crop; pixels outside the image are black
		Graphics::ManagedSurface crop(r.width(), r.height(), rgb24);
		memset(crop.getPixels(), 0, crop.pitch * crop.h); // fillRect does not support 3 bpp
		for (int y = 0; y < r.height(); y++) {
			for (int x = 0; x < r.width(); x++) {
				const int sx = r.left + x;
				if (sx < 0 || sx >= img->w)
					continue;
				memcpy(crop.getBasePtr(x, y), img->getBasePtr(sx, y), 3);
			}
		}
		Graphics::Surface *conv = crop.rawSurface().convertTo(res.screenFormat());
		Graphics::ManagedSurface *glyph = new Graphics::ManagedSurface();
		glyph->copyFrom(*conv);
		conv->free();
		delete conv;

		const byte c = (byte)charset[i];
		delete _glyphs[c];
		_glyphs[c] = glyph;
		sumW += r.width();
		sumH += r.height();
	}
	_avgW = sumW / (int)charset.size();
	_lineH = sumH / (int)charset.size();

	delete img;
	return true;
}

int BlitFont::charWidth(byte c) const {
	if (c == ' ' || c >= 0x80 || !_glyphs[c])
		return _spaceW;
	return _glyphs[c]->w;
}

int BlitFont::textWidth(const Common::String &text) const {
	int w = 0;
	for (uint i = 0; i < text.size(); i++)
		w += charWidth((byte)text[i]);
	return w;
}

static Common::String trimmed(Common::String s) {
	s.trim();
	return s;
}

void BlitFont::drawText(Gfx &gfx, const Common::String &text, const Common::Rect &r,
                        int hAlign, int vAlign, int keyMode, int pages) const {
	// Word wrap, exactly like the original (0x442940)
	Common::Array<Common::String> lines;
	Common::String rest = text;
	const int maxW = r.right - r.left;
	while (lines.size() < 20) {
		if (textWidth(rest) <= maxW) {
			lines.push_back(trimmed(rest));
			break;
		}
		Common::String tmp = rest;
		int i = rest.size();
		for (;;) {
			i--;
			if (textWidth(tmp) <= maxW || tmp.empty())
				break;
			tmp.deleteLastChar();
		}
		if (i < 0)
			i = 0;
		while (i != 0 && rest[i] != ' ' && rest[i] != '\n')
			i--;
		if (i < 0)
			i = 0;
		for (int k = 0; k < i; k++) {
			if (rest[k] == '\n') {
				i = k;
				break;
			}
		}
		lines.push_back(trimmed(Common::String(rest.c_str(), i + 1)));
		rest = Common::String(rest.c_str() + i + 1);
	}

	const int n = lines.size();
	int y;
	if (vAlign == kAlignEnd)
		y = r.bottom - _lineH * n;
	else if (vAlign == kAlignCenter)
		y = (int)((double)r.top + 0.5 * (double)(r.bottom - r.top) - 0.5 * (double)(_lineH * n));
	else
		y = r.top;

	const bool keyed = (keyMode == kKeyColor) || (keyMode == kKeyDefault && _keyedByDefault);

	for (int l = 0; l < n; l++) {
		const Common::String &line = lines[l];
		const int w = textWidth(line);
		int x;
		if (hAlign == kAlignEnd)
			x = r.right - w;
		else if (hAlign == kAlignCenter)
			x = (int)((double)r.left + 0.5 * (double)(r.right - r.left) - 0.5 * (double)w);
		else
			x = r.left;

		for (uint i = 0; i < line.size(); i++) {
			const byte c = (byte)line[i];
			const Graphics::ManagedSurface *g = (c != ' ' && c < 0x80) ? _glyphs[c] : nullptr;
			if (g) {
				if (keyed)
					gfx.blitKeyed(pages, *g, Common::Rect(g->w, g->h), x, y);
				else
					gfx.blit(pages, *g, x, y);
			}
			x += charWidth(c);
		}
		y += _lineH;
	}
}

bool loadGameFonts(Resources &res, BlitFont *fonts) {
	static const char *const kFull =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789~!@#$%^&*()_+|`-=\\{}:\"<>?[];',./";
	static const char *const kSignIn =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!$()-=\\:\"?,./";

	struct Def {
		const char *file;
		const char *charset;
		int space;
		bool invert;
		bool keyed;
		float threshold;
	};
	static const Def defs[kFontCount] = {
		{ "Standard.tga",    kFull,          6, false, true,  0.4f },
		{ "DialogFont.tga",  kFull,          6, true,  false, 0.6f },
		{ "DialogTitle.tga", kFull,          6, false, true,  0.4f },
		{ "CashFont.tga",    "0123456789.$", 2, false, true,  0.4f },
		{ "SignInFont.tga",  kSignIn,        6, true,  false, 0.4f },
		{ "TinyWhite.tga",   kFull,          6, false, false, 0.4f },
		{ "TinyBlack.tga",   kFull,          6, true,  false, 0.4f }
	};

	for (int i = 0; i < kFontCount; i++) {
		if (!fonts[i].load(res, Common::String("Data\\Gfx24\\Fonts\\") + defs[i].file, defs[i].charset,
		                   defs[i].space, defs[i].invert, defs[i].keyed, defs[i].threshold))
			return false;
	}
	return true;
}

} // End of namespace Gearhead
