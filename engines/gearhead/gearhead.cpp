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
#include "common/translation.h"
#include "engines/util.h"
#include "graphics/managed_surface.h"

#include "gearhead/auction.h"
#include "gearhead/autoscript.h"
#include "gearhead/carlot.h"
#include "gearhead/catalog.h"
#include "gearhead/console.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobengine.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/screens.h"
#include "gearhead/signin.h"
#include "gearhead/sound.h"
#include "gearhead/ui.h"
#include "gearhead/workshop.h"

namespace Gearhead {

GearheadEngine *g_engine;

GearheadEngine::GearheadEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _res(nullptr), _gfx(nullptr), _input(nullptr), _snd(nullptr), _jukebox(nullptr),
	_script(nullptr), _renderer3d(nullptr), _pointer(nullptr), _mechanic(nullptr), _msgBox(nullptr), _jobEngine(nullptr),
	_cmdCarId(-1), _cmdSkill(0), _cmdJobId(-1), _signIn(nullptr), _workShop(nullptr),
	_auction(nullptr), _carLot(nullptr), _catalog(nullptr), _junkYard(nullptr), _credits(nullptr),
	_fixOriginalBugs(false), _quitRequested(false), _saveOnQuit(false), _startTime(0), _realTime(0), _gameTime(0),
	_prevGameTime(0), _frameDelta(0.1f), _pauseStart(0), _pendingLoad(-1), _pendingJob(-1), _walkBobPhase(0.0f), _walkBobPrev(0.0f) {
	g_engine = this;
	setDebugger(new Console());
}

GearheadEngine::~GearheadEngine() {
	delete _jobEngine;
	delete _signIn;
	delete _auction;
	delete _carLot;
	delete _catalog;
	delete _junkYard;
	delete _workShop;
	delete _credits;
	delete _mechanic;
	delete _msgBox;
	delete _script;
	delete _renderer3d;
	// The textures of the car models are destroyed after the renderer
	_renderer3d = nullptr;
	delete _jukebox;
	delete _snd;
	delete _input;
	delete _gfx;
	delete _res;
	g_engine = nullptr;
}

bool GearheadEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
}

// ---------------------------------------------------------------- profiles

Common::String mechanicFileName(const Common::String &target, int id) {
	return Common::String::format("%s-mechanic%d.mek", target.c_str(), id);
}

Common::String GearheadEngine::mechanicSaveName(int id) const {
	return mechanicFileName(_targetName, id);
}

// ---------------------------------------------------------------- ScummVM saves

bool GearheadEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	// A dialog open makes the dialog the root; a job is ended by the workshop's Exit only
	const Widget *root = _input ? _input->root() : nullptr;
	if (_pendingLoad < 0 && ((root == _workShop && _mechanic && !_mechanic->_job) || (root && root == _signIn)))
		return true;
	if (msg)
		*msg = _("Mechanics can be loaded in the workshop when no job is running, or on the sign-in screen.");
	return false;
}

bool GearheadEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	const Widget *root = _input ? _input->root() : nullptr;
	if (_pendingLoad < 0 && root == _workShop && _mechanic && !_mechanic->_job && !_mechanic->isDev())
		return true;
	if (msg)
		*msg = _mechanic && _mechanic->isDev() ? _("The developer mechanics are not saved.")
		                                      : _("Mechanics can be saved in the workshop when no job is running.");
	return false;
}

Common::Error GearheadEngine::loadGameState(int slot) {
	if (slot < 0 || slot >= kNumMechanicSlots || !mechanicExists(slot))
		return Common::kReadingFailed;
	_pendingLoad = slot;
	return Common::kNoError;
}

void GearheadEngine::loadPendingSlot() {
	const int slot = _pendingLoad;
	_pendingLoad = -1;
	// Signing out like the workshop's Exit saves the mechanic
	if (_mechanic && !_mechanic->isDev() && !_mechanic->_job)
		_mechanic->saveToDisk();
	if (!_signIn->signInSlot(slot))
		warning("Cannot sign in the mechanic of slot %d", slot);
}

Common::Error GearheadEngine::saveGameState(int slot, const Common::String &desc, bool isAutosave) {
	// The description is the mechanic's name, the text typed in the save dialog is not used
	if (slot < 0 || slot >= kNumMechanicSlots || !_mechanic)
		return Common::kWritingFailed;
	// In another slot the copy becomes the mechanic of that slot
	const int id = _mechanic->_id;
	_mechanic->_id = slot;
	Common::WriteStream *ws = createMechanic(slot);
	bool ok = ws && _mechanic->save(*ws);
	if (ws) {
		ws->finalize();
		ok = ok && !ws->err();
		delete ws;
	}
	_mechanic->_id = id;
	return ok ? Common::kNoError : Common::kWritingFailed;
}

void GearheadEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);
	// The game clock stands still while ScummVM's menu is open
	if (pause)
		_pauseStart = g_system->getMillis();
	else
		_startTime += g_system->getMillis() - _pauseStart;
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
	if (_pendingLoad >= 0)
		loadPendingSlot();
	if (_pendingJob >= 0 && _input->root() == _workShop && _mechanic && !_mechanic->_job) {
		Job *job = _jobs.findById(_pendingJob);
		_pendingJob = -1;
		if (job)
			_jobEngine->offer(job);
	}
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
	tickTimer();
	_input->dispatchFrame();
	_gfx->present();
	g_system->delayMillis(10);
	return !_quitRequested && !shouldQuit();
}

void GearheadEngine::tickTimer() {
	// Timer_tick (0x454d24): game time and the smoothed frame time
	const uint32 prevReal = _realTime;
	_realTime = g_system->getMillis() - _startTime;
	_frameDelta = (float)((_realTime - prevReal) / 1000.0 * 0.15 + _frameDelta * 0.85);
	_prevGameTime = _gameTime;
	_gameTime += _realTime - prevReal;
}

bool GearheadEngine::pollOnly() {
	if (_quitRequested)
		return false;
	if (!_input->poll() || shouldQuit()) {
		_saveOnQuit = true;
		_quitRequested = true;
		return false;
	}
	if (_script)
		_script->update();
	tickTimer();
	_gfx->present();
	g_system->delayMillis(10);
	return true;
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
	loadPaintColors(*_res);
	if (!loadGameFonts(*_res, _fonts))
		error("Failed to load fonts");
	_msgBox = createMessageBox();
	_pointer = _res->getImage("Pointers\\Glove.tga");
	_gfx->setCursor(_pointer, 0, 0);
	_gfx->showCursor(true);
	progress(); // FixFonts (never drawn by the original)
	progress(); // global materials
	_cars.loadAll(*_res);
	progress(); // Cars (headers; full models load on demand)
	_decals.loadAll(*_res);
	progress(); // Decals
	_jobs.loadAll(*_res, _rnd);
	progress(); // Jobs

	_workShop = new WorkShopScreen();
	_workShop->init();
	progress();
	_catalog = new CatalogScreen();
	_catalog->init();
	progress(); // Catalog
	_auction = new AuctionScreen();
	_auction->init();
	progress(); // Auction
	_carLot = new CarLotScreen();
	_carLot->init();
	progress(); // CarLot
	_junkYard = new JunkYardScreen();
	_junkYard->init();
	progress(); // JunkYard
	_signIn = new SignInScreen();
	_signIn->init();
	progress();
	_credits = new CreditsScreen();
	_credits->init();
	progress();
	_jobEngine = new JobEngine();
	_jobEngine->init();
	progress(); // JobEngine

	_res->purge();
	debugC(1, kDebugGame, "Entering main loop..");
	startGame();
}

void GearheadEngine::startGame() {
	if (_cmdCarId >= 0) {
		// "CarDev": $50000, the chosen skill, one random car of the model, no jobs
		_mechanic = new Mechanic();
		_mechanic->init("CarDev", kMechanicIdCarDev, 50000.0f, CLIP(_cmdSkill, 0, 4));
		_mechanic->addNewCar(_cmdCarId);
		_mechanic->_car = _mechanic->_cars.empty() ? nullptr : _mechanic->_cars[0];
		_mechanic->_autoJobRequest = false;
		_mechanic->_clockTime = gameTimeMs();
		_workShop->activate();
	} else if (_cmdJobId >= 0) {
		// "JobDev": $100 and job n offered at once (0 = the normal choice)
		_mechanic = new Mechanic();
		_mechanic->init("JobDev", kMechanicIdJobDev, 100.0f, CLIP(_cmdSkill, 0, 4));
		_mechanic->_clockTime = gameTimeMs();
		_workShop->activate();
		_jobEngine->offerJob();
	} else if (ConfMan.getInt("save_slot") >= 0 && _signIn->signInSlot(ConfMan.getInt("save_slot"))) {
		// Loaded from the launcher: that mechanic signs in
	} else {
		_signIn->activate(true);
	}
}

Common::Error GearheadEngine::run() {
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	if (g_system->getScreenFormat() != format)
		return Common::kUnsupportedColorMode;

	ConfMan.registerDefault("fix_original_bugs", false);
	ConfMan.registerDefault("gearhead_saves_imported", false);
	// -PrintableSnaps: the camera also saves a picture on white
	ConfMan.registerDefault("gearhead_printablesnaps", false);
	_fixOriginalBugs = ConfMan.getBool("fix_original_bugs");
	if (ConfMan.hasKey("gearhead_carid"))
		_cmdCarId = ConfMan.getInt("gearhead_carid");
	if (ConfMan.hasKey("gearhead_skill"))
		_cmdSkill = ConfMan.getInt("gearhead_skill");
	if (ConfMan.hasKey("gearhead_jobid"))
		_cmdJobId = ConfMan.getInt("gearhead_jobid");

	_startTime = g_system->getMillis();
	// srand(GetTickCount()); a fixed seed can be set for reproducible runs
	_rnd.setSeed(ConfMan.hasKey("gearhead_seed") ? (uint32)ConfMan.getInt("gearhead_seed") : g_system->getMillis());

	Common::Archive *cabinet = nullptr;
	if (_gameDescription->flags & GF_INSTALLER_CAB) {
		cabinet = Common::makeInstallShieldArchive(Common::Path("data"));
		if (!cabinet)
			return Common::Error(Common::kNoGameDataFoundError, "data1.cab");
	}

	_gfx = new Gfx();
	_gfx->init(format);
#ifdef USE_TINYGL
	_renderer3d = new Renderer3D();
	_renderer3d->init(format);
#endif
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
	if (_saveOnQuit && _mechanic && !_mechanic->_job && !(_fixOriginalBugs && _mechanic->isDev())) {
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
