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
#include "common/textconsole.h"
#include "graphics/managed_surface.h"

#include "gearhead/detection.h"
#include "gearhead/resources.h"
#include "gearhead/scene3ds.h"

namespace Gearhead {

/** Load3DS_convertAxes (0x46c920) */
static Vec3 convertAxes(float x, float y, float z) {
	return Vec3(-x, z, -y);
}

static Vec3 readVec(Common::SeekableReadStream &s) {
	const float x = s.readFloatLE();
	const float y = s.readFloatLE();
	const float z = s.readFloatLE();
	return convertAxes(x, y, z);
}

static Common::String readCString(Common::SeekableReadStream &s, uint32 end) {
	Common::String r;
	while ((uint32)s.pos() < end) {
		const char c = s.readByte();
		if (!c)
			break;
		r += c;
	}
	return r;
}

void textureFromSurface(const Graphics::ManagedSurface &img, Texture3D &tex) {
	Common::Array<byte> rgba(img.w * img.h * 4);
	for (int y = 0; y < img.h; y++)
		for (int x = 0; x < img.w; x++) {
			byte a, r, g, b;
			img.format.colorToARGB(img.getPixel(x, y), a, r, g, b);
			byte *p = &rgba[(y * img.w + x) * 4];
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = img.format.aBits() ? a : 255;
		}
	textureFromRGBA(img.w, img.h, rgba, tex);
}

static bool rayTriangle(const Vec3 &o, const Vec3 &d, const Vec3 &a, const Vec3 &b, const Vec3 &c, float &t) {
	const Vec3 e1 = b - a, e2 = c - a;
	const Vec3 pv = d.cross(e2);
	const float det = e1.dot(pv);
	if (fabsf(det) < 1e-12f)
		return false;
	const float inv = 1.0f / det;
	const Vec3 tv = o - a;
	const float u = tv.dot(pv) * inv;
	if (u < 0.0f || u > 1.0f)
		return false;
	const Vec3 qv = tv.cross(e1);
	const float v = d.dot(qv) * inv;
	if (v < 0.0f || u + v > 1.0f)
		return false;
	t = e2.dot(qv) * inv;
	return t > 0.0f;
}

bool rayHitsMesh(const Mesh3D &mesh, const Mat4 &world, const Vec3 &origin, const Vec3 &dir, float &tBest) {
	bool hit = false;
	Common::Array<Vec3> w(mesh.vertices.size());
	for (uint i = 0; i < mesh.vertices.size(); i++)
		w[i] = world.transformPoint(mesh.vertices[i].pos);
	for (uint k = 0; k < mesh.polys.size(); k++) {
		const MeshPoly &p = mesh.polys[k];
		float t;
		if (rayTriangle(origin, dir, w[p.v[0]], w[p.v[1]], w[p.v[2]], t) && (!hit || t < tBest)) {
			tBest = t;
			hit = true;
		}
	}
	return hit;
}

// ---------------------------------------------------------------- Scene3DS

Scene3DS::~Scene3DS() {
	for (uint i = 0; i < _meshes.size(); i++)
		delete _meshes[i];
	for (uint i = 0; i < _materials.size(); i++)
		delete _materials[i];
	for (uint i = 0; i < _textures.size(); i++)
		delete _textures[i];
}

SceneMesh *Scene3DS::findMesh(const Common::String &name) const {
	for (uint i = 0; i < _meshes.size(); i++)
		if (_meshes[i]->name == name)
			return _meshes[i];
	return nullptr;
}

const SceneNode *Scene3DS::findNode(const Common::String &name) const {
	for (uint i = 0; i < _nodes.size(); i++)
		if (_nodes[i].name == name)
			return &_nodes[i];
	return nullptr;
}

Material3D *Scene3DS::findMaterial(const Common::String &name) const {
	for (uint i = 0; i < _materialNames.size(); i++)
		if (_materialNames[i] == name)
			return _materials[i];
	return nullptr;
}

bool Scene3DS::readChunk(Common::SeekableReadStream &s, uint32 limit, Chunk &c) {
	if ((uint32)s.pos() + 6 > limit)
		return false;
	const uint32 start = s.pos();
	c.id = s.readUint16LE();
	const uint32 len = s.readUint32LE();
	if (len < 6 || start + len > limit)
		return false;
	c.start = start + 6;
	c.end = start + len;
	return true;
}

Texture3D *Scene3DS::loadTexture(const Common::String &file) {
	for (uint i = 0; i < _textures.size(); i++)
		if (_textures[i]->name.equalsIgnoreCase(file))
			return _textures[i];
	Graphics::ManagedSurface *img = _res->loadImage(_dir + file, nullptr);
	if (!img) {
		warning("Scene3DS: cannot load texture %s", file.c_str());
		return nullptr;
	}
	Texture3D *t = new Texture3D();
	t->name = file;
	t->flags = 1;
	textureFromSurface(*img, *t);
	delete img;
	_textures.push_back(t);
	return t;
}

bool Scene3DS::load(Resources &res, const Common::String &dir, const Common::String &file) {
	_res = &res;
	_dir = dir;
	_name = file;
	Common::SeekableReadStream *s = res.open(dir + file);
	if (!s)
		return false;
	const uint32 size = s->size();
	Chunk main;
	if (!readChunk(*s, size, main) || main.id != 0x4d4d) {
		delete s;
		return false;
	}
	Chunk c;
	while (readChunk(*s, main.end, c)) {
		if (c.id == 0x3d3d)
			readEditor(*s, c.end);
		else if (c.id == 0xb000)
			readKeyframer(*s, c.end);
		s->seek(c.end);
	}
	delete s;
	debugC(1, kDebugGraphics, "Scene3DS %s: %d meshes, %d materials, %d textures, %d nodes", file.c_str(),
	       _meshes.size(), _materials.size(), _textures.size(), _nodes.size());
	return true;
}

void Scene3DS::readEditor(Common::SeekableReadStream &s, uint32 end) {
	Chunk c;
	while (readChunk(s, end, c)) {
		if (c.id == 0xafff)
			readMaterial(s, c.end);
		else if (c.id == 0x4000)
			readObject(s, c.end);
		s.seek(c.end);
	}
}

void Scene3DS::readMaterial(Common::SeekableReadStream &s, uint32 end) {
	// Load3DS_material (0xafff): blend from the transparency / additive flags
	Material3D *m = new Material3D();
	Common::String name;
	bool additive = false, alpha = false;
	Chunk c;
	while (readChunk(s, end, c)) {
		switch (c.id) {
		case 0xa000:
			name = readCString(s, c.end);
			break;
		case 0xa020: {
			// the first colour sub-chunk is read as 3 bytes (0x0011), / 255
			s.skip(6);
			const byte r = s.readByte(), g = s.readByte(), b = s.readByte();
			m->color = Vec3(r / 255.0f, g / 255.0f, b / 255.0f);
			break;
		}
		case 0xa050: {
			// transparency percentage: alpha mode 1, opacity (100 - p) / 100
			s.skip(6);
			const int16 p = s.readSint16LE();
			if (p != 0) {
				alpha = true;
				m->shadeAlpha = 1;
				m->alpha = (100 - p) * 0.01f;
			}
			break;
		}
		case 0xa081:
			m->flags |= Material3D::kFlagTwoSided;
			break;
		case 0xa083:
			additive = true;
			break;
		case 0xa100: {
			const byte shading = s.readByte();
			m->shadeColor = (shading == 1) ? 1 : 2;
			break;
		}
		case 0xa200: {
			// texture map: the file name sub-chunk 0xa300
			Chunk t;
			while (readChunk(s, c.end, t)) {
				if (t.id == 0xa300) {
					m->texture = loadTexture(readCString(s, t.end));
					if (m->texture) {
						// textures with an alpha channel make the material translucent
						for (uint i = 3; i < m->texture->rgba.size(); i += 4)
							if (m->texture->rgba[i] != 255) {
								alpha = true;
								break;
							}
					}
				}
				s.seek(t.end);
			}
			break;
		}
		default:
			break;
		}
		s.seek(c.end);
	}
	const int mode = (additive ? 2 : 0) | (alpha ? 1 : 0);
	static const int kBlend[4][2] = { { 1, 0 }, { 4, 5 }, { 1, 1 }, { 4, 1 } };
	m->blendSrc = kBlend[mode][0];
	m->blendDst = kBlend[mode][1];
	m->updateTranslucency();
	m->name = name;
	_materials.push_back(m);
	_materialNames.push_back(name);
}

void Scene3DS::readObject(Common::SeekableReadStream &s, uint32 end) {
	const Common::String name = readCString(s, end);
	Chunk c;
	while (readChunk(s, end, c)) {
		if (c.id == 0x4100) {
			readTriMesh(s, c.end, name);
		} else if (c.id == 0x4700 && !_camera.valid) {
			// Load3DS_camera: position, target, roll, lens; zoom = 1 / tan(lens / 2 degrees)
			_camera.pos = readVec(s);
			_camera.target = readVec(s);
			s.readFloatLE();
			const float lens = s.readFloatLE();
			_camera.zoom = 1.0f / tanf(lens * 0.017453292f * 0.5f);
			_camera.valid = true;
		}
		s.seek(c.end);
	}
}

void Scene3DS::readTriMesh(Common::SeekableReadStream &s, uint32 end, const Common::String &name) {
	Common::Array<Vec3> verts;
	struct Face {
		uint16 a, b, c, flags;
	};
	Common::Array<Face> faces;
	Common::Array<float> uvs;
	Common::Array<const Material3D *> faceMats;
	Chunk c;
	while (readChunk(s, end, c)) {
		switch (c.id) {
		case 0x4110: {
			const uint16 n = s.readUint16LE();
			for (uint i = 0; i < n; i++)
				verts.push_back(readVec(s));
			break;
		}
		case 0x4120: {
			const uint16 n = s.readUint16LE();
			for (uint i = 0; i < n; i++) {
				Face f;
				f.a = s.readUint16LE();
				f.b = s.readUint16LE();
				f.c = s.readUint16LE();
				f.flags = s.readUint16LE();
				faces.push_back(f);
			}
			faceMats.resize(n);
			for (uint i = 0; i < n; i++)
				faceMats[i] = &_defaultMaterial;
			Chunk m;
			while (readChunk(s, c.end, m)) {
				if (m.id == 0x4130) {
					const Material3D *mat = findMaterial(readCString(s, m.end));
					const uint16 k = s.readUint16LE();
					for (uint i = 0; i < k; i++) {
						const uint16 f = s.readUint16LE();
						if (f < faceMats.size() && mat)
							faceMats[f] = mat;
					}
				}
				s.seek(m.end);
			}
			break;
		}
		case 0x4140: {
			const uint16 n = s.readUint16LE();
			for (uint i = 0; i < n; i++) {
				uvs.push_back(s.readFloatLE());
				uvs.push_back(-s.readFloatLE()); // v negated
			}
			break;
		}
		default:
			// 0x4160 (mesh matrix) only moves the vertices into object space; we keep world space
			break;
		}
		s.seek(c.end);
	}

	SceneMesh *sm = new SceneMesh();
	sm->name = name;
	sm->mesh.name = name;
	sm->mesh.vertices.resize(verts.size());
	for (uint i = 0; i < verts.size(); i++) {
		sm->mesh.vertices[i].pos = verts[i];
		if (i == 0) {
			sm->mesh.boundsMin = sm->mesh.boundsMax = verts[i];
		} else {
			sm->mesh.boundsMin = Vec3(MIN(sm->mesh.boundsMin.x, verts[i].x), MIN(sm->mesh.boundsMin.y, verts[i].y), MIN(sm->mesh.boundsMin.z, verts[i].z));
			sm->mesh.boundsMax = Vec3(MAX(sm->mesh.boundsMax.x, verts[i].x), MAX(sm->mesh.boundsMax.y, verts[i].y), MAX(sm->mesh.boundsMax.z, verts[i].z));
		}
	}
	Common::Array<Vec3> normalSum(verts.size());
	for (uint i = 0; i < faces.size(); i++) {
		const Face &f = faces[i];
		if (f.a >= verts.size() || f.b >= verts.size() || f.c >= verts.size())
			continue;
		MeshPoly p;
		// stored reversed (c, b, a)
		p.v[0] = f.c;
		p.v[1] = f.b;
		p.v[2] = f.a;
		for (int k = 0; k < 3; k++) {
			const uint vi = p.v[k];
			p.uv[k][0] = vi * 2 + 1 < uvs.size() ? uvs[vi * 2] : 0.0f;
			p.uv[k][1] = vi * 2 + 1 < uvs.size() ? uvs[vi * 2 + 1] : 0.0f;
		}
		// Poly_fixUVWrap (0x46ff90), enabled by the face's wrap flags
		for (int axis = 0; axis < 2; axis++) {
			if (!(f.flags & (axis == 0 ? 0x8 : 0x800)))
				continue;
			float lo = p.uv[0][axis], hi = p.uv[0][axis];
			for (int k = 1; k < 3; k++) {
				lo = MIN(lo, p.uv[k][axis]);
				hi = MAX(hi, p.uv[k][axis]);
			}
			if (hi - lo > 0.8f) {
				const float add = ceilf(hi - lo);
				for (int k = 0; k < 3; k++)
					if (p.uv[k][axis] < 0.5f)
						p.uv[k][axis] += add;
			}
		}
		p.fileMaterial = -1;
		const Vec3 &a = verts[p.v[0]], &b = verts[p.v[1]], &cc = verts[p.v[2]];
		p.normal = (b - a).cross(cc - a).normalized();
		for (int k = 0; k < 3; k++)
			normalSum[p.v[k]] += p.normal;
		sm->mesh.polys.push_back(p);
		sm->materials.push_back(faceMats[i]);
	}
	// Mesh_computeVertexNormals (0x4705b0): the sum of the face normals, normalised
	for (uint i = 0; i < verts.size(); i++) {
		const float l = normalSum[i].length();
		sm->mesh.vertices[i].normal = (l == 0.0f || l > 1e30f) ? Vec3() : normalSum[i] * (1.0f / l);
	}
	_meshes.push_back(sm);
}

void Scene3DS::readKeyframer(Common::SeekableReadStream &s, uint32 end) {
	Chunk c;
	while (readChunk(s, end, c)) {
		if (c.id == 0xb002 || c.id == 0xb003 || c.id == 0xb004 || c.id == 0xb005) {
			SceneNode node;
			Chunk k;
			while (readChunk(s, c.end, k)) {
				switch (k.id) {
				case 0xb010:
					node.name = readCString(s, k.end);
					break;
				case 0xb013:
					node.pivot = readVec(s);
					break;
				case 0xb020:
				case 0xb021: {
					// track header, then key 0: frame, accel flags, optional TCB/ease values
					s.skip(2 + 8);
					const uint32 nkeys = s.readUint32LE();
					if (!nkeys)
						break;
					s.readUint32LE();
					const uint16 accel = s.readUint16LE();
					for (int bit = 0; bit < 5; bit++)
						if (accel & (1 << bit))
							s.readFloatLE();
					if (k.id == 0xb020) {
						node.pos = readVec(s);
					} else {
						const float angle = s.readFloatLE();
						const Vec3 axis = readVec(s);
						// The axis flip mirrors the rotation sense
						const Vec3 a = axis.normalized();
						const float th = -angle, co = cosf(th), si = sinf(th), t = 1.0f - co;
						Mat4 r;
						// column-vector rotation R; stored transposed for row vectors
						const float m[3][3] = {
							{ t * a.x * a.x + co, t * a.x * a.y - si * a.z, t * a.x * a.z + si * a.y },
							{ t * a.x * a.y + si * a.z, t * a.y * a.y + co, t * a.y * a.z - si * a.x },
							{ t * a.x * a.z - si * a.y, t * a.y * a.z + si * a.x, t * a.z * a.z + co }
						};
						for (int i = 0; i < 3; i++)
							for (int jj = 0; jj < 3; jj++)
								r.at(i, jj) = m[jj][i];
						if (axis.lengthSq() > 0.0f)
							node.rotation = r;
					}
					break;
				}
				default:
					break;
				}
				s.seek(k.end);
			}
			if (c.id == 0xb004)
				node.name += ".target";
			_nodes.push_back(node);
		}
		s.seek(c.end);
	}
}

} // End of namespace Gearhead
