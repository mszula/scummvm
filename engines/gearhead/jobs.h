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

#ifndef GEARHEAD_JOBS_H
#define GEARHEAD_JOBS_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/math3d.h"

namespace Graphics {
class ManagedSurface;
}

namespace Gearhead {

class CarInstance;
class CarList;
class Mechanic;
class MsvcRandom;
class Resources;
struct PartDef;

struct JobStat {
	int type = 0;        ///< 0 = one part (subject = PD_ID), 1 = region (0 all, 1 engine, 2 body, 3 running gear)
	int subject = 0;
	int condition = -1;  ///< 0 Scrap .. 3 Perfect, -1 = absent
};

struct JobPhase {
	int type = 0;        ///< 0 request, 1 hint/update, 2 thank-you
	Common::String text;
	int imageIndex = 0;
	Graphics::ManagedSurface *image = nullptr; ///< portrait, loaded by Job_loadPhaseImages (pool of JobList)
};

/** Job (0x70 bytes, job file record JOBDATA) */
struct Job {
	int id = 0;
	int carId = 0;
	int fileIndex = 0;
	float fee = 0.0f;
	Vec3 carColor;
	bool legalModes[4] = { true, true, true, true };
	bool allowCustomParts = true;
	Common::Array<JobPhase> phases;
	Common::Array<JobStat> startStats;
	Common::Array<JobStat> completeStats;

	int phaseCursor = 0;          ///< +0x30
	CarInstance *car = nullptr;   ///< +0x0c, owned while the job runs

	/** Job_createAuto (0x446369): a new car painted JOB_CARCOLOR, then the start stats */
	void createCar(CarList &cars, MsvcRandom &rnd);
	/** Job_destroyAuto (0x4464e7) */
	void destroyCar();
	/** Job_nextPhase (0x446852): cycles through the phases of one type */
	JobPhase *nextPhase(int type);
	/** Job_isComplete (0x4467c7): every JOB_COMPLETESTAT met */
	bool isComplete() const;
	/** Job_isPartAllowed (0x446824) */
	bool isPartAllowed(const PartDef *pd) const;
};

class JobList {
public:
	~JobList();
	/** Jobs_loadAll (0x445792): every JOBDATA of Data\Jobs\*.jpk */
	void loadAll(Resources &res, MsvcRandom &rnd);
	/** Jobs_pickNext (0x445936): cheapest uncompleted job whose car the skill allows (later one wins ties) */
	Job *pickNext(const Mechanic &mech, CarList &cars);
	Job *findById(int id);
	/** Job_loadPhaseImages (0x446532): portraits from the job's .jpk into the shared pool */
	void loadPhaseImages(Job &job, Resources &res);

	Common::Array<Job> _jobs;
	Common::Array<Common::String> _fileNames;

private:
	Common::Array<Graphics::ManagedSurface *> _phaseImages; ///< g_JobPhaseImages, slot = phase number
};

/** Mechanic::hasCompletedJob (bit test of MECH_COMPLETEDJOBS) */
bool hasCompletedJob(const Mechanic &mech, int jobId);

} // End of namespace Gearhead

#endif // GEARHEAD_JOBS_H
