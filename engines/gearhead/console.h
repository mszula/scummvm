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

#ifndef GEARHEAD_CONSOLE_H
#define GEARHEAD_CONSOLE_H

#include "gui/debugger.h"

namespace Gearhead {

/**
 * The debugger console (Ctrl+Alt+D): the mechanic's state, a few cheats and
 * the dumps of the screens. The automation script runs the same commands.
 */
class Console : public GUI::Debugger {
public:
	Console();

	/** Runs one command line; false when the command is unknown */
	bool runLine(const Common::String &line);

	/** debugPrintf, and the log too when the command comes from the automation script */
	void print(const char *format, ...) GCC_PRINTF(2, 3);

private:
	bool cmdCash(int argc, const char **argv);
	bool cmdSkill(int argc, const char **argv);
	bool cmdJobs(int argc, const char **argv);
	bool cmdJob(int argc, const char **argv);
	bool cmdCompleteJob(int argc, const char **argv);
	bool cmdGiveCar(int argc, const char **argv);
	bool cmdBuyPart(int argc, const char **argv);
	bool cmdMechanics(int argc, const char **argv);
	bool cmdDump(int argc, const char **argv);

	bool _fromScript;
};

/** A line of a dump: to the log, and to the console while it is open */
void dumpLine(const char *format, ...) GCC_PRINTF(1, 2);

} // End of namespace Gearhead

#endif // GEARHEAD_CONSOLE_H
