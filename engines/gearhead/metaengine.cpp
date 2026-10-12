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

#include "common/translation.h"

#include "gearhead/metaengine.h"
#include "gearhead/detection.h"
#include "gearhead/gearhead.h"

namespace Gearhead {

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_FIX_ORIGINAL_BUGS,
		{
			_s("Fix original bugs"),
			_s("Fix bugs of the original game (e.g. auction valuation, scrap costs). Off = behave exactly like the original."),
			"fix_original_bugs",
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

} // End of namespace Gearhead

const char *GearheadMetaEngine::getName() const {
	return "gearhead";
}

const ADExtraGuiOptionsMap *GearheadMetaEngine::getAdvancedExtraGuiOptions() const {
	return Gearhead::optionsList;
}

Common::Error GearheadMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new Gearhead::GearheadEngine(syst, desc);
	return Common::kNoError;
}

bool GearheadMetaEngine::hasFeature(MetaEngineFeature f) const {
	return false;
}

#if PLUGIN_ENABLED_DYNAMIC(GEARHEAD)
REGISTER_PLUGIN_DYNAMIC(GEARHEAD, PLUGIN_TYPE_ENGINE, GearheadMetaEngine);
#else
REGISTER_PLUGIN_STATIC(GEARHEAD, PLUGIN_TYPE_ENGINE, GearheadMetaEngine);
#endif
