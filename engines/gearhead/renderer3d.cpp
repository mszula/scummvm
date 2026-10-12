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

Renderer3D::Renderer3D() : _initialized(false), _zoom(1.0f), _aspect(1.0f), _far(1000.0f), _bound(nullptr), _triangles(0),
	_batchMat(nullptr), _pendingVerts(0), _call(0), _layerCall(0), _stencilNext(1), _batchStencil(0), _glStencil(0) {
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
	TinyGL::createContext(kScreenWidth, kScreenHeight, format, kMaxTextureSize, true, false, 8 * 1024 * 1024);
	tglDisable(TGL_LIGHTING);
	tglDisable(TGL_CULL_FACE); // culling uses the stored face normals (done here)
	tglEnable(TGL_DEPTH_TEST);
	tglDepthFunc(TGL_LEQUAL);
	tglShadeModel(TGL_SMOOTH);
	tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_MODULATE);
	tglDisable(TGL_STENCIL_TEST);
	tglStencilOp(TGL_KEEP, TGL_KEEP, TGL_REPLACE);
	_initialized = true;
	return true;
}

void Renderer3D::beginFrame(const Common::Rect &viewport, const Vec3 &clearColour) {
	flushBatch();
	_viewport = viewport;
	_deferred.clear();
	_bound = nullptr;
	_triangles = 0;
	_callStencil.clear();
	_stencilNext = 1;
	tglViewport(viewport.left, kScreenHeight - viewport.bottom, viewport.width(), viewport.height());
	tglClearColor(clearColour.x, clearColour.y, clearColour.z, 1.0f);
	tglDepthMask(TGL_TRUE);
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT | TGL_STENCIL_BUFFER_BIT);
	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();
}

void Renderer3D::setViewport(const Common::Rect &viewport) {
	// The deferred polygons belong to the current viewport and camera
	flushTranslucent();
	_viewport = viewport;
	tglViewport(viewport.left, kScreenHeight - viewport.bottom, viewport.width(), viewport.height());
}

void Renderer3D::setCamera(const Mat4 &cameraWorld, float zoom, float nearZ, float farZ, float aspectMul) {
	flushTranslucent();
	_view = cameraWorld.inverse();
	_cameraPos = cameraWorld.row(3);

	// SetPerspective(d, aspectMul = 1, near, far): x_ndc = x*d/(a*z), y_ndc = y*d/z,
	// depth = (1 - d/z)/q with q = 1 - d/far; then remapped to OpenGL's [-1, 1]
	const float a = (float)_viewport.width() / (float)_viewport.height() * aspectMul;
	const float d = zoom;
	_zoom = d;
	_aspect = a;
	_far = farZ;
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

/** Kept between the vertex colours and the ends of the range (less than an RGB565 step) */
static const float kColourMargin = 1.0f / 256.0f;

/**
 * TinyGL samples the pixel of a vertex at the vertex itself (not at the pixel
 * centre inside the triangle), so a vertex on the far border of the texture
 * (u = 1, or less than a texel past it) gets the first texel by wrapping, and
 * one a little before the near border the last texel: a line of the wrong
 * colour along the edge. Those coordinates are moved just inside.
 */
static void trimOvershoot(float (&uv)[3][2], int axis, float texel) {
	float lo = uv[0][axis], hi = uv[0][axis];
	for (int j = 1; j < 3; j++) {
		lo = MIN(lo, uv[j][axis]);
		hi = MAX(hi, uv[j][axis]);
	}
	const float far = ceilf(hi - texel);
	if (far > lo + texel && hi >= far && hi - far < texel) {
		for (int j = 0; j < 3; j++)
			if (uv[j][axis] >= far)
				uv[j][axis] = far - texel * 0.01f;
	}
	const float nearB = floorf(lo + texel);
	if (nearB < hi - texel && lo < nearB && nearB - lo < texel) {
		for (int j = 0; j < 3; j++)
			if (uv[j][axis] < nearB)
				uv[j][axis] = nearB;
	}
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

	const uint32 call = _layerCall ? _layerCall : ++_call;
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

		float uv[3][2];
		memcpy(uv, poly.uv, sizeof(uv));
		if (m->texture && m->texture->size > 0) {
			trimOvershoot(uv, 0, 1.0f / m->texture->size);
			trimOvershoot(uv, 1, 1.0f / m->texture->size);
		}
		Tri t;
		t.mat = m;
		t.depth = 0.0f;
		t.call = call;
		for (int j = 0; j < 3; j++) {
			Vert &v = t.v[j];
			v.pos = view[poly.v[j]];
			v.u = uv[j][0];
			v.v = uv[j][1];
			const Vec3 c = m->texture ? cols[j] : cols[j].mul(m->color);
			v.r = c.x;
			v.g = c.y;
			v.b = c.z;
			v.a = m->shadeAlpha == 1 ? m->alpha : (m->shadeAlpha == 2 ? mesh.vertices[poly.v[j]].alpha : 1.0f);
			if (m->shadeColor == 2) {
				// TinyGL steps the Gouraud colours in fixed point without a margin and
				// keeps the low byte: a value a little past 0 or 1 wraps (white or
				// black pixels along the edges, e.g. where the gloss is 0)
				v.r = CLIP(v.r, kColourMargin, 1.0f - kColourMargin);
				v.g = CLIP(v.g, kColourMargin, 1.0f - kColourMargin);
				v.b = CLIP(v.b, kColourMargin, 1.0f - kColourMargin);
			}
			if (m->shadeAlpha == 2)
				v.a = CLIP(v.a, kColourMargin, 1.0f - kColourMargin);
			t.depth = MAX(t.depth, v.pos.z);
		}

		if (m->translucent())
			_deferred.push_back(t);
		else
			emit(t);
	}
}

void Renderer3D::drawBillboard(const Vec3 &pos, float halfSize, float depthBias, const Material3D *mat) {
	Vec3 v = _view.transformPoint(pos);
	if (v.z < 1.0f)
		v.z = 1.0f;
	if (v.z >= _far)
		return;
	// Screen-space (NDC) centre and half size
	const float cx = v.x * _zoom / (_aspect * v.z), cy = v.y * _zoom / v.z;
	const float hx = halfSize * _zoom / (_aspect * v.z), hy = halfSize * _zoom / v.z;
	float z = v.z - depthBias;
	if (z < 1.0f)
		z = 1.0f;
	// The original's pre-transformed vertices are never clipped; our depth range starts at d
	z = MAX(z, _zoom * 1.001f);
	static const float kCorner[4][2] = { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } };
	static const float kUV[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	Tri t[2];
	const uint32 call = ++_call;
	for (int k = 0; k < 2; k++) {
		static const int kIdx[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
		t[k].mat = mat;
		t[k].depth = z;
		t[k].call = call;
		for (int j = 0; j < 3; j++) {
			const int i = kIdx[k][j];
			Vert &o = t[k].v[j];
			// A view-space point at depth z that projects to the wanted NDC position
			const float nx = cx + kCorner[i][0] * hx, ny = cy + kCorner[i][1] * hy;
			o.pos = Vec3(nx * _aspect * z / _zoom, ny * z / _zoom, z);
			o.u = kUV[i][0];
			o.v = kUV[i][1];
			o.r = o.g = o.b = 1.0f;
			o.a = mat->alpha;
		}
		if (mat->translucent())
			_deferred.push_back(t[k]);
		else
			emit(t[k]);
	}
}

void Renderer3D::drawScreenQuad(const Common::Rect &r, float u0, float v0, float u1, float v1, const Material3D *mat, float depth) {
	// A sprite at (or near) the far plane is a background: drawn without depth test or write
	const bool background = depth >= _far * 0.9f;
	const float z = background ? _zoom * 2.0f : MAX(depth, _zoom * 1.001f);
	const float vw = (float)_viewport.width(), vh = (float)_viewport.height();
	const float xs[2] = { (r.left - _viewport.left) / vw * 2.0f - 1.0f, (r.right - _viewport.left) / vw * 2.0f - 1.0f };
	const float ys[2] = { 1.0f - (r.top - _viewport.top) / vh * 2.0f, 1.0f - (r.bottom - _viewport.top) / vh * 2.0f };
	const float us[2] = { u0, u1 }, vs[2] = { v0, v1 };
	static const int kCorner[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	static const int kIdx[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
	if (background) {
		flushBatch();
		bindMaterial(mat);
		tglDisable(TGL_DEPTH_TEST);
		tglDepthMask(TGL_FALSE);
	}
	const uint32 call = ++_call;
	for (int k = 0; k < 2; k++) {
		Tri t;
		t.mat = mat;
		t.depth = z;
		t.call = call;
		for (int j = 0; j < 3; j++) {
			const int *cn = kCorner[kIdx[k][j]];
			Vert &o = t.v[j];
			o.pos = Vec3(xs[cn[0]] * _aspect * z / _zoom, ys[cn[1]] * z / _zoom, z);
			o.u = us[cn[0]];
			o.v = vs[cn[1]];
			o.r = o.g = o.b = 1.0f;
			o.a = mat->shadeAlpha == 1 ? mat->alpha : 1.0f;
		}
		if (background) {
			emit(t);
			continue;
		}
		if (mat->translucent())
			_deferred.push_back(t);
		else
			emit(t);
	}
	if (background) {
		flushBatch();
		tglEnable(TGL_DEPTH_TEST);
		tglDepthMask(mat->translucent() ? TGL_FALSE : TGL_TRUE);
		_bound = nullptr;
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

void Renderer3D::forgetTexture(Texture3D *t) {
	TextureMap::iterator it = _textures.find(t);
	if (it == _textures.end())
		return;
	TGLuint id = it->_value.id;
	tglDeleteTextures(1, &id);
	_textures.erase(it);
	_bound = nullptr;
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
	const byte stencil = stencilFor(t);
	if (t.mat != _batchMat || stencil != _batchStencil)
		flushBatch();
	_batchMat = t.mat;
	_batchStencil = stencil;
	_batch.push_back(t);
}

byte Renderer3D::stencilFor(const Tri &t) {
	if (t.mat->blendSrc == 1 && t.mat->blendDst == 0)
		return 0;
	Common::HashMap<uint32, byte>::const_iterator it = _callStencil.find(t.call);
	if (it != _callStencil.end())
		return it->_value;
	if (_stencilNext > 255) {
		// Out of values: start again on a cleared stencil
		flushBatch();
		tglClear(TGL_STENCIL_BUFFER_BIT);
		_callStencil.clear();
		_stencilNext = 1;
	}
	const byte value = (byte)_stencilNext++;
	_callStencil[t.call] = value;
	return value;
}

void Renderer3D::flushBatch() {
	if (_batch.empty())
		return;
	bindMaterial(_batchMat);
	if (_batchStencil != _glStencil) {
		if (!_batchStencil) {
			tglDisable(TGL_STENCIL_TEST);
		} else {
			tglEnable(TGL_STENCIL_TEST);
			tglStencilFunc(TGL_NOTEQUAL, _batchStencil, 0xff);
		}
		_glStencil = _batchStencil;
	}
	tglBegin(TGL_TRIANGLES);
	for (uint i = 0; i < _batch.size(); i++) {
		for (int j = 0; j < 3; j++) {
			const Vert &v = _batch[i].v[j];
			tglColor4f(v.r, v.g, v.b, v.a);
			tglTexCoord2f(v.u, v.v);
			tglVertex3f(v.pos.x, v.pos.y, v.pos.z);
		}
	}
	tglEnd();
	_triangles += _batch.size();
	_pendingVerts += _batch.size() * 3;
	_batch.clear();
	if (_pendingVerts > 30000) {
		TinyGL::presentBuffer();
		_pendingVerts = 0;
	}
}

void Renderer3D::flushTranslucent() {
	// Translucent polygons, back to front
	Common::Array<Tri *> order;
	for (uint i = 0; i < _deferred.size(); i++)
		order.push_back(&_deferred[i]);
	Common::sort(order.begin(), order.end(), [](const Tri *a, const Tri *b) { return a->depth > b->depth; });
	flushBatch();
	for (uint i = 0; i < order.size(); i++)
		emit(*order[i]);
	flushBatch();
	_deferred.clear();
}

void Renderer3D::endFrame(Gfx &gfx) {
	flushTranslucent();
	TinyGL::presentBuffer();
	_pendingVerts = 0;
	Graphics::Surface buffer;
	TinyGL::getSurfaceRef(buffer);
	Graphics::ManagedSurface &front = gfx.front();
	Common::Rect r = _viewport;
	r.clip(Common::Rect(kScreenWidth, kScreenHeight));
	for (int y = r.top; y < r.bottom; y++)
		memcpy(front.getBasePtr(r.left, y), buffer.getBasePtr(r.left, y), r.width() * buffer.format.bytesPerPixel);
}

} // End of namespace Gearhead
