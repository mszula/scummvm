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

#include "common/stream.h"

#include "gearhead/mechanic.h"
#include "gearhead/tlv.h"

namespace Gearhead {

const char *skillName(int index) {
	static const char *const kSkills[5] = { "Learning", "Novice", "Handy", "Expert", "God-like" };
	return kSkills[CLIP(index, 0, 4)];
}

Common::String formatWorkTime(int seconds) {
	const int h = seconds / 3600;
	const int m = (seconds - 3600 * h) / 60;
	const int s = seconds - 3600 * h - 60 * m;
	return Common::String::format("%02i:%02i:%02i", h, m, s);
}

Mechanic::Mechanic() : _root(nullptr) {
	init("Mechanic", 0, 0.0f, 0);
}

Mechanic::~Mechanic() {
	clear();
}

void Mechanic::clear() {
	_rawBanks.clear();
	_rawAutos.clear();
	_rawDecals.clear();
	delete _root;
	_root = nullptr;
}

void Mechanic::init(const Common::String &name, int id, float cash, int skillIndex) {
	clear();
	_name = name;
	_id = id;
	_cash = cash;
	_skillIndex = skillIndex;
	_totalCareerAutos = 0;
	_totalWorkTime = 0;
	_lastAutoNum = -1;
	_jobActive = false;
	for (int i = 0; i < kCompletedJobWords; i++)
		_completedJobs[i] = 0;
	_autoJobRequest = true;
	_signInTime = 0;
}

bool Mechanic::load(Common::SeekableReadStream &stream) {
	TLVChunk *root = TLVReader::parseFile(stream, kTag_MECHANICFILE);
	if (!root)
		return false;
	const TLVChunk *body = root->find(kTag_MECHANIC);
	if (!body || !body->isList()) {
		delete root;
		return false;
	}

	init("Mechanic", 0, 0.0f, 0);
	_root = root;
	for (uint i = 0; i < body->children().size(); i++) {
		const TLVChunk *c = body->children()[i];
		switch (c->id()) {
		case kTag_MECH_NAME:
			_name = c->asString();
			break;
		case kTag_MECH_ID:
			_id = c->asInt();
			break;
		case kTag_MECH_CASH:
			_cash = c->asFloat();
			break;
		case kTag_MECH_SKILLINDEX:
			_skillIndex = c->asInt();
			break;
		case kTag_MECH_TOTALCAREERAUTOS:
			_totalCareerAutos = c->asInt();
			break;
		case kTag_MECH_TOTALWORKTIME:
			_totalWorkTime = c->asInt();
			break;
		case kTag_MECH_LASTAUTONUM:
			_lastAutoNum = c->asInt();
			break;
		case kTag_MECH_PARTSBANK:
			_rawBanks.push_back(c);
			break;
		case kTag_MECH_AUTO:
			_rawAutos.push_back(c);
			break;
		case kTag_MECH_DECAL:
			_rawDecals.push_back(c);
			break;
		case kTag_MECH_COMPLETEDJOBS: {
			const Common::Array<int32> words = c->asIntArray();
			for (uint w = 0; w < words.size() && w < kCompletedJobWords; w++)
				_completedJobs[w] = words[w];
			break;
		}
		case kTag_MECH_AUTOJOBREQUEST:
			// read as a byte by the original
			_autoJobRequest = c->data().size() > 0 && c->data()[0] != 0;
			break;
		default:
			// MECH_NUMCURAUTOS is ignored on read; unknown tags are skipped
			break;
		}
	}
	return true;
}

bool Mechanic::save(Common::WriteStream &stream) const {
	TLVWriter w;
	w.open(kTag_MECHANICFILE);
	w.begin(kTag_MECHANIC);
	w.writeString(kTag_MECH_NAME, _name);
	w.writeInt(kTag_MECH_ID, _id);
	w.writeFloat(kTag_MECH_CASH, _cash);
	w.writeInt(kTag_MECH_SKILLINDEX, _skillIndex);
	w.writeInt(kTag_MECH_NUMCURAUTOS, numCars());
	w.writeInt(kTag_MECH_TOTALCAREERAUTOS, _totalCareerAutos);
	w.writeInt(kTag_MECH_TOTALWORKTIME, _totalWorkTime);
	// Only written with a current car and no active job
	if (_lastAutoNum >= 0 && !_jobActive)
		w.writeInt(kTag_MECH_LASTAUTONUM, _lastAutoNum);
	for (uint i = 0; i < _rawBanks.size(); i++)
		w.writeChunk(*_rawBanks[i]);
	for (uint i = 0; i < _rawAutos.size(); i++)
		w.writeChunk(*_rawAutos[i]);
	for (uint i = 0; i < _rawDecals.size(); i++)
		w.writeChunk(*_rawDecals[i]);
	w.begin(kTag_MECH_COMPLETEDJOBS);
	for (int i = 0; i < kCompletedJobWords; i++)
		w.appendInt(_completedJobs[i]);
	w.end();
	w.writeInt(kTag_MECH_AUTOJOBREQUEST, _autoJobRequest ? 1 : 0);
	w.end();
	return w.close(stream);
}

bool MechSummary::load(Common::SeekableReadStream &stream) {
	TLVChunk *root = TLVReader::parseFile(stream, kTag_MECHANICFILE);
	if (!root)
		return false;
	const TLVChunk *body = root->find(kTag_MECHANIC);
	if (!body) {
		delete root;
		return false;
	}
	name = body->getString(kTag_MECH_NAME);
	id = body->getInt(kTag_MECH_ID);
	skill = skillName(body->getInt(kTag_MECH_SKILLINDEX));
	workTime = formatWorkTime(body->getInt(kTag_MECH_TOTALWORKTIME));
	cars = Common::String::format("%i", body->getInt(kTag_MECH_NUMCURAUTOS));
	cash = Common::String::format("%6.2f", body->getFloat(kTag_MECH_CASH));
	delete root;
	return true;
}

} // End of namespace Gearhead
