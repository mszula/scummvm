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

#ifndef GEARHEAD_MATH3D_H
#define GEARHEAD_MATH3D_H

#include "common/scummsys.h"

namespace Gearhead {

struct Vec3 {
	float x, y, z;

	Vec3() : x(0), y(0), z(0) {}
	Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	Vec3 operator+(const Vec3 &o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
	Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
	Vec3 &operator-=(const Vec3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
	Vec3 mul(const Vec3 &o) const { return Vec3(x * o.x, y * o.y, z * o.z); }

	float dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }
	Vec3 cross(const Vec3 &o) const { return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }
	float length() const { return sqrtf(dot(*this)); }
	float lengthSq() const { return dot(*this); }
	Vec3 normalized() const {
		const float l = length();
		return l > 0.0f ? *this * (1.0f / l) : *this;
	}
};

/**
 * 4x4 matrix in the original's row-vector convention: p' = p * M, rows 0..2
 * are the X/Y/Z axes, row 3 the translation (m[12..14]). Passing m[] to
 * glLoadMatrixf yields exactly that product, because OpenGL reads the array
 * column-major (i.e. as the transpose).
 */
struct Mat4 {
	float m[16];

	Mat4() { setIdentity(); }

	void setIdentity() {
		for (int i = 0; i < 16; i++)
			m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}

	float &at(int r, int c) { return m[r * 4 + c]; }
	float at(int r, int c) const { return m[r * 4 + c]; }

	Vec3 row(int r) const { return Vec3(m[r * 4], m[r * 4 + 1], m[r * 4 + 2]); }
	void setRow(int r, const Vec3 &v, float w) {
		m[r * 4] = v.x; m[r * 4 + 1] = v.y; m[r * 4 + 2] = v.z; m[r * 4 + 3] = w;
	}

	/** Rows = axes, translation = pos */
	static Mat4 fromAxes(const Vec3 &x, const Vec3 &y, const Vec3 &z, const Vec3 &pos) {
		Mat4 r;
		r.setRow(0, x, 0.0f);
		r.setRow(1, y, 0.0f);
		r.setRow(2, z, 0.0f);
		r.setRow(3, pos, 1.0f);
		return r;
	}

	/** this * o */
	Mat4 operator*(const Mat4 &o) const {
		Mat4 r;
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++) {
				float s = 0.0f;
				for (int k = 0; k < 4; k++)
					s += at(i, k) * o.at(k, j);
				r.at(i, j) = s;
			}
		return r;
	}

	Vec3 transformPoint(const Vec3 &p) const {
		return Vec3(p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12],
		            p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13],
		            p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14]);
	}

	Vec3 transformDir(const Vec3 &p) const {
		return Vec3(p.x * m[0] + p.y * m[4] + p.z * m[8],
		            p.x * m[1] + p.y * m[5] + p.z * m[9],
		            p.x * m[2] + p.y * m[6] + p.z * m[10]);
	}

	/** General 4x4 inverse (cofactors) */
	Mat4 inverse() const {
		const float *a = m;
		float inv[16];
		inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
		inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
		inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
		inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
		inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
		inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
		inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
		inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
		inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
		inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
		inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
		inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
		inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
		inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
		inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
		inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
		float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
		Mat4 r;
		if (det == 0.0f)
			return r;
		det = 1.0f / det;
		for (int i = 0; i < 16; i++)
			r.m[i] = inv[i] * det;
		return r;
	}
};

} // End of namespace Gearhead

#endif // GEARHEAD_MATH3D_H
