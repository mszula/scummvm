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
#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/detection.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/jobs.h"
#include "gearhead/mechanic.h"
#include "gearhead/random.h"
#include "gearhead/resources.h"
#include "gearhead/tlv.h"

namespace Gearhead {

bool hasCompletedJob(const Mechanic &mech, int jobId) {
	return mech.isJobCompleted(jobId);
}

JobList::~JobList() {
	for (uint i = 0; i < _jobs.size(); i++)
		_jobs[i].destroyCar();
	for (uint i = 0; i < _phaseImages.size(); i++)
		delete _phaseImages[i];
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

Job *JobList::pickNext(const Mechanic &mech, CarList &cars) {
	Job *best = nullptr;
	float bestFee = 1e13f;
	for (uint i = 0; i < _jobs.size(); i++) {
		Job &job = _jobs[i];
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

Job *JobList::findById(int id) {
	for (uint i = 0; i < _jobs.size(); i++)
		if (_jobs[i].id == id)
			return &_jobs[i];
	return nullptr;
}

void JobList::loadPhaseImages(Job &job, Resources &res) {
	if (job.fileIndex < 0 || job.fileIndex >= (int)_fileNames.size())
		return;
	Common::SeekableReadStream *s = res.open("Data\\Jobs\\" + _fileNames[job.fileIndex]);
	if (!s)
		return;
	TLVChunk *root = TLVReader::parseFile(*s, kTag_JOBFILE);
	delete s;
	if (!root)
		return;
	const Common::Array<const TLVChunk *> images = root->findAll(kTag_JOBPHASEIMAGE);
	for (uint i = 0; i < job.phases.size(); i++) {
		// Pool slot i belongs to phase i; the slots are reused by later jobs
		if (_phaseImages.size() < i + 1)
			_phaseImages.push_back(nullptr);
		const int idx = job.phases[i].imageIndex;
		Graphics::ManagedSurface *img = nullptr;
		if (idx >= 0 && idx < (int)images.size())
			img = decodeBitmapSurface(*images[idx], g_engine->_gfx->format());
		if (!img)
			img = new Graphics::ManagedSurface(1, 1, g_engine->_gfx->format());
		// The slot's bitmap object is reused, so phases of other jobs that
		// still point to it see the new portrait (as in the original)
		if (!_phaseImages[i])
			_phaseImages[i] = new Graphics::ManagedSurface();
		_phaseImages[i]->copyFrom(*img);
		delete img;
		job.phases[i].image = _phaseImages[i];
	}
	delete root;
}

void Job::createCar(CarList &cars, MsvcRandom &rnd) {
	destroyCar();
	CarModel *model = cars.find(carId, true);
	if (!model)
		return;
	car = new CarInstance(model, rnd);
	car->fillPaint(carColor);
	for (uint i = 0; i < startStats.size(); i++)
		car->applyStartStat(startStats[i].type, startStats[i].subject, startStats[i].condition);
}

void Job::destroyCar() {
	delete car;
	car = nullptr;
}

JobPhase *Job::nextPhase(int type) {
	for (uint i = phaseCursor; i < phases.size(); i++) {
		if (phases[i].type == type) {
			phaseCursor = i + 1;
			return &phases[i];
		}
	}
	for (int i = 0; i < phaseCursor && i < (int)phases.size(); i++) {
		if (phases[i].type == type) {
			phaseCursor = i + 1;
			return &phases[i];
		}
	}
	phaseCursor = 0;
	return nullptr;
}

/** JobStat_isMet (0x448d73) */
static bool statMet(const JobStat &s, const CarInstance &car) {
	if (s.type == 0) {
		const Part *p = car.findPart(s.subject);
		return p && (s.condition < 0 || p->condition >= s.condition);
	}
	if (s.type == 1)
		return car.isComplete(s.subject) && car.minCondition(s.subject) >= s.condition;
	return false;
}

bool Job::isComplete() const {
	if (!car)
		return false;
	for (uint i = 0; i < completeStats.size(); i++)
		if (!statMet(completeStats[i], *car))
			return false;
	return true;
}

bool Job::isPartAllowed(const PartDef *pd) const {
	return allowCustomParts || !pd->custom;
}

} // End of namespace Gearhead
