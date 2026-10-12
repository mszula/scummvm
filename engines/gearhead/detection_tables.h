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

namespace Gearhead {

const PlainGameDescriptor gearheadGames[] = {
	{ "gearhead", "Snap-on Gearhead Garage" },
	{ nullptr, nullptr }
};

const ADGameDescription gameDescriptions[] = {
	// Installed game (directory containing ghg.exe and Data/)
	{
		"gearhead",
		nullptr,
		AD_ENTRY2s("ghg.exe",        "bdc8422923677902dfb626a3cc45798b", 1015849,
		           "Gfx24.dat",      "e889d45bfd558f5429d6c087c43b492f", 7687101),
		Common::EN_ANY,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO2(GUIO_NOMIDI, GAMEOPTION_FIX_ORIGINAL_BUGS)
	},

	// Original CD (Build 131, 1999-10-27): data read directly from the InstallShield cabinet
	{
		"gearhead",
		"CD",
		AD_ENTRY2s("data1.cab",      "f361efb6a808b2407ff6fddf66467fc8", 27970716,
		           "Setup.Ins",      "02d71326cfd0cf2178d62296e4f70888", 58912),
		Common::EN_ANY,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | GF_INSTALLER_CAB,
		GUIO2(GUIO_NOMIDI, GAMEOPTION_FIX_ORIGINAL_BUGS)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Gearhead
