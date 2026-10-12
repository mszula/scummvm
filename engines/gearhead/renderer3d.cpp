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

#include "common/algorithm.h"
#include "common/textconsole.h"
#include "graphics/surface.h"
#include "graphics/tinygl/tinygl.h"

#include "gearhead/gfx.h"
#include "gearhead/renderer3d.h"

namespace Gearhead {

Renderer3D::Renderer3D() : _initialized(false), _bound(nullptr), _triangles(0) {
}

Renderer3D::~Renderer3D() {
	if (_initialized) {
		for (TextureMap::iterator it = _textures.begin(); it != _textures.end(); ++it) {
			TGLuint id = it->_value.id;
			tglDeleteTextures(1, &id);
		}
		TinyGL::destroyContext();
	}
}

bool Renderer3D::init(const Graphics::PixelFormat &format) {
	TinyGL::createContext(kScreenWidth, kScreenHeight, format, kMaxTextureSize, false, false);
	tglDisable(TGL_LIGHTING);
	tglDisable(TGL_CULL_FACE); // culling uses the stored face normals (done here)
	tglEnable(TGL_DEPTH_TEST);
	tglDepthFunc(TGL_LEQUAL);
	tglShadeModel(TGL_SMOOTH);
	tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_MODULATE);
	_initialized = true;
	return true;
}

void Renderer3D::beginFrame(const Common::Rect &viewport, const Vec3 &clearColour) {
	_viewport = viewport;
	_deferred.clear();
	_bound = nullptr;
	_triangles = 0;
	tglViewport(viewport.left, kScreenHeight - viewport.bottom, viewport.width(), viewport.height());
	tglClearColor(clearColour.x, clearColour.y, clearColour.z, 1.0f);
	tglDepthMask(TGL_TRUE);
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();
}

void Renderer3D::setCamera(const Mat4 &cameraWorld, float zoom, float nearZ, float farZ) {
	_view = cameraWorld.inverse();
	_cameraPos = cameraWorld.row(3);

	// SetPerspective(d, aspectMul = 1, near, far): x_ndc = x*d/(a*z), y_ndc = y*d/z,
	// depth = (1 - d/z)/q with q = 1 - d/far; then remapped to OpenGL's [-1, 1]
	const float a = (float)_viewport.width() / (float)_viewport.height();
	const float d = zoom;
	const float q = 1.0f - d / farZ;
	Mat4 p;
	for (int i = 0; i < 16; i++)
		p.m[i] = 0.0f;
	p.at(0, 0) = 1.0f / a;
	p.at(1, 1) = 1.0f;
	p.at(2, 2) = 1.0f / (q * d);
	p.at(2, 3) = 1.0f / d;
	p.at(3, 2) = -1.0f / q;
	Mat4 remap;
	remap.at(2, 2) = 2.0f;
	remap.at(3, 2) = -1.0f;
	const Mat4 gl = p * remap;
	(void)nearZ;
	tglMatrixMode(TGL_PROJECTION);
	tglLoadMatrixf(gl.m);
	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();
}

Vec3 Renderer3D::light(const Vec3 &p, const Vec3 &n, const Vec3 &base, bool unlit,
                       const Common::Array<Light3D> &objLights) const {
	Vec3 lit = unlit ? base : base + _ambient;
	if (!unlit) {
		for (uint i = 0; i < objLights.size(); i++) {
			const Light3D &l = objLights[i];
			if (l.type == Light3D::kDirectional) {
				const float d = n.dot(l.direction);
				if (d >= 0.0f)
					lit += l.colour * d;
			} else {
				const Vec3 L = l.position - p;
				const float d = n.dot(L);
				if (d >= 0.0f) {
					const float len = L.length();
					const float f = len > 0.0f ? (l.k1 / len - l.k2) * d : 0.0f;
					if (f >= 0.0f)
						lit += l.colour * f;
				}
			}
		}
	}
	return Vec3(MIN(lit.x, 1.0f), MIN(lit.y, 1.0f), MIN(lit.z, 1.0f));
}

void Renderer3D::drawMesh(const Mesh3D &mesh, const Mat4 &world, const Common::Array<const Material3D *> &materials,
                          const Common::Array<Vec3> *vertexColours) {
	const Mat4 inv = world.inverse();
	const Mat4 mv = world * _view;
	const Vec3 eye = inv.transformPoint(_cameraPos);

	// Lights in object space (directions are not renormalised, like the original)
	Common::Array<Light3D> objLights = _lights;
	for (uint i = 0; i < objLights.size(); i++) {
		objLights[i].position = inv.transformPoint(_lights[i].position);
		objLights[i].direction = inv.transformDir(_lights[i].direction);
	}

	// Gouraud colours and view-space positions per vertex
	const uint nv = mesh.vertices.size();
	Common::Array<Vec3> lit(nv), litUnlit(nv), view(nv);
	Common::Array<bool> litDone(nv);
	for (uint i = 0; i < nv; i++) {
		view[i] = mv.transformPoint(mesh.vertices[i].pos);
		litDone[i] = false;
	}

	for (uint k = 0; k < mesh.polys.size() && k < materials.size(); k++) {
		const Material3D *m = materials[k];
		if (!m)
			continue;
		const MeshPoly &poly = mesh.polys[k];
		const MeshVertex &v0 = mesh.vertices[poly.v[0]];

		// Back-face test with the stored face normal
		if (!m->twoSided() && poly.normal.dot(eye) <= poly.normal.dot(v0.pos))
			continue;

		const bool unlitFlag = m->flags & Material3D::kFlagUnlit;
		Vec3 cols[3];
		if (m->shadeColor == 0) {
			cols[0] = cols[1] = cols[2] = Vec3(1, 1, 1);
		} else if (m->shadeColor == 1) {
			cols[0] = cols[1] = cols[2] = light(v0.pos, poly.normal, m->emissive, unlitFlag, objLights);
		} else {
			for (int j = 0; j < 3; j++) {
				const int vi = poly.v[j];
				const Vec3 &cv = vertexColours ? (*vertexColours)[vi] : mesh.vertices[vi].colour;
				if (unlitFlag) {
					cols[j] = cv;
				} else {
					if (!litDone[vi]) {
						lit[vi] = light(mesh.vertices[vi].pos, mesh.vertices[vi].normal, cv, false, objLights);
						litDone[vi] = true;
					}
					cols[j] = lit[vi];
				}
			}
		}

		Tri t;
		t.mat = m;
		t.depth = 0.0f;
		for (int j = 0; j < 3; j++) {
			Vert &v = t.v[j];
			v.pos = view[poly.v[j]];
			v.u = poly.uv[j][0];
			v.v = poly.uv[j][1];
			const Vec3 c = m->texture ? cols[j] : cols[j].mul(m->color);
			v.r = c.x;
			v.g = c.y;
			v.b = c.z;
			v.a = m->shadeAlpha == 1 ? m->alpha : (m->shadeAlpha == 2 ? mesh.vertices[poly.v[j]].alpha : 1.0f);
			t.depth = MAX(t.depth, v.pos.z);
		}

		if (m->translucent())
			_deferred.push_back(t);
		else
			emit(t);
	}
}

static TGLenum blendFactor(int f) {
	switch (f) {
	case 0: return TGL_ZERO;
	case 1: return TGL_ONE;
	case 2: return TGL_SRC_COLOR;
	case 3: return TGL_ONE_MINUS_SRC_COLOR;
	case 4: return TGL_SRC_ALPHA;
	case 5: return TGL_ONE_MINUS_SRC_ALPHA;
	case 6: return TGL_DST_COLOR;
	case 7: return TGL_ONE_MINUS_DST_COLOR;
	case 8: return TGL_DST_ALPHA;
	case 9: return TGL_ONE_MINUS_DST_ALPHA;
	default: return TGL_ONE;
	}
}

uint32 Renderer3D::textureId(Texture3D *t) {
	TextureMap::iterator it = _textures.find(t);
	if (it != _textures.end() && it->_value.version == t->version)
		return it->_value.id;

	TGLuint id;
	if (it != _textures.end()) {
		id = it->_value.id;
	} else {
		tglGenTextures(1, &id);
	}
	tglBindTexture(TGL_TEXTURE_2D, id);
	const TGLint filter = (t->flags & 1) ? TGL_LINEAR : TGL_NEAREST;
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, filter);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, filter);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, (t->flags & 4) ? TGL_CLAMP_TO_EDGE : TGL_REPEAT);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, (t->flags & 8) ? TGL_CLAMP_TO_EDGE : TGL_REPEAT);
	tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, t->size, t->size, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, t->rgba.data());
	TexEntry e;
	e.id = id;
	e.version = t->version;
	_textures[t] = e;
	_bound = nullptr; // texture binding changed
	return id;
}

void Renderer3D::bindMaterial(const Material3D *m) {
	if (m == _bound)
		return;
	if (m->texture) {
		const uint32 id = textureId(m->texture);
		tglEnable(TGL_TEXTURE_2D);
		tglBindTexture(TGL_TEXTURE_2D, id);
	} else {
		tglDisable(TGL_TEXTURE_2D);
	}
	if (m->blendSrc == 1 && m->blendDst == 0) {
		tglDisable(TGL_BLEND);
	} else {
		tglEnable(TGL_BLEND);
		tglBlendFunc(blendFactor(m->blendSrc), blendFactor(m->blendDst));
	}
	tglDepthMask(m->translucent() ? TGL_FALSE : TGL_TRUE);
	_bound = m;
}

void Renderer3D::emit(const Tri &t) {
	bindMaterial(t.mat);
	tglBegin(TGL_TRIANGLES);
	for (int j = 0; j < 3; j++) {
		const Vert &v = t.v[j];
		tglColor4f(v.r, v.g, v.b, v.a);
		tglTexCoord2f(v.u, v.v);
		tglVertex3f(v.pos.x, v.pos.y, v.pos.z);
	}
	tglEnd();
	_triangles++;
}

void Renderer3D::flushTranslucent() {
	// Translucent polygons, back to front
	Common::Array<Tri *> order;
	for (uint i = 0; i < _deferred.size(); i++)
		order.push_back(&_deferred[i]);
	Common::sort(order.begin(), order.end(), [](const Tri *a, const Tri *b) { return a->depth > b->depth; });
	for (uint i = 0; i < order.size(); i++)
		emit(*order[i]);
	_deferred.clear();
}

void Renderer3D::endFrame(Gfx &gfx) {
	flushTranslucent();
	TinyGL::presentBuffer();
	Graphics::Surface buffer;
	TinyGL::getSurfaceRef(buffer);
	Graphics::ManagedSurface &front = gfx.front();
	Common::Rect r = _viewport;
	r.clip(Common::Rect(kScreenWidth, kScreenHeight));
	for (int y = r.top; y < r.bottom; y++)
		memcpy(front.getBasePtr(r.left, y), buffer.getBasePtr(r.left, y), r.width() * buffer.format.bytesPerPixel);
}

} // End of namespace Gearhead
