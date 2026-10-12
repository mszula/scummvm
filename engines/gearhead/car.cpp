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

#include "gearhead/car.h"
#include "gearhead/random.h"

namespace Gearhead {

CarInstance::CarInstance(CarModel *model, MsvcRandom &rnd) : _model(model), _rnd(rnd) {
	// Auto_createPaintMaterial (0x43d348)
	_paintTexture.create(kMaxTextureSize);
	_paintTexture.name = Common::String::format("PaintOnTex%d", (int)_rnd.frand(99999.0f));
	_paintMaterial.shadeColor = 2;
	_paintMaterial.shadeAlpha = 0;
	_paintMaterial.flags |= Material3D::kFlagTwoSided;
	_paintMaterial.texture = &_paintTexture;
	_paintMaterial.name = "Paint";
	fillPaint(paintColor((int)_rnd.frand(26.01f)));
}

void CarInstance::fillPaint(const Vec3 &colour) {
	// Image_setPixelColor on a 16-bit image: quantise to RGB565
	const int r5 = CLIP((int)(colour.x * 31.0f + 0.5f), 0, 31);
	const int g6 = CLIP((int)(colour.y * 63.0f + 0.5f), 0, 63);
	const int b5 = CLIP((int)(colour.z * 31.0f + 0.5f), 0, 31);
	_paintTexture.fill((byte)(r5 << 3 | r5 >> 2), (byte)(g6 << 2 | g6 >> 4), (byte)(b5 << 3 | b5 >> 2));
}

InstalledPart *CarInstance::findPart(int id) {
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i].def->id == id)
			return &_parts[i];
	return nullptr;
}

bool CarInstance::isInstalled(int id) const {
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i].def->id == id)
			return true;
	return false;
}

bool CarInstance::hasMutexConflict(const PartDef *pd) const {
	if (!pd->mutexSet)
		return false;
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i].def->mutexSet & pd->mutexSet)
			return true;
	return false;
}

bool CarInstance::canAttach(const PartDef *pd) const {
	if (isInstalled(pd->id) || hasMutexConflict(pd))
		return false;
	for (uint i = 0; i < pd->attachDep.size(); i++) {
		const int dep = pd->attachDep[i];
		if (dep < 0)
			break;
		if (isInstalled(dep))
			continue;
		// PD_AMEA: a part from the same mutex set satisfies the dependency
		const PartDef *dd = _model->findPart(dep);
		bool ok = false;
		if (pd->amea && dd && dd->mutexSet)
			ok = hasMutexConflict(dd);
		if (!ok)
			return false;
	}
	return true;
}

bool CarInstance::installRandomPart(int id, int condition, int spread, bool allowCustom) {
	const PartDef *pd = _model->findPart(id);
	if (!pd)
		return false;
	if (isInstalled(id))
		return true;
	if (pd->custom && !allowCustom)
		return false;
	if (hasMutexConflict(pd))
		return false;
	// Missing attach dependencies first, with the same condition
	for (uint i = 0; i < pd->attachDep.size(); i++) {
		if (pd->attachDep[i] < 0)
			break;
		if (!isInstalled(pd->attachDep[i]))
			installRandomPart(pd->attachDep[i], condition, spread, allowCustom);
	}
	const float lo = MAX(0.0f, (float)(condition - spread));
	const float hi = MIN(3.99f, (float)(condition + spread + 1));
	InstalledPart p;
	p.def = pd;
	p.condition = (int)(lo + _rnd.frand(hi - lo));
	if (pd->special == 4)
		p.condition = 3; // always perfect
	p.wiggle = _rnd.frand(0.1f) - 0.2f;
	_parts.push_back(p);
	return true;
}

void CarInstance::removePartRecursive(int id) {
	const PartDef *pd = _model->findPart(id);
	if (!pd)
		return;
	for (uint i = 0; i < pd->removeDep.size(); i++) {
		if (pd->removeDep[i] < 0)
			break;
		if (isInstalled(pd->removeDep[i]))
			removePartRecursive(pd->removeDep[i]);
	}
	for (uint i = 0; i < _parts.size(); i++) {
		if (_parts[i].def->id == id) {
			_parts.remove_at(i);
			break;
		}
	}
}

bool CarInstance::isComplete(int region) const {
	for (uint i = 0; i < _model->_parts.size(); i++) {
		const PartDef *pd = _model->_parts[i];
		if (pd->custom || (region != 0 && pd->region != region))
			continue;
		if (canAttach(pd))
			return false;
	}
	return true;
}

void CarInstance::applyStartStat(int type, int subject, int condition) {
	if (type == 0) {
		InstalledPart *p = findPart(subject);
		if (!p) {
			if (condition >= 0)
				installRandomPart(subject, condition, 0, true);
		} else if (condition < 0) {
			removePartRecursive(subject);
		} else {
			p->condition = condition;
		}
	} else if (type == 1) {
		for (uint i = 0; i < _model->_parts.size(); i++) {
			const PartDef *pd = _model->_parts[i];
			if ((subject != 0 && pd->region != subject) || pd->special == 4 || pd->custom)
				continue;
			installRandomPart(pd->id, condition, 0, false);
			if (isComplete(subject))
				break;
		}
	}
}

const Material3D *CarInstance::fileMaterial(int index) const {
	if (index == kPaintMaterialIndex)
		return &_paintMaterial;
	if (index >= 0 && index < (int)_model->_materials.size())
		return _model->_materials[index];
	return nullptr;
}

void CarInstance::showRegion(int region, bool showSpec) {
	const Common::Array<Mesh3D *> &meshes = _model->_meshes;
	_meshVisible.clear();
	_meshVisible.resize(meshes.size());
	_specVisible.clear();
	_specVisible.resize(meshes.size());
	_polyMaterials.resize(meshes.size());
	for (uint i = 0; i < meshes.size(); i++) {
		_meshVisible[i] = false;
		_specVisible[i] = false;
		if (!meshes[i])
			continue;
		_polyMaterials[i].resize(meshes[i]->polys.size());
		for (uint k = 0; k < meshes[i]->polys.size(); k++)
			_polyMaterials[i][k] = fileMaterial(meshes[i]->polys[k].fileMaterial);
	}

	for (uint i = 0; i < _parts.size(); i++) {
		const InstalledPart &part = _parts[i];
		const PartDef *pd = part.def;
		if (region != 0 && pd->region != region)
			continue;
		if (region == 0 && !pd->visibleInCar)
			continue;
		if (pd->meshIndex < 0 || !meshes[pd->meshIndex])
			continue;
		Common::Array<const Material3D *> &mats = _polyMaterials[pd->meshIndex];

		// Part_applyBaseMaterials (0x438950)
		if (part.condition == 3 && !pd->multiMaterials.empty()) {
			uint k = 0, j = 0;
			for (uint c = 0; c < mats.size(); c++) {
				mats[c] = fileMaterial(pd->multiMaterials[MIN<uint>(k, pd->multiMaterials.size() - 1)]);
				if (j < pd->switchPolys.size() && (int)c == pd->switchPolys[j] - 1) {
					k++;
					j++;
				}
			}
		} else if (part.condition < 3) {
			for (uint c = 0; c < mats.size(); c++)
				mats[c] = conditionMaterial(part.condition);
		}
		// Part_applyMaterials (0x438a67): perfect body panels show the paint
		if (part.condition == 3) {
			for (uint c = 0; c < pd->paintPolys.size(); c++)
				if (pd->paintPolys[c] < (int)mats.size())
					mats[pd->paintPolys[c]] = &_paintMaterial;
		}

		_meshVisible[pd->meshIndex] = true;
		if (showSpec && part.condition == 3 && pd->meshIndex < (int)_model->_specMeshes.size() && _model->_specMeshes[pd->meshIndex])
			_specVisible[pd->meshIndex] = true;
	}

	// Shell meshes ("?0..."): always shown in their region, paint polys painted
	static const char kRegionPrefix[4] = { '0', '#', '$', '^' };
	for (uint i = 0; i < meshes.size(); i++) {
		const Mesh3D *m = meshes[i];
		if (!m || !m->isShell())
			continue;
		if (region != 0 && (region > 3 || m->name[0] != kRegionPrefix[region]))
			continue;
		_meshVisible[i] = true;
		if (showSpec && i < _model->_specMeshes.size() && _model->_specMeshes[i])
			_specVisible[i] = true;
		for (uint k = 0; k < m->polys.size(); k++)
			if (m->polys[k].fileMaterial == kPaintMaterialIndex)
				_polyMaterials[i][k] = &_paintMaterial;
	}
}

} // End of namespace Gearhead
