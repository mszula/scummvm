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

#ifndef GEARHEAD_CAR_H
#define GEARHEAD_CAR_H

#include "common/array.h"

#include "gearhead/cardata.h"

namespace Gearhead {

class MsvcRandom;

/** One physical part on a car (Part, 0x18 bytes) */
struct InstalledPart {
	const PartDef *def;
	int condition;  ///< 0 Scrap .. 3 Perfect
	float wiggle;   ///< PART_VALUEWIGGLE, rand in [-0.2, -0.1]
};

/** A car instance (Auto, auto.cpp) */
class CarInstance {
public:
	CarInstance(CarModel *model, MsvcRandom &rnd);

	CarModel *model() const { return _model; }

	/** Fill the paint texture with one colour (16 bpp: quantised to RGB565 like the original) */
	void fillPaint(const Vec3 &colour);

	InstalledPart *findPart(int id);
	bool isInstalled(int id) const;
	/** Auto_findMutexPart: an installed part sharing a mutex bit */
	bool hasMutexConflict(const PartDef *pd) const;

	/** Auto_installRandomPart (0x43db0c): the part and, recursively, its missing attach dependencies */
	bool installRandomPart(int id, int condition, int spread, bool allowCustom);
	/** Auto_removePartRecursive (0x43deef) */
	void removePartRecursive(int id);
	/** JobStat_apply (0x448e10) for one start stat */
	void applyStartStat(int type, int subject, int condition);
	/** Auto_isComplete: no non-custom part of the region missing that could be attached */
	bool isComplete(int region) const;

	/** Auto_showRegion (0x43e123): mesh visibility and per-polygon materials */
	void showRegion(int region, bool showSpec);

	Common::Array<InstalledPart> _parts;
	Texture3D _paintTexture;
	Material3D _paintMaterial;

	Common::Array<bool> _meshVisible;
	Common::Array<bool> _specVisible;
	Common::Array<Common::Array<const Material3D *> > _polyMaterials;

private:
	const Material3D *fileMaterial(int index) const;
	bool canAttach(const PartDef *pd) const;

	CarModel *_model;
	MsvcRandom &_rnd;
};

} // End of namespace Gearhead

#endif // GEARHEAD_CAR_H
