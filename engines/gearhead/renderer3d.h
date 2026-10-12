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

#ifndef GEARHEAD_RENDERER3D_H
#define GEARHEAD_RENDERER3D_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "graphics/pixelformat.h"

#include "gearhead/cardata.h"
#include "gearhead/math3d.h"

namespace Gearhead {

class Gfx;

struct Light3D {
	enum Type {
		kDirectional = 0,
		kPoint = 1
	};
	Type type = kPoint;
	Vec3 position;          ///< world space (point lights)
	Vec3 direction;         ///< world space (directional lights)
	Vec3 colour = Vec3(1, 1, 1);
	float k1 = 1.0f;        ///< intensity
	float k2 = 0.01f;       ///< intensity / range
};

/**
 * Software 3D device modelled on the VMech video drivers (docs/logic/alibi_api.md):
 * the driver transforms and lights on the CPU (ambient + directional + point
 * lights with linear fall-off, vertex colour added), culls with the stored
 * face normals and defers translucent polygons, sorted back to front.
 * Rasterisation (Z buffer, texture MODULATE, blending) is done by TinyGL.
 */
class Renderer3D {
public:
	Renderer3D();
	~Renderer3D();

	bool init(const Graphics::PixelFormat &format);

	void beginFrame(const Common::Rect &viewport, const Vec3 &clearColour);
	/** Device_setViewport without clearing (part list cells) */
	void setViewport(const Common::Rect &viewport);
	/**
	 * Camera world matrix (rows right, up, forward, position); zoom = cot(fovY / 2).
	 * SetPerspective uses aspect = (viewport w / h) * aspectMul.
	 */
	void setCamera(const Mat4 &cameraWorld, float zoom, float nearZ, float farZ, float aspectMul = 1.0f);
	void setAmbient(const Vec3 &ambient) { _ambient = ambient; }
	void clearLights() { _lights.clear(); }
	void addLight(const Light3D &light) { _lights.push_back(light); }

	/**
	 * DrawMesh: @p materials holds one material per polygon (nullptr = skip);
	 * @p vertexColours optionally replaces the vertex colours (the gloss pass writes them)
	 */
	void drawMesh(const Mesh3D &mesh, const Mat4 &world, const Common::Array<const Material3D *> &materials,
	              const Common::Array<Vec3> *vertexColours = nullptr);
	/**
	 * SpriteGroup render (0x46b240) + Billboard_draw (0x46ab60): a screen-aligned
	 * quad around the projected @p pos whose half size is @p halfSize world units
	 * at that depth (scale = viewport h * d / 2 / z pixels per unit). Only the
	 * depth is pulled @p depthBias units towards the camera (not closer than 1),
	 * so the quad is drawn over the geometry around it.
	 */
	void drawBillboard(const Vec3 &pos, float halfSize, float depthBias, const Material3D *mat);
	/** Scene(1): draw the deferred translucent polygons now (next layer) */
	void flushTranslucent();
	/** Draw the deferred translucent polygons and copy the viewport to the front page */
	void endFrame(Gfx &gfx);

	int trianglesDrawn() const { return _triangles; }

private:
	struct Vert {
		Vec3 pos; ///< view space
		float u, v;
		float r, g, b, a;
	};
	struct Tri {
		Vert v[3];
		const Material3D *mat;
		float depth;
	};

	Vec3 light(const Vec3 &p, const Vec3 &n, const Vec3 &base, bool unlit,
	           const Common::Array<Light3D> &objLights) const;
	void bindMaterial(const Material3D *m);
	void emit(const Tri &t);
	uint32 textureId(Texture3D *t);

	bool _initialized;
	Common::Rect _viewport;
	Mat4 _view;
	Vec3 _cameraPos;
	Mat4 _projection;
	float _zoom, _aspect, _far;
	Vec3 _ambient;
	Common::Array<Light3D> _lights;
	Common::Array<Tri> _deferred;
	const Material3D *_bound;
	int _triangles;

	struct TexEntry {
		uint32 id;
		uint32 version;
	};
	struct PtrHash {
		uint operator()(const Texture3D *p) const { return (uint)((uintptr)p >> 4); }
	};
	typedef Common::HashMap<Texture3D *, TexEntry, PtrHash> TextureMap;
	TextureMap _textures;
};

} // End of namespace Gearhead

#endif // GEARHEAD_RENDERER3D_H
