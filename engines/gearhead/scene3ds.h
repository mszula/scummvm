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

#ifndef GEARHEAD_SCENE3DS_H
#define GEARHEAD_SCENE3DS_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/cardata.h"

namespace Common {
class SeekableReadStream;
}

namespace Gearhead {

class Renderer3D;
class Resources;

/**
 * A mesh of a 3DS scene. The vertices are placed in world space (game axes) at
 * load time with the transform ALIBI applies: object space from the mesh
 * matrix, minus the keyframer pivot, then the key-0 rotation and position.
 */
struct SceneMesh {
	Common::String name;
	Mesh3D mesh;
	Vec3 objMin, objMax; ///< object-space bounds (MeshData +4, after the pivot)
	Common::Array<const Material3D *> materials; ///< per polygon
	bool visible = true;
	bool lit = true;  ///< light mask (the car lot's Floor gets none)
};

/** Keyframer data of one node at t = 0 (Scene3DS_evalKeys) */
struct SceneNode {
	Common::String name;
	Vec3 pos;          ///< position key 0 (Node+0x58)
	Vec3 pivot;
	Mat4 rotation;     ///< rotation key 0 (Node+0x70)
};

/**
 * A 3DS light (Load3DS_light 0x46c750): a point light. Colour 0x0010 (floats only),
 * range DL_OUTER_RANGE 0x465a (default 300), intensity DL_MULTIPLIER 0x465b * 1.8
 * (default multiplier 2.2)
 */
struct SceneLight {
	Common::String name;
	Vec3 pos;
	Vec3 colour = Vec3(1, 1, 1);
	float range = 300.0f;
	float intensity = 2.2f * 1.8f;
};

/** Something drawn with a scene: a car clone, a shadow, a part on a shelf */
struct SceneObject {
	const Mesh3D *mesh = nullptr;
	Mat4 world;                                        ///< mesh space -> world
	const Common::Array<const Material3D *> *materials = nullptr;
	bool visible = true;
	bool lit = true;
};

/**
 * Autodesk 3DS scene as loaded by ALIBI (Scene3DS_load 0x46e060): axes
 * (x, y, z) -> (-x, z, -y), faces reversed, v negated, UV seam fix, 3DS
 * materials mapped to blend modes, keyframes evaluated at t = 0.
 */
class Scene3DS {
public:
	~Scene3DS();

	/** @p dir is the resource directory of the file and its textures (e.g. "Data\Scenes\") */
	bool load(Resources &res, const Common::String &dir, const Common::String &file);

	/** SceneFile_findMesh: exact, case-sensitive */
	SceneMesh *findMesh(const Common::String &name) const;
	const SceneNode *findNode(const Common::String &name) const;

	Common::Array<SceneMesh *> _meshes;
	Common::Array<Material3D *> _materials;
	Common::Array<Texture3D *> _textures;
	Common::Array<SceneNode> _nodes;
	Common::Array<SceneLight> _lights;

	struct Camera {
		bool valid = false;
		Vec3 pos, target;
		/**
		 * 1 / tan(fov / 2): the camera chunk's lens value taken as degrees,
		 * replaced by the keyframer's FOV track (Scene3DS_evalKeys) when there is one
		 */
		float zoom = 1.0f;
		Common::String name;
	} _camera;
	Common::String _name;

private:
	struct Chunk {
		uint16 id;
		uint32 start, end; ///< payload range
	};
	bool readChunk(Common::SeekableReadStream &s, uint32 limit, Chunk &c);
	void readEditor(Common::SeekableReadStream &s, uint32 end);
	void readMaterial(Common::SeekableReadStream &s, uint32 end);
	void readObject(Common::SeekableReadStream &s, uint32 end);
	void readTriMesh(Common::SeekableReadStream &s, uint32 end, const Common::String &name);
	void readKeyframer(Common::SeekableReadStream &s, uint32 end);
	/** Face normals, vertex normals (Mesh_computeVertexNormals 0x4705b0) and bounds from the vertices */
	static void computeNormals(SceneMesh &sm);
	Texture3D *loadTexture(const Common::String &file);
	Material3D *findMaterial(const Common::String &name) const;

	Resources *_res = nullptr;
	Common::String _dir;
	Common::Array<Common::String> _materialNames;
	Material3D _defaultMaterial;
};

/** Convert a 2D image to a square power-of-two RGBA texture (Texture_makePow2 0x465030) */
void textureFromSurface(const Graphics::ManagedSurface &img, Texture3D &tex);

/** Nearest triangle of @p mesh (placed by @p world) hit by the ray; t along the ray */
bool rayHitsMesh(const Mesh3D &mesh, const Mat4 &world, const Vec3 &origin, const Vec3 &dir, float &t);

} // End of namespace Gearhead

#endif // GEARHEAD_SCENE3DS_H
