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

#include "gearhead/car.h"
#include "gearhead/detection.h"
#include "gearhead/jobs.h"
#include "gearhead/mechanic.h"
#include "gearhead/random.h"
#include "gearhead/resources.h"
#include "gearhead/tlv.h"

namespace Gearhead {

bool hasCompletedJob(const Mechanic &mech, int jobId) {
	if (jobId < 0 || jobId >= kCompletedJobWords * 32)
		return false;
	return (mech._completedJobs[jobId / 32] >> (jobId % 32)) & 1;
}

static void readStats(const TLVChunk &c, uint tag, Common::Array<JobStat> &out) {
	const Common::Array<const TLVChunk *> list = c.findAll(tag);
	for (uint i = 0; i < list.size(); i++) {
		JobStat s;
		s.type = list[i]->getInt(kTag_JS_TYPE, 0);
		s.subject = list[i]->getInt(kTag_JS_SUBJECT, 0);
		s.condition = list[i]->getInt(kTag_JS_CONDITION, -1);
		out.push_back(s);
	}
}

void JobList::loadAll(Resources &res, MsvcRandom &rnd) {
	const Common::StringArray files = res.list("Data\\Jobs", "*.jpk");
	for (uint f = 0; f < files.size(); f++) {
		Common::SeekableReadStream *s = res.open("Data\\Jobs\\" + files[f]);
		if (!s)
			continue;
		TLVChunk *root = TLVReader::parseFile(*s, kTag_JOBFILE);
		delete s;
		if (!root)
			continue;
		_fileNames.push_back(files[f]);
		const Common::Array<const TLVChunk *> jobs = root->findAll(kTag_JOBDATA);
		for (uint j = 0; j < jobs.size(); j++) {
			const TLVChunk &c = *jobs[j];
			Job job;
			// Job_ctor (0x4461e8): a random default colour
			job.carColor = paintColor((int)rnd.frand(26.01f));
			job.fileIndex = _fileNames.size() - 1;
			job.id = c.getInt(kTag_JOB_ID);
			job.carId = c.getInt(kTag_JOB_CARID);
			job.fee = c.getFloat(kTag_JOB_FEE);
			if (c.find(kTag_JOB_CARCOLOR)) {
				const Common::Array<float> col = c.find(kTag_JOB_CARCOLOR)->asFloatArray();
				if (col.size() >= 3)
					job.carColor = Vec3(col[0], col[1], col[2]);
			}
			if (c.find(kTag_JOB_LEGALMODES)) {
				const Common::Array<byte> &lm = c.find(kTag_JOB_LEGALMODES)->data();
				for (uint k = 0; k < 4; k++)
					job.legalModes[k] = k * 4 < lm.size() && lm[k * 4] != 0;
			}
			job.allowCustomParts = c.getInt(kTag_JOB_ALLOWCUSTOMPARTS, 1) != 0;
			const Common::Array<const TLVChunk *> phases = c.findAll(kTag_JOB_PHASE);
			for (uint k = 0; k < phases.size(); k++) {
				JobPhase p;
				p.type = phases[k]->getInt(kTag_JP_TYPE);
				p.text = phases[k]->getString(kTag_JP_TEXT);
				p.imageIndex = phases[k]->getInt(kTag_JP_IMAGEINDEX);
				job.phases.push_back(p);
			}
			readStats(c, kTag_JOB_STARTSTAT, job.startStats);
			readStats(c, kTag_JOB_COMPLETESTAT, job.completeStats);
			_jobs.push_back(job);
		}
		delete root;
	}
	debugC(1, kDebugGame, "Loaded %d jobs from %d files", _jobs.size(), _fileNames.size());
}

const Job *JobList::pickNext(const Mechanic &mech, CarList &cars) const {
	const Job *best = nullptr;
	float bestFee = 1e13f;
	for (uint i = 0; i < _jobs.size(); i++) {
		const Job &job = _jobs[i];
		if (job.fee <= bestFee && !hasCompletedJob(mech, job.id)) {
			CarModel *model = cars.find(job.carId, false);
			if (model && model->_minSkill <= mech._skillIndex) {
				best = &job;
				bestFee = job.fee;
			}
		}
	}
	return best;
}

const Job *JobList::findById(int id) const {
	for (uint i = 0; i < _jobs.size(); i++)
		if (_jobs[i].id == id)
			return &_jobs[i];
	return nullptr;
}

CarInstance *Job::createCar(CarList &cars, MsvcRandom &rnd) const {
	CarModel *model = cars.find(carId, true);
	if (!model)
		return nullptr;
	CarInstance *car = new CarInstance(model, rnd);
	car->fillPaint(carColor);
	for (uint i = 0; i < startStats.size(); i++)
		car->applyStartStat(startStats[i].type, startStats[i].subject, startStats[i].condition);
	return car;
}

} // End of namespace Gearhead
