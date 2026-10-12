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

#ifndef GEARHEAD_H
#define GEARHEAD_H

#include "common/error.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "engines/engine.h"

#include "gearhead/cardata.h"
#include "gearhead/detection.h"
#include "gearhead/font.h"
#include "gearhead/jobs.h"
#include "gearhead/random.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Graphics {
class ManagedSurface;
}

/**
 * This is the namespace of the Gearhead Garage engine, a reimplementation of
 * the VMech engine of "Snap-on Gearhead Garage" (Ratloop / Mekada, 1999).
 *
 * Status: start-up sequence, Sign In, the workshop with the job system.
 *
 * Games using this engine:
 * - Snap-on Gearhead Garage
 */
namespace Gearhead {

class AutoScript;
class CreditsScreen;
class Gfx;
class Input;
class JobEngine;
class JukeBox;
class Mechanic;
class PreFabDialog;
class Renderer3D;
class Resources;
class SignInScreen;
class SoundManager;
class WorkShopScreen;

class GearheadEngine : public Engine {
public:
	GearheadEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~GearheadEngine() override;

	bool hasFeature(EngineFeature f) const override;

	/** Option "fix_original_bugs": off = behave exactly like the original */
	bool fixOriginalBugs() const { return _fixOriginalBugs; }

	/**
	 * One pass of the main loop: pump input (and the automation script),
	 * dispatch the frame's events, tick the root pane and present the screen.
	 * Returns false when the engine should stop.
	 */
	bool runFrame();
	/** Busy-wait like the original splash/modal loops, but keep the window alive */
	void waitMs(uint32 ms, bool skippable = false);
	/**
	 * EventMgr_poll (0x4538ff) alone: advance the clock and read the devices
	 * without dispatching events, used by the loops that wait for a sound.
	 */
	bool pollOnly();

	/** Game time in ms (g_GameTimeMs), advanced once per frame by Timer_tick (0x454d24) */
	uint32 gameTimeMs() const { return _gameTime; }
	/** g_PrevTimeMs: the game time of the previous frame */
	uint32 prevGameTimeMs() const { return _prevGameTime; }
	/** Smoothed frame time in seconds (g_FrameDelta, Timer_update 0x454d24) */
	float frameDelta() const { return _frameDelta; }

	/** Ask the main loop to end without saving (the sign-in screen's Exit / quit flag) */
	void requestQuit() { _quitRequested = true; }

	// Mechanic profiles, one per sign-in slot (originals: Data\Mechanics\<id>.mek)
	Common::String mechanicSaveName(int id) const;
	Common::SeekableReadStream *openMechanic(int id);
	Common::WriteStream *createMechanic(int id);
	void deleteMechanic(int id);
	bool mechanicExists(int id);

	Resources *_res;
	Gfx *_gfx;
	Input *_input;
	SoundManager *_snd;
	JukeBox *_jukebox;
	AutoScript *_script;
	BlitFont _fonts[kFontCount];
	Renderer3D *_renderer3d;
	CarList _cars;
	JobList _jobs;
	MsvcRandom _rnd;

	Graphics::ManagedSurface *_pointer;
	Mechanic *_mechanic;
	PreFabDialog *_msgBox;   ///< the standard OK/Cancel message box (Mechanic+0x80)
	JobEngine *_jobEngine;

	SignInScreen *_signIn;
	WorkShopScreen *_workShop;
	CreditsScreen *_credits;

protected:
	Common::Error run() override;

private:
	void importOriginalSaves();
	void runStartup();
	void tickTimer();

	const ADGameDescription *_gameDescription;
	bool _fixOriginalBugs;
	bool _quitRequested;
	bool _saveOnQuit;   ///< Alt+F4 (and quitting through ScummVM) saves the mechanic
	uint32 _startTime;
	uint32 _realTime;
	uint32 _gameTime;
	uint32 _prevGameTime;
	float _frameDelta;
};

extern GearheadEngine *g_engine;

} // End of namespace Gearhead

#endif // GEARHEAD_H
