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
#include "gearhead/gearhead.h"
#include "gearhead/random.h"
#include "gearhead/sound.h"

namespace Gearhead {

// ---------------------------------------------------------------- Part

float Part::value() const {
	if (condition == 0)
		return 0.0f;
	const double v = (condition / 3.0) * (double)(def->costMax - def->costMin) * (wiggle + 1.0f) + def->costMin;
	return (float)MAX(0.0, v);
}

float Part::junkPrice() const {
	const double d = (double)(def->costMax - def->costMin) * 0.6666666666666666 + def->costMin - def->costMin;
	const double v = (condition / 3.0) * d * (wiggle + 1.0f) + def->costMin;
	return (float)MAX(0.0, v);
}

float Part::repairCost() const {
	const double v = ((double)wiggle + 0.5) * (double)(def->costMax - value());
	return (float)MAX(0.0, v);
}

Part *createPart(const CarModel *model, const PartDef *def, int condition, MsvcRandom &rnd) {
	Part *p = new Part();
	p->wiggle = rnd.frand(0.1f) - 0.2f;
	p->def = def;
	p->modelId = model->_id;
	p->condition = (def->special == 4) ? 3 : condition;
	return p;
}

// ---------------------------------------------------------------- CarInstance

CarInstance::CarInstance(CarModel *model, MsvcRandom &rnd) : _model(model), _rnd(rnd), _repairTimeMs(0),
	_repairCost(0.0f), _purchaseCost(0.0f), _careerNum(0), _isTop(false), _running(false), _falseStart(false) {
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

CarInstance::~CarInstance() {
	stopEngine();
	for (uint i = 0; i < _parts.size(); i++)
		delete _parts[i];
}

void CarInstance::fillPaint(const Vec3 &colour) {
	// Image_setPixelColor on a 16-bit image: quantise to RGB565
	const int r5 = CLIP((int)(colour.x * 31.0f + 0.5f), 0, 31);
	const int g6 = CLIP((int)(colour.y * 63.0f + 0.5f), 0, 63);
	const int b5 = CLIP((int)(colour.z * 31.0f + 0.5f), 0, 31);
	_paintTexture.fill((byte)(r5 << 3 | r5 >> 2), (byte)(g6 << 2 | g6 >> 4), (byte)(b5 << 3 | b5 >> 2));
}

Part *CarInstance::findPart(int id) const {
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i]->def->id == id)
			return _parts[i];
	return nullptr;
}

Part *CarInstance::findMutexPart(int id) const {
	const PartDef *pd = _model->findPart(id);
	if (!pd || !pd->mutexSet)
		return nullptr;
	for (uint i = 0; i < _parts.size(); i++)
		if (_parts[i]->def->mutexSet & pd->mutexSet)
			return _parts[i];
	return nullptr;
}

bool CarInstance::hasMutexConflict(const PartDef *pd) const {
	return findMutexPart(pd->id) != nullptr;
}

bool CarInstance::canRemove(const Part *part, AssemblyError &err) const {
	err.code = AssemblyError::kBlocked;
	err.ids.clear();
	if (!isInstalled(part->def->id))
		return false;
	const Common::Array<int> &deps = part->def->removeDep;
	for (uint i = 0; i < deps.size() && i < 16; i++) {
		if (deps[i] < 0)
			break;
		if (findPart(deps[i]))
			err.ids.push_back(deps[i]);
		if (findMutexPart(deps[i]))
			err.ids.push_back(deps[i]);
	}
	return err.ids.empty();
}

bool CarInstance::canInstall(const Part *part, AssemblyError &err) const {
	if (part->condition == 0) {
		err.code = AssemblyError::kBroken;
		err.ids.clear();
		err.ids.push_back(part->def->id);
		return false;
	}
	return canAttachId(part->def->id, &err);
}

bool CarInstance::canAttachId(int id, AssemblyError *err) const {
	AssemblyError local;
	AssemblyError &e = err ? *err : local;
	e.code = AssemblyError::kMissingDeps;
	e.ids.clear();
	if (findPart(id)) {
		e.code = AssemblyError::kAlreadyInstalled;
		e.ids.push_back(id);
		return false;
	}
	const Part *conflict = findMutexPart(id);
	if (conflict) {
		e.code = AssemblyError::kReplaces;
		e.ids.push_back(conflict->def->id);
		return false;
	}
	const PartDef *pd = _model->findPart(id);
	if (!pd)
		return false;
	for (uint i = 0; i < pd->attachDep.size() && i < 16; i++) {
		const int dep = pd->attachDep[i];
		if (dep < 0)
			break;
		// PD_AMEA: an installed mutually-exclusive variant satisfies the dependency
		if (!findPart(dep) && (!pd->amea || !findMutexPart(dep)))
			e.ids.push_back(dep);
	}
	return e.ids.empty();
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
	const int c = (int)(lo + _rnd.frand(hi - lo));
	_parts.push_back(createPart(_model, pd, c, _rnd));
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
		if (_parts[i]->def->id == id) {
			delete _parts[i];
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
		if (!isInstalled(pd->id) && canAttachId(pd->id, nullptr))
			return false;
	}
	return true;
}

int CarInstance::minCondition(int region) const {
	int m = 3;
	for (uint i = 0; i < _parts.size(); i++) {
		if (region != 0 && _parts[i]->def->region != region)
			continue;
		m = MIN(m, _parts[i]->condition);
	}
	return m;
}

void CarInstance::applyStartStat(int type, int subject, int condition) {
	if (type == 0) {
		Part *p = findPart(subject);
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

void CarInstance::applyBaseMaterials(const Part *part) {
	const PartDef *pd = part->def;
	if (pd->meshIndex < 0 || pd->meshIndex >= (int)_polyMaterials.size())
		return;
	Common::Array<const Material3D *> &mats = _polyMaterials[pd->meshIndex];
	if (part->condition == 3) {
		if (pd->multiMaterials.empty())
			return;
		uint k = 0, j = 0;
		for (uint c = 0; c < mats.size(); c++) {
			mats[c] = fileMaterial(pd->multiMaterials[MIN<uint>(k, pd->multiMaterials.size() - 1)]);
			if (j < pd->switchPolys.size() && (int)c == pd->switchPolys[j] - 1) {
				k++;
				j++;
			}
		}
	} else {
		for (uint c = 0; c < mats.size(); c++)
			mats[c] = conditionMaterial(part->condition);
	}
}

void CarInstance::applyPartMaterials(const Part *part) {
	applyBaseMaterials(part);
	const PartDef *pd = part->def;
	if (part->condition != 3 || pd->meshIndex < 0 || pd->meshIndex >= (int)_polyMaterials.size())
		return;
	Common::Array<const Material3D *> &mats = _polyMaterials[pd->meshIndex];
	for (uint c = 0; c < pd->paintPolys.size(); c++)
		if (pd->paintPolys[c] < (int)mats.size())
			mats[pd->paintPolys[c]] = &_paintMaterial;
}

void CarInstance::setPartMaterial(const Part *part, const Material3D *mat) {
	const int mi = part->def->meshIndex;
	if (mi < 0 || mi >= (int)_polyMaterials.size())
		return;
	Common::Array<const Material3D *> &mats = _polyMaterials[mi];
	for (uint c = 0; c < mats.size(); c++)
		mats[c] = mat;
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
		const Part *part = _parts[i];
		const PartDef *pd = part->def;
		if (region != 0 && pd->region != region)
			continue;
		if (region == 0 && !pd->visibleInCar)
			continue;
		if (pd->meshIndex < 0 || !meshes[pd->meshIndex])
			continue;
		applyPartMaterials(part);
		_meshVisible[pd->meshIndex] = true;
		if (showSpec && part->condition == 3 && pd->meshIndex < (int)_model->_specMeshes.size() && _model->_specMeshes[pd->meshIndex])
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

// ---------------------------------------------------------------- Engine

void CarInstance::tryStart() {
	SoundManager *snd = g_engine->_snd;
	_shake = Vec3();
	_running = false;
	_falseStart = false;
	bool starter = false;
	for (uint i = 0; i < _parts.size(); i++) {
		if (_parts[i]->def->special == 2) {
			starter = true;
			break;
		}
	}
	if (!starter) {
		snd->play("engine\\false2.wav");
		return;
	}
	if (isComplete(1) && minCondition(1) > 1) {
		_running = true;
		bool played = false;
		for (uint i = 0; i < _model->_ignitionSounds.size(); i++) {
			if (findPart(_model->_ignitionSounds[i].partId) && _model->_ignitionSounds[i].sound) {
				_model->_ignitionSounds[i].sound->play();
				played = true;
			}
		}
		if (!played)
			snd->play("misc\\carstart.wav");
		return;
	}
	snd->play("engine\\false1.wav");
	_falseStart = true;
}

void CarInstance::shakeEngine(const Vec3 &offset) {
	for (uint i = 0; i < _model->_parts.size(); i++) {
		const PartDef *pd = _model->_parts[i];
		if (pd->region != 1 || pd->meshIndex < 0 || !_model->_meshes[pd->meshIndex])
			continue;
		Mesh3D *m = _model->_meshes[pd->meshIndex];
		m->local.setRow(3, m->local.row(3) + offset, 1.0f);
	}
}

bool CarInstance::updateEngine() {
	SoundManager *snd = g_engine->_snd;
	if (_running) {
		bool playing = snd->get("misc\\carstart.wav")->isPlaying();
		for (uint i = 0; i < _model->_ignitionSounds.size(); i++)
			if (_model->_ignitionSounds[i].sound && _model->_ignitionSounds[i].sound->isPlaying())
				playing = true;
		if (!playing)
			return false;
	}
	if (_falseStart && !snd->get("engine\\false1.wav")->isPlaying())
		return false;

	bool moveBack = false;
	if (_shake.lengthSq() >= 1e-05f) {
		_shake = _shake * -1.0f;
		moveBack = true;
	} else {
		const float z = 0.4f - _rnd.frand(0.8f);
		const float y = 0.4f - _rnd.frand(0.8f);
		const float x = 0.4f - _rnd.frand(0.8f);
		_shake = Vec3(x, y, z);
	}
	shakeEngine(_shake);
	// Parts with PD_SPECIAL 3 (crankshaft, flywheel, fan) spin 36 degrees per update
	const float a = 0.62831855f;
	Mat4 rot;
	rot.setRow(0, Vec3(cosf(a), sinf(a), 0.0f), 0.0f);
	rot.setRow(1, Vec3(-sinf(a), cosf(a), 0.0f), 0.0f);
	for (uint i = 0; i < _model->_parts.size(); i++) {
		const PartDef *pd = _model->_parts[i];
		if (pd->region != 1 || pd->special != 3 || pd->meshIndex < 0 || !_model->_meshes[pd->meshIndex])
			continue;
		Mesh3D *m = _model->_meshes[pd->meshIndex];
		const Vec3 loc = m->local.row(3);
		Mat4 r = rot * m->local;
		r.setRow(3, loc, 1.0f);
		m->local = r;
	}
	if (moveBack)
		_shake = Vec3();
	return true;
}

void CarInstance::stopEngine() {
	if (!g_engine || !g_engine->_snd)
		return;
	_running = false;
	_falseStart = false;
	SoundManager *snd = g_engine->_snd;
	snd->get("engine\\false1.wav")->stop();
	snd->get("misc\\carstart.wav")->stop();
	for (uint i = 0; i < _model->_ignitionSounds.size(); i++)
		if (_model->_ignitionSounds[i].sound)
			_model->_ignitionSounds[i].sound->stop();
	if (_shake.lengthSq() >= 1e-05f) {
		_shake = _shake * -1.0f;
		shakeEngine(_shake);
		_shake = Vec3();
	}
}

} // End of namespace Gearhead
