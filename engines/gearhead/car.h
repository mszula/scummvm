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
class TLVChunk;
class TLVWriter;

/**
 * One physical part (Part, 0x18 bytes). Parts move between a car, the
 * player's parts bin and the junkyard stock as objects, so they are always
 * handled by pointer.
 */
struct Part {
	const PartDef *def = nullptr;
	int modelId = 0;     ///< PART_AUTOID
	int condition = 3;   ///< 0 Scrap .. 3 Perfect
	float wiggle = 0.0f; ///< PART_VALUEWIGGLE, rand in [-0.2, -0.1]

	/** Part_getValue (0x4387ea) */
	float value() const;
	/** Part_getJunkPrice (0x43886c): junkyard price and scrap value */
	float junkPrice() const;
	/** Part_getRepairCost (0x438901) */
	float repairCost() const;
};

/** Part_ctor (0x438590): random wiggle, then Part_init (special 4 parts are always perfect) */
Part *createPart(const CarModel *model, const PartDef *def, int condition, MsvcRandom &rnd);
/** Part_load (0x438664): the condition defaults to 3; nullptr if the PartData does not exist */
Part *loadPart(const TLVChunk &c);
/** Part_save (0x438753) as list @p tag */
void savePart(TLVWriter &w, uint tag, const Part &p);

/** Mechanic+0x34 (PartCheckResult): why a part cannot be attached or removed */
struct AssemblyError {
	enum {
		kAlreadyInstalled = 1,
		kMissingDeps = 2,
		kBlocked = 3,
		kBroken = 4,
		kReplaces = 5
	};
	int code = 0;
	Common::Array<int> ids;
};

/** A car instance (Auto, auto.cpp) */
class CarInstance {
public:
	CarInstance(CarModel *model, MsvcRandom &rnd);
	~CarInstance();

	CarModel *model() const { return _model; }

	/** Fill the paint texture with one colour (16 bpp: quantised to RGB565 like the original) */
	void fillPaint(const Vec3 &colour);

	/** Auto_findPart (0x43d73b) */
	Part *findPart(int id) const;
	bool isInstalled(int id) const { return findPart(id) != nullptr; }
	/** Auto_findMutexPart (0x43d79f): an installed part sharing a mutex bit with part @p id */
	Part *findMutexPart(int id) const;
	bool hasMutexConflict(const PartDef *pd) const;

	/** Auto_canRemove (0x43d9f6) */
	bool canRemove(const Part *part, AssemblyError &err) const;
	/** Auto_canInstall (0x43d820): broken parts cannot go on */
	bool canInstall(const Part *part, AssemblyError &err) const;
	/** Auto_canAttachId (0x43d87e) */
	bool canAttachId(int id, AssemblyError *err) const;

	/**
	 * Auto_installRandomPart (0x43db0c): install a part in random condition around
	 * @p centre; if only attach dependencies are missing, every dependency is
	 * installed first (quirk: an already installed one aborts). Returns
	 * "more parts wanted" (count < maxParts).
	 */
	bool installRandomPart(int id, float centre, float spread, int maxParts, bool allowCustom);
	/** Auto_randomize (0x43dcc2): a shuffled fraction of the stock parts */
	void randomize(float centre, float spread, float fraction);
	/** Auto_removePartRecursive (0x43deef) */
	void removePartRecursive(int id);
	/** JobStat_apply (0x448e10) for one start stat */
	void applyStartStat(int type, int subject, int condition);
	/** Auto_isComplete (0x43e45b): no non-custom part of the region missing that could be attached */
	bool isComplete(int region) const;
	/** Auto_getMinCondition (0x43e542): 3 if no part of the region is installed */
	int minCondition(int region) const;

	/** Auto_showRegion (0x43e123): mesh visibility and per-polygon materials */
	void showRegion(int region, bool showSpec);
	/** Part_applyMaterials (0x438a67) for one installed part */
	void applyPartMaterials(const Part *part);
	/** Give every polygon of the part's mesh one material (condition highlights) */
	void setPartMaterial(const Part *part, const Material3D *mat);
	/** Mesh index of a part's PD_MESHNAME (-1 if the model has none) */
	int partMesh(const PartDef *pd) const { return pd ? pd->meshIndex : -1; }

	/** Auto_load (0x43cf96) from a MECH_AUTO list; the model is created by the caller */
	void load(const TLVChunk &c);
	/** Auto_save (0x43d210) as MECH_AUTO */
	void save(TLVWriter &w) const;

	/** Auto_tryStart (0x43e5d4) */
	void tryStart();
	/** Auto_updateEngine (0x43e730): false when the engine (or the false start) has stopped */
	bool updateEngine();
	/** Auto_stopEngine (0x43eafc) */
	void stopEngine();
	bool isRunning() const { return _running; }
	bool isFalseStarting() const { return _falseStart; }

	Common::Array<Part *> _parts;   ///< installed parts (owned)
	Texture3D _paintTexture;
	Material3D _paintMaterial;

	int _repairTimeMs;    ///< CAR_REPAIRTIME
	float _repairCost;    ///< CAR_REPAIRCOST
	float _purchaseCost;  ///< CAR_PURCHASECOST
	int _careerNum;       ///< CAR_CAREERNUM
	bool _isTop;          ///< CAR_ISTOP: fully restored, the ledger stops

	Common::Array<bool> _meshVisible;
	Common::Array<bool> _specVisible;
	Common::Array<Common::Array<const Material3D *> > _polyMaterials;

private:
	const Material3D *fileMaterial(int index) const;
	void applyBaseMaterials(const Part *part);
	void shakeEngine(const Vec3 &offset);

	CarModel *_model;
	MsvcRandom &_rnd;
	bool _running;
	bool _falseStart;
	Vec3 _shake;
};

} // End of namespace Gearhead

#endif // GEARHEAD_CAR_H
