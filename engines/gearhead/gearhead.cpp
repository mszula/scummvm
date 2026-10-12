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

#include "common/path.h"
#include "common/compression/installshield_cab.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/managed_surface.h"

#include "gearhead/autoscript.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/resources.h"
#include "gearhead/screens.h"
#include "gearhead/signin.h"
#include "gearhead/sound.h"

namespace Gearhead {

GearheadEngine *g_engine;

GearheadEngine::GearheadEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _res(nullptr), _gfx(nullptr), _input(nullptr), _snd(nullptr), _jukebox(nullptr),
	_script(nullptr), _pointer(nullptr), _mechanic(nullptr), _signIn(nullptr), _workShop(nullptr), _credits(nullptr),
	_fixOriginalBugs(false), _quitRequested(false), _saveOnQuit(false), _startTime(0) {
	g_engine = this;
}

GearheadEngine::~GearheadEngine() {
	delete _signIn;
	delete _workShop;
	delete _credits;
	delete _mechanic;
	delete _script;
	delete _jukebox;
	delete _snd;
	delete _input;
	delete _gfx;
	delete _res;
	g_engine = nullptr;
}

bool GearheadEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
}

uint32 GearheadEngine::gameTimeMs() const {
	return g_system->getMillis() - _startTime;
}

// ---------------------------------------------------------------- profiles

Common::String GearheadEngine::mechanicSaveName(int id) const {
	return Common::String::format("%s-mechanic%d.mek", _targetName.c_str(), id);
}

Common::SeekableReadStream *GearheadEngine::openMechanic(int id) {
	return _saveFileMan->openForLoading(mechanicSaveName(id));
}

Common::WriteStream *GearheadEngine::createMechanic(int id) {
	debugC(1, kDebugGame, "Saving Mechanic %d to %s", id, mechanicSaveName(id).c_str());
	return _saveFileMan->openForSaving(mechanicSaveName(id), false);
}

void GearheadEngine::deleteMechanic(int id) {
	_saveFileMan->removeSavefile(mechanicSaveName(id));
}

bool GearheadEngine::mechanicExists(int id) {
	Common::SeekableReadStream *s = openMechanic(id);
	delete s;
	return s != nullptr;
}

void GearheadEngine::importOriginalSaves() {
	// Profiles of an installed original (Data\Mechanics\<id>.mek) are copied once
	if (ConfMan.getBool("gearhead_saves_imported"))
		return;
	for (int id = 0; id < kNumMechanicSlots; id++) {
		if (mechanicExists(id))
			continue;
		Common::SeekableReadStream *in = _res->open(Common::String::format("Data\\Mechanics\\%d.mek", id));
		if (!in)
			continue;
		Common::WriteStream *out = createMechanic(id);
		if (out) {
			byte buf[4096];
			while (!in->eos()) {
				const uint32 n = in->read(buf, sizeof(buf));
				if (!n)
					break;
				out->write(buf, n);
			}
			out->finalize();
			delete out;
			debug("Imported original mechanic profile %d", id);
		}
		delete in;
	}
	ConfMan.setBool("gearhead_saves_imported", true);
	ConfMan.flushToDisk();
}

// ---------------------------------------------------------------- main loop

bool GearheadEngine::runFrame() {
	if (_quitRequested)
		return false;
	if (!_input->poll() || shouldQuit()) {
		// Closing the window / quitting through ScummVM behaves like Alt+F4
		_saveOnQuit = true;
		_quitRequested = true;
		return false;
	}
	if (_script)
		_script->update();
	if (_input->isKeyDown(kScanAlt) && _input->isKeyDown(kScanF4)) {
		_saveOnQuit = true;
		_quitRequested = true;
		return false;
	}
	_input->dispatchFrame();
	_gfx->present();
	g_system->delayMillis(10);
	return !_quitRequested && !shouldQuit();
}

void GearheadEngine::waitMs(uint32 ms, bool skippable) {
	const uint32 end = g_system->getMillis() + ms;
	while (g_system->getMillis() < end && !shouldQuit()) {
		const uint before = _input->buttons();
		if (!_input->poll())
			break;
		if (_script)
			_script->update();
		if (skippable && _input->buttons() && !before)
			break;
		_gfx->present();
		g_system->delayMillis(10);
	}
}

void GearheadEngine::runStartup() {
	// 1. Black screen with the Mekada logo, centred at (320, 240), shown by the pitch detector
	_gfx->clear(kPageBoth);
	Graphics::ManagedSurface *logo = _res->getImage("Splash\\Mekada.tga");
	_gfx->blit(kPageFront, *logo, 320 - logo->w / 2, 240 - logo->h / 2);
	_res->release(logo);
	_gfx->present();
	waitMs(1500, true);

	// 2. Loading screen, start-up sound, music track 1
	Graphics::ManagedSurface *loading1 = _res->getImage("Splash\\Loading1.tga");
	_gfx->blit(kPageBoth, *loading1, 0, 0);
	_res->release(loading1);
	_snd->play("misc\\gamego.wav");
	_jukebox->playTrack(1);
	_gfx->present();

	// 3. 13-step progress bar revealing Splash\Loading2.tga in (71,303)-(268,425)
	const Common::Rect barRect(71, 303, 268, 425);
	Graphics::ManagedSurface *loading2 = _res->getImage("Splash\\Loading2.tga");
	Graphics::ManagedSurface bar(barRect.width(), barRect.height(), loading2->format);
	bar.simpleBlitFrom(*loading2, barRect, Common::Point(0, 0));
	_res->release(loading2);

	const int kSteps = 13;
	int step = 0;
	auto progress = [&]() {
		step++;
		// ProgressBar_draw: columns 0..trunc((w-1)*cur/max), only while cur < 12
		if (step < 12) {
			const int cols = (int)((double)(bar.w - 1) * step / kSteps) + 1;
			_gfx->blitRect(kPageFront, bar, Common::Rect(cols, bar.h), barRect.left, barRect.top);
		}
		_input->poll();
		_gfx->present();
	};

	// Paint colours, blit fonts, pointer
	if (!loadGameFonts(*_res, _fonts))
		error("Failed to load fonts");
	_pointer = _res->getImage("Pointers\\Glove.tga");
	_gfx->setCursor(_pointer, 0, 0);
	_gfx->showCursor(true);
	progress(); // FixFonts (never drawn by the original)
	progress(); // global materials
	progress(); // Cars (not implemented yet)
	progress(); // Decals (not implemented yet)
	progress(); // Jobs (not implemented yet)

	_workShop = new WorkShopScreen();
	_workShop->init();
	progress();
	progress(); // Catalog
	progress(); // Auction
	progress(); // CarLot
	progress(); // JunkYard
	_signIn = new SignInScreen();
	_signIn->init();
	progress();
	_credits = new CreditsScreen();
	_credits->init();
	progress();
	progress(); // JobEngine

	_res->purge();
	debugC(1, kDebugGame, "Entering main loop..");
	_signIn->activate(true);
}

Common::Error GearheadEngine::run() {
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	if (g_system->getScreenFormat() != format)
		return Common::kUnsupportedColorMode;

	ConfMan.registerDefault("fix_original_bugs", false);
	ConfMan.registerDefault("gearhead_saves_imported", false);
	_fixOriginalBugs = ConfMan.getBool("fix_original_bugs");

	_startTime = g_system->getMillis();
	_rnd.setSeed(g_system->getMillis()); // srand(GetTickCount())

	Common::Archive *cabinet = nullptr;
	if (_gameDescription->flags & GF_INSTALLER_CAB) {
		cabinet = Common::makeInstallShieldArchive(Common::Path("data"));
		if (!cabinet)
			return Common::Error(Common::kNoGameDataFoundError, "data1.cab");
	}

	_gfx = new Gfx();
	_gfx->init(format);
	_res = new Resources();
	_res->init(cabinet, format);
	_input = new Input();
	_snd = new SoundManager(*_res);
	_jukebox = new JukeBox(*_snd);

	if (ConfMan.hasKey("gearhead_script")) {
		_script = new AutoScript();
		if (!_script->load(ConfMan.get("gearhead_script"))) {
			warning("Cannot read automation script %s", ConfMan.get("gearhead_script").c_str());
			delete _script;
			_script = nullptr;
		}
	}

	importOriginalSaves();
	runStartup();

	while (runFrame()) {
	}

	// Alt+F4 saves the current mechanic when no job is active. The original
	// does not check for the dev mechanics (CarDev/JobDev); the Exit button
	// of the sign-in screen and a normal shutdown save nothing.
	if (_saveOnQuit && _mechanic && !_mechanic->_jobActive && !(_fixOriginalBugs && _mechanic->isDev())) {
		Common::WriteStream *ws = createMechanic(_mechanic->_id);
		if (ws) {
			_mechanic->save(*ws);
			ws->finalize();
			delete ws;
		}
	}
	return Common::kNoError;
}

} // End of namespace Gearhead
