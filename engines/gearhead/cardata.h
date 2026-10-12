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

#ifndef GEARHEAD_CARDATA_H
#define GEARHEAD_CARDATA_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/math3d.h"

namespace Gearhead {

class Resources;
class TLVChunk;

enum {
	kPaintMaterialIndex = 666, ///< POLY_MATINDEX of body-paint polygons
	kMaxTextureSize = 256,
	kNumPaintColors = 27
};

/** Square power-of-two RGBA texture (Texture_makePow2 0x465030: at most 256) */
struct Texture3D {
	Common::String name;
	int size = 0;
	uint32 flags = 1;              ///< TEX_FLAGS: bit0 bilinear, bit2/3 clamp U/V
	Common::Array<byte> rgba;      ///< size * size * 4, top row first
	uint32 version = 0;            ///< bumped whenever the pixels change (re-upload)

	void create(int sz);
	void fill(byte r, byte g, byte b);
	void setPixel(int x, int y, byte r, byte g, byte b);
};

/** Material (0x40 bytes in the original, Material_ctor 0x469810) */
struct Material3D {
	enum {
		kFlagTranslucent = 1,
		kFlagTwoSided = 2,
		kFlagUnlit = 0x800
	};

	int shadeColor = 2;            ///< 0 unlit, 1 flat, 2 Gouraud
	int shadeAlpha = 0;            ///< 0 opaque, 1 material alpha, 2 vertex alpha
	int blendSrc = 1;              ///< blend enum: 0 ZERO 1 ONE 2 SRC_COLOR 3 INV_SRC_COLOR 4 SRC_ALPHA 5 INV_SRC_ALPHA ...
	int blendDst = 0;
	Common::String name;
	Texture3D *texture = nullptr;  ///< MAT_TEXINDEX1, modulated with the lit colour
	Vec3 color = Vec3(0.5f, 0.5f, 0.6f);
	float alpha = 1.0f;
	Vec3 emissive;
	uint32 flags = 0;

	bool translucent() const { return flags & kFlagTranslucent; }
	bool twoSided() const { return flags & kFlagTwoSided; }
	/** Material_updateTranslucency (0x4699c0) for materials created at runtime */
	void updateTranslucency();
};

struct MeshVertex {
	Vec3 pos;
	Vec3 normal;
	Vec3 colour;   ///< added to the lit colour (0 unless the gloss pass writes it)
	float alpha = 1.0f;
};

struct MeshPoly {
	int v[3];
	float uv[3][2];
	int fileMaterial;  ///< POLY_MATINDEX (kPaintMaterialIndex = paint)
	Vec3 normal;       ///< face normal, normalize((v1 - v0) x (v2 - v0))
};

/** A mesh of a car model (CD_AUTOMESH / CD_SPECMESH) */
struct Mesh3D {
	Common::String name;
	int index = -1;
	Mat4 local;        ///< rows MESH_MATRIXX/Y/Z, translation MESH_LOC
	Common::Array<MeshVertex> vertices;
	Common::Array<MeshPoly> polys;
	Vec3 boundsMin, boundsMax;   ///< in world (car) space

	/** Shell meshes ("$01", "^01_"...): second character '0', always shown */
	bool isShell() const { return name.size() > 1 && name[1] == '0'; }
};

/** CD_PARTDATA (PartData, 0xe4 bytes) */
struct PartDef {
	int id = -1;
	Common::String name;
	Common::String meshName;
	int region = -1;                     ///< 1 engine, 2 body, 3 running gear
	float costMin = 0.0f, costMax = 0.0f;
	Common::Array<int> attachDep, removeDep;
	uint32 mutexSet = 0;
	bool custom = false;
	bool visibleInCar = true;            ///< PD_VIC
	bool amea = false;
	int special = 0;
	Common::Array<Vec3> bolts;
	Common::Array<int> multiMaterials;   ///< indices into CarModel::materials
	Common::Array<int> switchPolys;      ///< cumulative poly counts, -1 terminated
	int meshIndex = -1;
	Common::Array<int> paintPolys;       ///< polys of the mesh drawn with the car paint (region 2 only)
};

/** A car model loaded from a .car file (AutoData) */
class CarModel {
public:
	CarModel();
	~CarModel();

	/** Header only (CD_ID, CD_NAME, CD_MINSKILL), like AutoData_loadHeader */
	bool loadHeader(Resources &res, const Common::String &path);
	/** Full load (AutoData_load 0x43fb1c) */
	bool load(Resources &res);
	bool isLoaded() const { return _loaded; }

	const PartDef *findPart(int id) const;

	int _id;
	Common::String _name;
	int _minSkill;
	Common::String _path;

	Common::Array<Texture3D *> _textures;
	Common::Array<Material3D *> _materials;
	Common::Array<Mesh3D *> _meshes;       ///< indexed by MESH_INDEX (may contain nullptr)
	Common::Array<Mesh3D *> _specMeshes;   ///< same indexing
	Common::Array<PartDef *> _parts;       ///< file order

private:
	Mesh3D *readMesh(const TLVChunk &c);
	void clear();
	bool _loaded;
};

/** Every car model of Data\Cars, loaded (header only) at start-up (AutoDataList) */
class CarList {
public:
	~CarList();
	void loadAll(Resources &res);
	/** AutoData_find: optionally loads the full model */
	CarModel *find(int id, bool load);
	const Common::Array<CarModel *> &models() const { return _models; }

private:
	Common::Array<CarModel *> _models;
	Resources *_res = nullptr;
};

/** Part condition levels (g_CondLevels): 0 Scrap, 1 Barely, 2 Worn, 3 Perfect */
const char *conditionName(int condition);
/** Material drawn on every polygon of a damaged part (CondLevel +0x94) */
const Material3D *conditionMaterial(int condition);

/** g_PaintColors from Gfx24\PaintColors.tga (27 colours, divided by 256) */
void loadPaintColors(Resources &res);
Vec3 paintColor(int index);

} // End of namespace Gearhead

#endif // GEARHEAD_CARDATA_H
