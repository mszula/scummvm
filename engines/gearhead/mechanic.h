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

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Gearhead {

class TLVChunk;

enum {
	kNumMechanicSlots = 10,
	kMechanicIdCarDev = 666,
	kMechanicIdJobDev = 667,
	kCompletedJobWords = 64
};

/** SkillLevel table (g_SkillTable 0x4e664c): the display clamps the index */
const char *skillName(int index);

/** Seconds as "HH:MM:SS" (0x450843) */
Common::String formatWorkTime(int seconds);

/**
 * The player profile (Mechanic, mechanic.cpp, 0x394 bytes in the original).
 * Fields not yet reimplemented (cars, parts banks, decals) are kept as their
 * raw TLV chunks so that loading and saving a profile is lossless.
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

	Common::String _name;
	int _id;
	float _cash;
	int _skillIndex;
	int _totalCareerAutos;
	int _totalWorkTime;      ///< seconds
	int _lastAutoNum;        ///< careerNum of the current car, -1 = none
	bool _jobActive;
	int32 _completedJobs[kCompletedJobWords];
	bool _autoJobRequest;
	uint32 _signInTime;      ///< game time when the work-time clock started

	int numCars() const { return _rawAutos.size(); }
	bool isDev() const { return _id >= kMechanicIdCarDev; }

private:
	void clear();

	TLVChunk *_root;   ///< owns the raw chunks below
	Common::Array<const TLVChunk *> _rawBanks;
	Common::Array<const TLVChunk *> _rawAutos;
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
