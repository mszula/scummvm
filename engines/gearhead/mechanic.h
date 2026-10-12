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

#ifndef GEARHEAD_MECHANIC_H
#define GEARHEAD_MECHANIC_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/car.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Gearhead {

class MsvcRandom;
class TLVChunk;
class TLVWriter;
struct Job;

enum {
	kNumMechanicSlots = 10,
	kMechanicIdCarDev = 666,
	kMechanicIdJobDev = 667,
	kCompletedJobWords = 64,
	kMaxLotCars = 12
};

/** SkillLevel table (g_SkillTable 0x4e664c): the display clamps the index */
const char *skillName(int index);

/** SkillLevel (0x20 bytes): ranges for random cars (Auto_randomize) */
struct SkillLevel {
	int index;
	const char *name;
	float condCentre[2];   ///< +0x08
	float condSpread[2];   ///< +0x10
	float partFraction[2]; ///< +0x18
};
/** SkillLevel_get (0x45f8dc): clamped to 0..4 */
const SkillLevel &skillLevel(int index);
/**
 * Auto_randomize with the ranges of a skill level, drawn fraction first, then
 * spread, then centre (Mechanic_addNewCar, Auction_generateCar)
 */
void randomizeForSkill(CarInstance *car, int skillIndex, MsvcRandom &rnd);

/** Seconds as "HH:MM:SS" (FormatDuration 0x450843, compact) */
Common::String formatWorkTime(int seconds);
/** Seconds as "1 hour and 2 minutes and 3 seconds" (FormatDuration, long form) */
Common::String formatDuration(int seconds);

/**
 * The parts of one car model the player owns (PartsBank, 0x30 bytes): the
 * parts bin shown in the workshop and the junkyard stock.
 */
struct PartsBank {
	int modelId = 0;
	Common::Array<Part *> yard; ///< +0x08, PBANK_INYARDPART
	Common::Array<Part *> bin;  ///< +0x1c, PBANK_INBINPART

	~PartsBank();
	bool load(const TLVChunk &c);
	void save(TLVWriter &w) const;
};

/**
 * The player profile (Mechanic, mechanic.cpp, 0x394 bytes in the original).
 * Fields not yet reimplemented (lot cars, decals) are kept as their raw TLV
 * chunks so that loading and saving a profile is lossless.
 */
class Mechanic {
public:
	Mechanic();
	~Mechanic();

	/** Mechanic_init (0x457cc9) */
	void init(const Common::String &name, int id, float cash, int skillIndex);

	/** Mechanic_readTLV (0x457dcf) */
	bool load(Common::SeekableReadStream &stream);
	/** Mechanic_saveToDisk (0x4584bb) + Mechanic_writeBody (0x4581db), same chunk order */
	bool save(Common::WriteStream &stream) const;
	/** Mechanic_save: write Mechanics\<id>.mek */
	void saveToDisk() const;
	/** Mechanic_saveIfReal (0x4588d2): not for dev mechanics or while a job runs */
	void saveIfReal() const;
	/** Mechanic_load (0x45861e): re-read Mechanics\<id>.mek (a job is abandoned) */
	bool reloadFromDisk();

	// Money
	/** Mechanic_canAfford (0x458e82) */
	bool canAfford(int amount) const { return _cash >= (float)amount; }
	/** Mechanic_pay (0x458ead): "You don't have the cash!" if short */
	bool pay(int amount, bool sound);
	/** Mechanic_addRepairCost (0x459418) */
	void addRepairCost(int amount);
	void drawName() const;
	/** Mechanic_drawCash (0x4592e5) */
	void drawCash() const;

	// Cars
	/** Mechanic_addNewCar (0x45898d): a random car of model @p carId (dev mechanics / tests) */
	CarInstance *addNewCar(int carId);
	/** Mechanic_buyCar (0x4591bf) */
	void buyCar(CarInstance *car, float price);
	/** Mechanic_sellCurrentCar (0x459170) */
	void sellCurrentCar(float price);
	/** The parts bank of a model (nullptr if none) */
	PartsBank *findBank(int modelId) const;

	// Parts
	/** Mechanic_ensureBank (0x458b38): the bank of the car's model, created and filled once */
	void ensureBank(CarInstance *car);
	/** Mechanic_restockYard (0x459e1a): a few parts come and go every junkyard visit */
	void restockYard();
	/** Mechanic_buyYardPart (0x458f23): junkyard -> parts bin for trunc(price) */
	bool buyYardPart(Part *part);
	/** JunkYard_returnPart (0x42776b): refund and back to the junkyard */
	void returnYardPart(Part *part);
	/** Mechanic_removePart (0x459050): car -> parts bin */
	bool removePart(Part *part);
	/** Mechanic_attachPart (0x4590bd): parts bin -> car */
	bool attachPart(Part *part);
	/** Mechanic_repairPart (0x45911c): blocks while misc\repair.wav plays */
	void repairPart(Part *part);
	/** Mechanic_sellPart (0x458f9e): scrap a bin part to the junkyard */
	void sellPart(Part *part);
	/** Mechanic_updateCurrentCar (0x458ab7) */
	void updateCurrentCar(int region);

	// Messages (the standard message box, Mechanic+0x80)
	void msgNoCash();
	void msgCarLotFull();
	void msgAssemblyError(const AssemblyError &err);
	void msgCarComplete();

	// Clock and skill
	/** Mechanic_update (0x45946a) + Mechanic_tickSkillAndTime (0x45939f) */
	void update();
	/** Mechanic_checkSkill (0x45a0ac) */
	void checkSkill();
	/** Mechanic_advanceSkill (0x45a175) */
	void advanceSkill(int level);

	// Jobs
	bool isJobCompleted(int jobId) const;
	/** Mechanic_setJobCompleted (0x459c3a) */
	void setJobCompleted(int jobId);
	/** Mechanic_countCompletedJobs (0x459da4) */
	int countCompletedJobs() const;

	Common::String _name;
	int _id;
	float _cash;
	int _skillIndex;
	int _totalCareerAutos;
	int _totalWorkTime;      ///< seconds
	int _lastAutoNum;        ///< careerNum of the current car, -1 = none
	int32 _completedJobs[kCompletedJobWords];
	bool _autoJobRequest;
	uint32 _clockTime;       ///< +0x18: game time of the last work-time update
	uint32 _skillCheckTime;  ///< g_LastSkillCheck

	Common::Array<PartsBank *> _banks;  ///< +0x380 (owned)
	Common::Array<CarInstance *> _cars; ///< +0x360 the car lot (owned)
	CarInstance *_car;   ///< +0x358 current car (a lot car or the job car; not owned)
	Job *_job;           ///< +0x374 current job
	PartsBank *_bank;    ///< +0x37c parts bank of the current car's model

	int numCars() const { return _cars.size(); }
	bool isDev() const { return _id >= kMechanicIdCarDev; }

private:
	void clear();
	void seedBank(CarInstance *car, PartsBank *bank);

	TLVChunk *_root;   ///< owns the raw chunks below
	Common::Array<const TLVChunk *> _rawDecals;
};

/** What the sign-in roster shows of a profile (MechSummary 0x42d060, MechInfo_loadHeader 0x45a413) */
struct MechSummary {
	Common::String name;
	int id;
	Common::String skill;
	Common::String workTime;
	Common::String cars;
	Common::String cash;

	bool load(Common::SeekableReadStream &stream);
};

} // End of namespace Gearhead

#endif // GEARHEAD_MECHANIC_H
