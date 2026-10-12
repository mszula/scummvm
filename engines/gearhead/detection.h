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

#ifndef GEARHEAD_DETECTION_H
#define GEARHEAD_DETECTION_H

#include "engines/advancedDetector.h"

namespace Gearhead {

enum GearheadDebugChannels {
	kDebugResources = 1,
	kDebugGraphics,
	kDebugUI,
	kDebugSound,
	kDebugGame,
	kDebugScript
};

enum GearheadGameFlags {
	/** Data comes straight from the CD (InstallShield data1.cab) */
	GF_INSTALLER_CAB = (1 << 0)
};

extern const PlainGameDescriptor gearheadGames[];

extern const ADGameDescription gameDescriptions[];

#define GAMEOPTION_FIX_ORIGINAL_BUGS GUIO_GAMEOPTIONS1

} // End of namespace Gearhead

class GearheadMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	GearheadMetaEngineDetection();
	~GearheadMetaEngineDetection() override {}

	const char *getName() const override {
		return "gearhead";
	}

	const char *getEngineName() const override {
		return "Gearhead Garage";
	}

	const char *getOriginalCopyright() const override {
		return "Snap-on Gearhead Garage (C) 1999 Ratloop, Inc. / Mekada";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // GEARHEAD_DETECTION_H
