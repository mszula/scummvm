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
#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "graphics/managed_surface.h"

#include "gearhead/cardata.h"
#include "gearhead/detection.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/tlv.h"

namespace Gearhead {

// ---------------------------------------------------------------- Texture3D

void Texture3D::create(int sz) {
	size = sz;
	rgba.resize(sz * sz * 4);
	fill(0, 0, 0);
}

void Texture3D::fill(byte r, byte g, byte b) {
	for (uint i = 0; i + 3 < rgba.size(); i += 4) {
		rgba[i] = r;
		rgba[i + 1] = g;
		rgba[i + 2] = b;
		rgba[i + 3] = 255;
	}
	version++;
}

void Texture3D::setPixel(int x, int y, byte r, byte g, byte b) {
	if (x < 0 || y < 0 || x >= size || y >= size)
		return;
	byte *p = &rgba[(y * size + x) * 4];
	p[0] = r;
	p[1] = g;
	p[2] = b;
	p[3] = 255;
	version++;
}

static int pow2Floor(int v) {
	int p = 1;
	while (p * 2 <= v)
		p *= 2;
	return p;
}

/** BITMAP_* chunk -> RGBA pixels (formats 8, 0xb, 16, 24, 0x20) */
static bool decodeBitmapPixels(const TLVChunk &bm, int &w, int &h, Common::Array<byte> &src) {
	w = bm.getInt(kTag_BITMAP_WIDTH);
	h = bm.getInt(kTag_BITMAP_HEIGHT);
	const int fmt = bm.getInt(kTag_BITMAP_FORMAT);
	const TLVChunk *dataChunk = bm.find(kTag_BITMAP_DATA);
	const TLVChunk *palChunk = bm.find(kTag_BITMAP_PALETTE);
	if (w <= 0 || h <= 0 || !dataChunk)
		return false;
	const byte *data = dataChunk->data().data();
	const uint32 dataSize = dataChunk->data().size();

	src.resize(w * h * 4);
	for (int i = 0; i < w * h; i++) {
		byte r = 0, g = 0, b = 0;
		if (fmt == 16 && (uint32)i * 2 + 1 < dataSize) {
			const uint16 v = READ_LE_UINT16(data + i * 2);
			r = ((v >> 11) & 31) << 3 | ((v >> 11) & 31) >> 2;
			g = ((v >> 5) & 63) << 2 | ((v >> 5) & 63) >> 4;
			b = (v & 31) << 3 | (v & 31) >> 2;
		} else if (fmt == 24 && (uint32)i * 3 + 2 < dataSize) {
			b = data[i * 3];
			g = data[i * 3 + 1];
			r = data[i * 3 + 2];
		} else if ((fmt == 8 || fmt == 0xb) && (uint32)i < dataSize && palChunk) {
			const byte idx = data[i];
			const Common::Array<byte> &pal = palChunk->data();
			if ((uint)idx * 3 + 2 < pal.size()) {
				r = pal[idx * 3];
				g = pal[idx * 3 + 1];
				b = pal[idx * 3 + 2];
			}
		} else if (fmt == 0x20 && (uint32)i * 4 + 3 < dataSize) {
			b = data[i * 4];
			g = data[i * 4 + 1];
			r = data[i * 4 + 2];
		}
		src[i * 4] = r;
		src[i * 4 + 1] = g;
		src[i * 4 + 2] = b;
		src[i * 4 + 3] = 255;
	}
	return true;
}

Graphics::ManagedSurface *decodeBitmapSurface(const TLVChunk &bm, const Graphics::PixelFormat &format) {
	int w, h;
	Common::Array<byte> src;
	if (!decodeBitmapPixels(bm, w, h, src))
		return nullptr;
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(w, h, format);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const byte *p = &src[(y * w + x) * 4];
			s->setPixel(x, y, format.RGBToColor(p[0], p[1], p[2]));
		}
	return s;
}

/** TEX_BITMAP -> square power-of-two RGBA (bilinear resample when needed) */
static void decodeBitmap(const TLVChunk &bm, Texture3D &tex) {
	int w, h;
	Common::Array<byte> src;
	if (!decodeBitmapPixels(bm, w, h, src)) {
		tex.create(1);
		return;
	}

	const int size = MIN<int>(MAX(pow2Floor(w), pow2Floor(h)), kMaxTextureSize);
	tex.create(size);
	if (size == w && size == h) {
		memcpy(tex.rgba.data(), src.data(), src.size());
		return;
	}
	for (int y = 0; y < size; y++) {
		const float fy = MAX(0.0f, (y + 0.5f) * h / size - 0.5f);
		const int y0 = MIN<int>((int)fy, h - 1), y1 = MIN(y0 + 1, h - 1);
		const float ty = fy - y0;
		for (int x = 0; x < size; x++) {
			const float fx = MAX(0.0f, (x + 0.5f) * w / size - 0.5f);
			const int x0 = MIN<int>((int)fx, w - 1), x1 = MIN(x0 + 1, w - 1);
			const float tx = fx - x0;
			for (int c = 0; c < 3; c++) {
				const float a = src[(y0 * w + x0) * 4 + c] * (1 - tx) + src[(y0 * w + x1) * 4 + c] * tx;
				const float b = src[(y1 * w + x0) * 4 + c] * (1 - tx) + src[(y1 * w + x1) * 4 + c] * tx;
				tex.rgba[(y * size + x) * 4 + c] = (byte)(a * (1 - ty) + b * ty + 0.5f);
			}
		}
	}
}

// ---------------------------------------------------------------- Material3D

void Material3D::updateTranslucency() {
	const bool opaque = (blendSrc == 1 && blendDst == 0 && shadeAlpha <= 0) ||
	                    (blendDst == 0 && (blendSrc == 4 || blendSrc == 5)) ||
	                    ((blendDst == 2 || blendDst == 6) && (blendSrc == 3 || blendSrc == 7));
	if (opaque)
		flags &= ~kFlagTranslucent;
	else
		flags |= kFlagTranslucent;
}

static Vec3 readVec3(const TLVChunk *c) {
	if (!c)
		return Vec3();
	const Common::Array<float> f = c->asFloatArray();
	return f.size() >= 3 ? Vec3(f[0], f[1], f[2]) : Vec3();
}

// ---------------------------------------------------------------- CarModel

CarModel::CarModel() : _id(-1), _minSkill(0), _loaded(false) {
}

CarModel::~CarModel() {
	clear();
}

void CarModel::clear() {
	for (uint i = 0; i < _textures.size(); i++)
		delete _textures[i];
	for (uint i = 0; i < _materials.size(); i++)
		delete _materials[i];
	for (uint i = 0; i < _meshes.size(); i++)
		delete _meshes[i];
	for (uint i = 0; i < _specMeshes.size(); i++)
		delete _specMeshes[i];
	for (uint i = 0; i < _parts.size(); i++)
		delete _parts[i];
	for (uint i = 0; i < _ignitionSounds.size(); i++)
		delete _ignitionSounds[i].sound;
	_ignitionSounds.clear();
	_textures.clear();
	_materials.clear();
	_meshes.clear();
	_specMeshes.clear();
	_parts.clear();
	_loaded = false;
}

bool CarModel::loadHeader(Resources &res, const Common::String &path) {
	_path = path;
	Common::SeekableReadStream *s = res.open(path);
	if (!s)
		return false;
	TLVChunk *root = TLVReader::parseFile(*s, kTag_CARFILE);
	delete s;
	if (!root)
		return false;
	const TLVChunk *cd = root->find(kTag_CARDATA);
	if (cd) {
		_id = cd->getInt(kTag_CD_ID, -1);
		_name = cd->getString(kTag_CD_NAME);
		_minSkill = cd->getInt(kTag_CD_MINSKILL);
	}
	delete root;
	return cd != nullptr;
}

Mesh3D *CarModel::readMesh(const TLVChunk &c) {
	Mesh3D *m = new Mesh3D();
	m->name = c.getString(kTag_MESH_NAME);
	m->index = c.getInt(kTag_MESH_INDEX, -1);
	m->local = Mat4::fromAxes(readVec3(c.find(kTag_MESH_MATRIXX)), readVec3(c.find(kTag_MESH_MATRIXY)),
	                          readVec3(c.find(kTag_MESH_MATRIXZ)), readVec3(c.find(kTag_MESH_LOC)));

	const Common::Array<const TLVChunk *> verts = c.findAll(kTag_MESH_VERT);
	m->vertices.resize(verts.size());
	for (uint i = 0; i < verts.size(); i++) {
		m->vertices[i].pos = readVec3(verts[i]->find(kTag_VERT_LOC));
		m->vertices[i].normal = readVec3(verts[i]->find(kTag_VERT_NORMAL));
	}

	const Common::Array<const TLVChunk *> polys = c.findAll(kTag_MESH_POLY);
	m->polys.resize(polys.size());
	static const uint kVertTags[3] = { kTag_POLY_VERTINDEX1, kTag_POLY_VERTINDEX2, kTag_POLY_VERTINDEX3 };
	static const uint kMapTags[3] = { kTag_POLY_MAP1, kTag_POLY_MAP2, kTag_POLY_MAP3 };
	for (uint i = 0; i < polys.size(); i++) {
		MeshPoly &p = m->polys[i];
		for (int k = 0; k < 3; k++) {
			p.v[k] = CLIP<int>(polys[i]->getInt(kVertTags[k]), 0, MAX<int>(0, (int)m->vertices.size() - 1));
			const TLVChunk *uv = polys[i]->find(kMapTags[k]);
			const Common::Array<float> f = uv ? uv->asFloatArray() : Common::Array<float>();
			p.uv[k][0] = f.size() > 0 ? f[0] : 0.0f;
			p.uv[k][1] = f.size() > 1 ? f[1] : 0.0f;
		}
		p.fileMaterial = polys[i]->getInt(kTag_POLY_MATINDEX, 0);
		if (!m->vertices.empty()) {
			const Vec3 &a = m->vertices[p.v[0]].pos, &b = m->vertices[p.v[1]].pos, &cc = m->vertices[p.v[2]].pos;
			p.normal = (b - a).cross(cc - a).normalized();
		}
	}

	// World-space bounds
	for (uint i = 0; i < m->vertices.size(); i++) {
		const Vec3 w = m->local.transformPoint(m->vertices[i].pos);
		if (i == 0) {
			m->boundsMin = m->boundsMax = w;
		} else {
			m->boundsMin = Vec3(MIN(m->boundsMin.x, w.x), MIN(m->boundsMin.y, w.y), MIN(m->boundsMin.z, w.z));
			m->boundsMax = Vec3(MAX(m->boundsMax.x, w.x), MAX(m->boundsMax.y, w.y), MAX(m->boundsMax.z, w.z));
		}
	}
	return m;
}

static bool isPaintTextureName(const Common::String &name) {
	Common::String n = name;
	n.toLowercase();
	return n.hasPrefix("painton") || n.hasSuffix("tex.jpg");
}

bool CarModel::load(Resources &res) {
	if (_loaded)
		return true;
	clear();
	Common::SeekableReadStream *s = res.open(_path);
	if (!s)
		return false;
	TLVChunk *root = TLVReader::parseFile(*s, kTag_CARFILE);
	delete s;
	if (!root)
		return false;
	const TLVChunk *cd = root->find(kTag_CARDATA);
	if (!cd) {
		delete root;
		return false;
	}

	for (uint i = 0; i < cd->children().size(); i++) {
		const TLVChunk &c = *cd->children()[i];
		switch (c.id()) {
		case kTag_CD_ID:
			_id = c.asInt();
			break;
		case kTag_CD_NAME:
			_name = c.asString();
			break;
		case kTag_CD_MINSKILL:
			_minSkill = c.asInt();
			break;
		case kTag_CD_TEXTURE: {
			Texture3D *t = new Texture3D();
			t->name = c.getString(kTag_TEX_NAME);
			t->flags = c.getInt(kTag_TEX_FLAGS, 1);
			const TLVChunk *bm = c.find(kTag_TEX_BITMAP);
			if (bm)
				decodeBitmap(*bm, *t);
			else
				t->create(1);
			_textures.push_back(t);
			break;
		}
		case kTag_CD_MATERIAL: {
			Material3D *m = new Material3D();
			m->name = c.getString(kTag_MAT_NAME);
			if (c.find(kTag_MAT_COLOR))
				m->color = readVec3(c.find(kTag_MAT_COLOR));
			m->shadeAlpha = c.getInt(kTag_MAT_SHADEALPHA, 0);
			m->shadeColor = c.getInt(kTag_MAT_SHADECOLOR, 2);
			m->blendSrc = c.getInt(kTag_MAT_BLENDSRC, 1);
			m->blendDst = c.getInt(kTag_MAT_BLENDDST, 0);
			m->alpha = c.getFloat(kTag_MAT_ALPHA, 1.0f);
			const int tex = c.getInt(kTag_MAT_TEXINDEX1, -1);
			if (tex >= 0 && tex < (int)_textures.size())
				m->texture = _textures[tex];
			m->updateTranslucency();
			// The file's MAT_FLAGS is read after the automatic translucency and wins
			if (c.find(kTag_MAT_FLAGS))
				m->flags = c.getInt(kTag_MAT_FLAGS);
			_materials.push_back(m);
			break;
		}
		case kTag_CD_PARTDATA: {
			PartDef *p = new PartDef();
			p->id = c.getInt(kTag_PD_ID, -1);
			p->name = c.getString(kTag_PD_NAME);
			p->custom = c.getInt(kTag_PD_CUSTOM) != 0;
			p->region = c.getInt(kTag_PD_REGION, -1);
			p->meshName = c.getString(kTag_PD_MESHNAME);
			p->costMin = c.getFloat(kTag_PD_COSTMIN);
			p->costMax = c.getFloat(kTag_PD_COSTMAX);
			if (c.find(kTag_PD_ATTACHDEP))
				p->attachDep = c.find(kTag_PD_ATTACHDEP)->asIntArray();
			if (c.find(kTag_PD_REMOVEDEP))
				p->removeDep = c.find(kTag_PD_REMOVEDEP)->asIntArray();
			p->mutexSet = (uint32)c.getInt(kTag_PD_MUTEXCSET);
			p->amea = c.getInt(kTag_PD_AMEA) != 0;
			p->visibleInCar = c.getInt(kTag_PD_VIC, 1) != 0;
			p->special = c.getInt(kTag_PD_SPECIAL);
			const Common::Array<const TLVChunk *> bolts = c.findAll(kTag_PD_BOLT);
			for (uint b = 0; b < bolts.size(); b++)
				p->bolts.push_back(readVec3(bolts[b]));
			const TLVChunk *mm = c.find(kTag_PD_MULTIMAT);
			if (mm) {
				const Common::Array<const TLVChunk *> mats = mm->findAll(kTag_MMAT_MATERIALINDEX);
				for (uint k = 0; k < mats.size(); k++)
					p->multiMaterials.push_back(mats[k]->asInt());
				if (mm->find(kTag_MMAT_SWITCHPOLYLIST))
					p->switchPolys = mm->find(kTag_MMAT_SWITCHPOLYLIST)->asIntArray();
			}
			_parts.push_back(p);
			break;
		}
		case kTag_CD_AUTOMESH:
		case kTag_CD_SPECMESH: {
			Mesh3D *m = readMesh(c);
			Common::Array<Mesh3D *> &list = (c.id() == kTag_CD_AUTOMESH) ? _meshes : _specMeshes;
			if (m->index < 0) {
				delete m;
				break;
			}
			if ((int)list.size() <= m->index)
				list.resize(m->index + 1);
			delete list[m->index];
			list[m->index] = m;
			break;
		}
		case kTag_CD_SOUND_IGNITION: {
			// AutoData_readIgnitionSound (0x44052c): IS_WAVFILE holds the WAV data itself
			IgnitionSound is;
			is.partId = c.getInt(kTag_IS_PARTID, -1);
			const TLVChunk *wav = c.find(kTag_IS_WAVFILE);
			if (wav && !wav->data().empty()) {
				is.sound = new Sound();
				if (!is.sound->load(wav->data().data(), wav->data().size())) {
					delete is.sound;
					is.sound = nullptr;
				}
			}
			_ignitionSounds.push_back(is);
			break;
		}
		default:
			// CD_SOUND_NOSTARTER / FALSESTART are never read by the original
			break;
		}
	}
	delete root;

	_specMeshes.resize(MAX(_specMeshes.size(), _meshes.size()));

	// Bind meshes to parts: for each mesh, the first part with that mesh name
	for (uint i = 0; i < _meshes.size(); i++) {
		if (!_meshes[i])
			continue;
		for (uint p = 0; p < _parts.size(); p++) {
			if (_parts[p]->meshName.equalsIgnoreCase(_meshes[i]->name)) {
				_parts[p]->meshIndex = i;
				break;
			}
		}
	}

	// PartData_collectPaintPolys (0x43a290): body parts only
	for (uint p = 0; p < _parts.size(); p++) {
		PartDef *pd = _parts[p];
		if (pd->meshIndex < 0 || pd->region != 2)
			continue;
		const Mesh3D *m = _meshes[pd->meshIndex];
		for (uint k = 0; k < m->polys.size(); k++) {
			const int fm = m->polys[k].fileMaterial;
			const bool paint = fm == kPaintMaterialIndex ||
			                   (fm >= 0 && fm < (int)_materials.size() && _materials[fm]->texture &&
			                    isPaintTextureName(_materials[fm]->texture->name));
			if (paint)
				pd->paintPolys.push_back(k);
		}
	}

	_loaded = true;
	debugC(1, kDebugGame, "Loaded car %d '%s': %d textures, %d materials, %d meshes, %d parts", _id, _name.c_str(),
	       _textures.size(), _materials.size(), _meshes.size(), _parts.size());
	return true;
}

const PartDef *CarModel::findPart(int id) const {
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i]->id == id)
			return _parts[i];
	return nullptr;
}

// ---------------------------------------------------------------- CarList

CarList::~CarList() {
	for (uint i = 0; i < _models.size(); i++)
		delete _models[i];
}

void CarList::loadAll(Resources &res) {
	_res = &res;
	const Common::StringArray files = res.list("Data\\Cars", "*.car");
	for (uint i = 0; i < files.size(); i++) {
		CarModel *m = new CarModel();
		if (m->loadHeader(res, "Data\\Cars\\" + files[i])) {
			_models.push_back(m);
			debugC(1, kDebugGame, "Car %d: %s (%s)", m->_id, m->_name.c_str(), files[i].c_str());
		} else {
			delete m;
		}
	}
}

CarModel *CarList::find(int id, bool load) {
	for (uint i = 0; i < _models.size(); i++) {
		if (_models[i]->_id == id) {
			if (load && !_models[i]->isLoaded() && _res)
				_models[i]->load(*_res);
			return _models[i];
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------- condition levels, paint colours

const char *conditionName(int condition) {
	static const char *const kNames[4] = { "Scrap", "Barely", "Worn", "Perfect" };
	return kNames[CLIP(condition, 0, 3)];
}

const Material3D *conditionMaterial(int condition) {
	static Material3D mats[4];
	static bool init = false;
	if (!init) {
		// CondLevels_initColors (0x44f998), 16-bit mode: tint / 255, Gouraud, two-sided
		static const float kTint[4][3] = { { 71, 49, 38 }, { 85, 78, 67 }, { 153, 134, 128 }, { 255, 255, 255 } };
		for (int i = 0; i < 4; i++) {
			mats[i].shadeColor = 2;
			mats[i].flags = Material3D::kFlagTwoSided;
			mats[i].color = Vec3(kTint[i][0] / 255.0f, kTint[i][1] / 255.0f, kTint[i][2] / 255.0f);
			mats[i].name = conditionName(i);
		}
		init = true;
	}
	return &mats[CLIP(condition, 0, 3)];
}

// CondLevels_initColors (0x44f998), 16-bit (D3D) status colours
static const float kStatusColour[4][3] = { { 0.25f, 0.25f, 0.25f }, { 0.75f, 0.05f, 0.05f }, { 0.75f, 0.75f, 0.05f }, { 0.05f, 0.5f, 0.05f } };

Vec3 conditionColour(int condition) {
	const float *c = kStatusColour[CLIP(condition, 0, 3)];
	return Vec3(c[0], c[1], c[2]);
}

const Material3D *conditionHighlight(int condition) {
	static Material3D mats[4];
	static bool init = false;
	if (!init) {
		// CondLevel_ctor (0x44f810): additive, vertex alpha, Gouraud, two-sided
		for (int i = 0; i < 4; i++) {
			mats[i].blendSrc = 1;
			mats[i].blendDst = 1;
			mats[i].shadeAlpha = 2;
			mats[i].shadeColor = 2;
			mats[i].updateTranslucency();
			mats[i].flags |= Material3D::kFlagTwoSided;
			mats[i].color = conditionColour(i);
			mats[i].name = conditionName(i);
		}
		init = true;
	}
	return &mats[CLIP(condition, 0, 3)];
}

static float g_paintColors[kNumPaintColors][3];

void loadPaintColors(Resources &res) {
	const Graphics::PixelFormat rgb24 = Graphics::PixelFormat::createFormatRGB24();
	Graphics::ManagedSurface *img = res.loadImage("Data\\Gfx24\\PaintColors.tga", &rgb24);
	if (!img)
		error("Error loading bitmap resource. Data\\Gfx24\\PaintColors.tga");
	for (int i = 0; i < kNumPaintColors; i++) {
		const int x = i % img->w, y = i / img->w;
		if (y >= img->h)
			break;
		byte r, g, b;
		img->format.colorToRGB(img->getPixel(x, y), r, g, b);
		g_paintColors[i][0] = r / 256.0f;
		g_paintColors[i][1] = g / 256.0f;
		g_paintColors[i][2] = b / 256.0f;
	}
	delete img;
}

Vec3 paintColor(int index) {
	const float *c = g_paintColors[CLIP(index, 0, kNumPaintColors - 1)];
	return Vec3(c[0], c[1], c[2]);
}

} // End of namespace Gearhead
