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

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"
#include "common/translation.h"

#include "gearhead/metaengine.h"
#include "gearhead/detection.h"
#include "gearhead/gearhead.h"
#include "gearhead/input.h"

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

Common::KeymapArray GearheadMetaEngine::initKeymaps(const char *target) const {
	using namespace Common;
	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "gearhead-default", _("Default keymappings"));
	Action *act;

	act = new Action(kStandardActionLeftClick, _("Left click"));
	act->setLeftClickEvent();
	act->addDefaultInputMapping("MOUSE_LEFT");
	act->addDefaultInputMapping("JOY_A");
	keymap->addAction(act);

	act = new Action(kStandardActionRightClick, _("Right click (hold to turn the car)"));
	act->setRightClickEvent();
	act->addDefaultInputMapping("MOUSE_RIGHT");
	act->addDefaultInputMapping("JOY_B");
	keymap->addAction(act);

	static const struct {
		const char *id;
		const char *description;
		Gearhead::KeyAction action;
		const char *key;
		const char *joy;
	} kKeys[] = {
		{ "UP", _s("Turn the car up / walk forward"), Gearhead::kActionUp, "UP", "JOY_UP" },
		{ "DOWN", _s("Turn the car down / walk back"), Gearhead::kActionDown, "DOWN", "JOY_DOWN" },
		{ "LEFT", _s("Turn the car left / walk left"), Gearhead::kActionLeft, "LEFT", "JOY_LEFT" },
		{ "RIGHT", _s("Turn the car right / walk right"), Gearhead::kActionRight, "RIGHT", "JOY_RIGHT" },
		{ "BACK", _s("Back / cancel"), Gearhead::kActionBack, "ESCAPE", "JOY_X" }
	};
	for (uint i = 0; i < ARRAYSIZE(kKeys); i++) {
		act = new Action(kKeys[i].id, _(kKeys[i].description));
		act->setCustomEngineActionEvent(kKeys[i].action);
		act->addDefaultInputMapping(kKeys[i].key);
		act->addDefaultInputMapping(kKeys[i].joy);
		keymap->addAction(act);
	}
	return Keymap::arrayOf(keymap);
}

#if PLUGIN_ENABLED_DYNAMIC(GEARHEAD)
REGISTER_PLUGIN_DYNAMIC(GEARHEAD, PLUGIN_TYPE_ENGINE, GearheadMetaEngine);
#else
REGISTER_PLUGIN_STATIC(GEARHEAD, PLUGIN_TYPE_ENGINE, GearheadMetaEngine);
#endif
