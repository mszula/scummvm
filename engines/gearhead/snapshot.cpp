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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/hashmap.h"
#include "common/savefile.h"
#include "common/system.h"
#include "graphics/managed_surface.h"
#include "image/bmp.h"
#include "image/png.h"

#include "gearhead/car.h"
#include "gearhead/cardata.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/snapshot.h"
#include "gearhead/sound.h"

namespace Gearhead {

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

/**
 * Bitmap_convert(8) for the BMP files: an adaptive palette of the 256 most
 * used colours (the original uses NeuQuant), every pixel mapped to the nearest
 */
static void quantize8(int w, int h, const Common::Array<byte> &rgba, Graphics::Surface &out, byte *palette) {
	Common::HashMap<uint32, uint32> counts;
	for (int i = 0; i < w * h; i++) {
		const byte *p = &rgba[i * 4];
		counts[(uint32)(p[0] >> 3) << 10 | (uint32)(p[1] >> 3) << 5 | (p[2] >> 3)]++;
	}
	Common::Array<uint32> keys;
	for (Common::HashMap<uint32, uint32>::const_iterator it = counts.begin(); it != counts.end(); ++it)
		keys.push_back(it->_key);
	// The most frequent colours first
	Common::sort(keys.begin(), keys.end(), [&counts](uint32 a, uint32 b) {
		return counts[a] != counts[b] ? counts[a] > counts[b] : a < b;
	});
	const uint n = MIN<uint>(keys.size(), 256);
	memset(palette, 0, 256 * 3);
	for (uint i = 0; i < n; i++) {
		palette[i * 3] = (byte)(((keys[i] >> 10) & 31) << 3 | 4);
		palette[i * 3 + 1] = (byte)(((keys[i] >> 5) & 31) << 3 | 4);
		palette[i * 3 + 2] = (byte)((keys[i] & 31) << 3 | 4);
	}
	out.create(w, h, Graphics::PixelFormat::createFormatCLUT8());
	Common::HashMap<uint32, byte> cache;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const byte *p = &rgba[(y * w + x) * 4];
			const uint32 key = (uint32)(p[0] >> 3) << 10 | (uint32)(p[1] >> 3) << 5 | (p[2] >> 3);
			Common::HashMap<uint32, byte>::const_iterator c = cache.find(key);
			byte best = 0;
			if (c != cache.end()) {
				best = c->_value;
			} else {
				int bestD = 0x7fffffff;
				for (uint i = 0; i < n; i++) {
					const int dr = palette[i * 3] - p[0], dg = palette[i * 3 + 1] - p[1], db = palette[i * 3 + 2] - p[2];
					const int d = dr * dr + dg * dg + db * db;
					if (d < bestD) {
						bestD = d;
						best = (byte)i;
					}
				}
				cache[key] = best;
			}
			*(byte *)out.getBasePtr(x, y) = best;
		}
}

CameraTool::CameraTool() : _sound(nullptr), _logo(nullptr) {
}

CameraTool::~CameraTool() {
	delete _logo;
}

void CameraTool::init() {
	Resources *res = g_engine->_res;
	setCursor("Pointers\\camera.tga");
	// Overlays\MekadaLogo.tga (128x213) becomes a 256x256 texture drawn at half size, (5,5)-(69,111)
	const Graphics::ManagedSurface *img = res->getImage("Overlays\\MekadaLogo.tga");
	if (img) {
		const int w = (int)(img->w * 0.5), h = (int)(img->h * 0.5);
		Common::Array<byte> src, tex, small;
		surfaceToRGBA(*img, src);
		resizeRGBA(img->w, img->h, src, 256, 256, tex);
		resizeRGBA(256, 256, tex, w, h, small);
		const Graphics::PixelFormat &fmt = g_engine->_gfx->format();
		_logo = new Graphics::ManagedSurface(w, h, fmt);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				const byte *p = &small[(y * w + x) * 4];
				_logo->setPixel(x, y, fmt.RGBToColor(p[0], p[1], p[2]));
			}
	}
	_sound = g_engine->_snd->get("misc\\camera.wav");
}

void CameraTool::flashWhite() {
	Gfx *gfx = g_engine->_gfx;
	gfx->fill(kPageFront, Common::Rect(kScreenWidth, kScreenHeight), gfx->rgb(0.99f, 0.99f, 0.99f));
	gfx->present();
}

Graphics::ManagedSurface *CameraTool::takePicture(const Vec3 &clearColour) {
	Gfx *gfx = g_engine->_gfx;
	Renderer3D *r3d = g_engine->_renderer3d;
	// The car over the whole screen with the current camera, the logo in the corner, presented
	const Common::Rect full(kScreenWidth, kScreenHeight);
	g_engine->_workShop->_carView.renderScene(full, clearColour);
	if (r3d)
		r3d->endFrame(*gfx);
	if (_logo)
		gfx->blit(kPageFront, *_logo, 5, 5);
	gfx->present();
	// Snapshot_grabScreen (0x408e8b)
	Graphics::ManagedSurface *shot = new Graphics::ManagedSurface(kScreenWidth, kScreenHeight, gfx->front().format);
	shot->simpleBlitFrom(gfx->front());
	return shot;
}

void CameraTool::save(Graphics::ManagedSurface *img, bool printable) {
	if (!img)
		return;
	Common::SaveFileManager *sfm = g_system->getSavefileManager();
	// SnapShots\Shots_<name>\Shot<N>.jpg / Shot<N>p.bmp: as save files "<target>-Shots_<name>-Shot<N>...";
	// the first free N of 0..999 (Shot999 is overwritten when all are taken)
	Common::String who = g_engine->_mechanic->_name;
	for (uint i = 0; i < who.size(); i++)
		if (!Common::isAlnum(who[i]))
			who.setChar('_', i);
	const Common::String base = Common::String::format("%s-Shots_%s-Shot", g_engine->targetName().c_str(), who.c_str());
	// No JPEG encoder: the picture is written as PNG
	const char *ext = printable ? "p.bmp" : ".png";
	Common::String name;
	for (int n = 0; n < 1000; n++) {
		name = base + Common::String::format("%i", n) + ext;
		if (!sfm->exists(name))
			break;
	}
	Common::Array<byte> rgba;
	surfaceToRGBA(*img, rgba);
	if (printable) {
		// Converted to 8 bits and saved as BMP
		Graphics::Surface pal8;
		byte palette[256 * 3];
		quantize8(img->w, img->h, rgba, pal8, palette);
		Common::OutSaveFile *f = sfm->openForSaving(name, false);
		if (f) {
			Image::writeBMP(*f, pal8, palette, 256);
			f->finalize();
			delete f;
			_lastFiles.push_back(name);
		}
		pal8.free();
	} else {
		Common::OutSaveFile *f = sfm->openForSaving(name, false);
		if (f) {
			Image::writePNG(*f, img->rawSurface());
			f->finalize();
			delete f;
			_lastFiles.push_back(name);
		}
		// The 100x75 thumbnail Shot<N>.bmp, 8 bits
		Common::Array<byte> thumb;
		resizeRGBA(img->w, img->h, rgba, 100, 75, thumb);
		Graphics::Surface pal8;
		byte palette[256 * 3];
		quantize8(100, 75, thumb, pal8, palette);
		const Common::String tname = name.substr(0, name.size() - 4) + ".bmp";
		f = sfm->openForSaving(tname, false);
		if (f) {
			Image::writeBMP(*f, pal8, palette, 256);
			f->finalize();
			delete f;
			_lastFiles.push_back(tname);
		}
		pal8.free();
	}
	debugC(1, kDebugGame, "Snapshot saved as %s", name.c_str());
	delete img;
}

void CameraTool::onLeftDown(const Common::Point &p) {
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	CarInstance *car = m->_car;
	if (!car)
		return;
	_lastFiles.clear();
	// "Model in Photo": the model's mesh named "model" is shown for the picture
	int model = -1;
	bool modelWasVisible = false;
	if (ws->_modelInPhoto) {
		const CarModel *cm = car->model();
		for (uint i = 0; i < cm->_meshes.size(); i++)
			if (cm->_meshes[i] && cm->_meshes[i]->name.equalsIgnoreCase("model"))
				model = i;
		if (model >= 0 && model < (int)car->_meshVisible.size()) {
			modelWasVisible = car->_meshVisible[model];
			car->_meshVisible[model] = true;
		}
	}
	_sound->play();
	flashWhite();
	// -PrintableSnaps: also a picture on white (Shot<N>p.bmp)
	if (ConfMan.getBool("gearhead_printablesnaps"))
		save(takePicture(Vec3(0.9999f, 0.9999f, 0.9999f)), true);
	save(takePicture(Vec3(0.3f, 0.29f, 0.3f)), false);
	// The picture stays while the button is held
	Input *input = g_engine->_input;
	while (input->buttons() & 1) {
		if (!g_engine->pollOnly())
			break;
	}
	flashWhite();
	if (model >= 0 && model < (int)car->_meshVisible.size())
		car->_meshVisible[model] = modelWasVisible;
	// The workshop is repainted
	g_engine->_gfx->blit(kPageBoth, *g_engine->_res->getImage("WorkShop.tga"), 0, 0);
	m->drawCash();
	m->drawName();
	ws->refresh();
}

} // End of namespace Gearhead
