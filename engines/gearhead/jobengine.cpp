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

#include "common/debug.h"
#include "graphics/managed_surface.h"

#include "gearhead/car.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobengine.h"
#include "gearhead/jobs.h"
#include "gearhead/mechanic.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

// ---------------------------------------------------------------- JobDialog

JobDialog::JobDialog() : _phase(nullptr), _result(-1) {
	_takeFocus = true; // JobDialog_ctor (0x447540)
	_ok.setOnClick(makeCallback(this, &JobDialog::onOk));
	_cancel.setOnClick(makeCallback(this, &JobDialog::onCancel));
	_restart.setOnClick(makeCallback(this, &JobDialog::onCancel));
}

JobDialog::~JobDialog() {
}

void JobDialog::init() {
	Resources *res = g_engine->_res;
	setFrame("Overlays\\Job\\JobFrame.tga");
	setPos(175, 90);
	const Common::Point o(_frameRect.left, _frameRect.top);
	_cancel.setImages(res->getImage("Overlays\\Dialog\\CancelUP.tga"), res->getImage("Overlays\\Dialog\\CancelDN.tga"),
	                  res->getImage("Overlays\\Dialog\\CancelUP.tga"));
	_restart.setImages(res->getImage("Overlays\\Dialog\\RestartUP.tga"), res->getImage("Overlays\\Dialog\\RestartDN.tga"),
	                   res->getImage("Overlays\\Dialog\\CancelUP.tga"));
	_restart.setPos(o.x + 181, o.y + 262);
	_cancel.setPos(o.x + 181, o.y + 262);
	_ok.setImages(res->getImage("Overlays\\Dialog\\OkUP.tga"), res->getImage("Overlays\\Dialog\\OkDN.tga"),
	              res->getImage("Overlays\\Dialog\\OkUP.tga"));
	_ok.setPos(o.x + 66, o.y + 262);
	_messageRect = Common::Rect(o.x + 102, o.y + 54, o.x + 294, o.y + 229);
	_portraitRect = Common::Rect(o.x + 14, o.y + 34, o.x + 89, o.y + 109);
	_difficultyRect = Common::Rect(o.x + 14, o.y + 141, o.x + 89, o.y + 166);
	_feeRect = Common::Rect(o.x + 14, o.y + 199, o.x + 89, o.y + 224);
	_hintRect = Common::Rect(o.x + 12, o.y + 232, o.x + 288, o.y + 254);
	_msgFont = &g_engine->_fonts[kFontStandard];
	// The frame and the portrait only go to the visible page; no open sound
	_framePages = kPageFront;
	_openSound = false;
}

void JobDialog::onOk(Button *b) {
	_result = 1;
}

void JobDialog::onCancel(Button *b) {
	_result = 0;
}

void JobDialog::drawContents() {
	Gfx *gfx = g_engine->_gfx;
	if (_phase && _phase->image)
		gfx->blit(kPageFront, *_phase->image, _portraitRect.left, _portraitRect.top);
	// Dialog_drawText with an empty message: the title only
	if (_titleFont && !_title.empty())
		_titleFont->drawText(*gfx, _title, _titleRect, kAlignCenter, kAlignCenter, kKeyDefault, kPageBoth);
	const BlitFont &font = g_engine->_fonts[kFontStandard];
	if (_phase)
		font.drawText(*gfx, _phase->text, _messageRect, kAlignStart, kAlignStart, kKeyColor, kPageFront);
	font.drawText(*gfx, _feeText, _feeRect, kAlignCenter, kAlignCenter, kKeyColor, kPageFront);
	font.drawText(*gfx, _difficultyText, _difficultyRect, kAlignCenter, kAlignCenter, kKeyColor, kPageFront);
	font.drawText(*gfx, _hintText, _hintRect, kAlignCenter, kAlignCenter, kKeyColor, kPageFront);
}

int JobDialog::run(const JobPhase &phase) {
	_phase = &phase;
	_ok.setState(Button::kStateUp);
	_cancel.setState(Button::kStateUp);
	_restart.setState(Button::kStateUp);
	open();
	_result = -1;
	while (_result < 0 && g_engine->runFrame()) {
	}
	close();
	return _result;
}

bool JobDialog::askRequest(Job &job, bool autoMode) {
	_title = "Job Request";
	JobPhase *phase = job.nextPhase(0);
	clearChildren();
	addChild(&_ok);
	if (!autoMode)
		addChild(&_cancel);
	_feeText = Common::String::format("$%4.2f", job.fee);
	if (job.fee < 250.0f)
		_difficultyText = "Easy";
	else if (job.fee < 550.0f)
		_difficultyText = "Medium";
	else if (job.fee < 1050.0f)
		_difficultyText = "Hard";
	else
		_difficultyText = "Expert";
	_hintText = "Hit OK to begin the job.";
	if (!phase) {
		// The original dereferences the missing phase
		static const JobPhase empty;
		return run(empty) != 0;
	}
	return run(*phase) != 0;
}

bool JobDialog::askUpdate(Job &job, bool autoMode) {
	_title = "Job Update";
	JobPhase *phase = job.nextPhase(1);
	if (!phase)
		return true;
	clearChildren();
	addChild(&_ok);
	if (!autoMode) {
		addChild(&_cancel);
		_hintText = "Hit OK to continue or CANCEL to give up on this job.";
	} else {
		addChild(&_restart);
		_hintText = "Hit OK to continue or RESTART to restart this job.";
	}
	return run(*phase) != 0;
}

void JobDialog::showComplete(Job &job) {
	_title = "Job Complete!";
	JobPhase *phase = job.nextPhase(2);
	if (!phase)
		return;
	clearChildren();
	addChild(&_ok);
	_hintText.clear();
	run(*phase);
}

// ---------------------------------------------------------------- JobEngine

JobEngine::JobEngine() : _tabImage(nullptr), _tabErase(nullptr), _current(nullptr), _offerSound(nullptr),
	_tabSound(nullptr), _savedCash(0), _cheatLatch(false), _completionRequested(false) {
	_tab.setOnClick(makeCallback(this, &JobEngine::onTabClick));
}

JobEngine::~JobEngine() {
	delete _tabImage;
	delete _tabErase;
}

void JobEngine::init() {
	JobDialog::init();
	Resources *res = g_engine->_res;
	// Resourcer_getImage caches JobTab.tga: the up and down images are the
	// same bitmap, and the portrait is pasted into it
	const Graphics::ManagedSurface *tab = res->getImage("Overlays\\Job\\JobTab.tga");
	_tabImage = new Graphics::ManagedSurface(tab->w, tab->h, tab->format);
	_tabImage->blitFrom(*tab);
	const Common::Rect r(522, 147, 522 + tab->w, 147 + tab->h);
	const Graphics::ManagedSurface *ws = res->getImage("WorkShop.tga");
	_tabErase = new Graphics::ManagedSurface(r.width(), r.height(), ws->format);
	_tabErase->simpleBlitFrom(*ws, r, Common::Point(0, 0));
	_tab.setImages(_tabImage, _tabImage, _tabErase);
	_tab.setPos(r.left, r.top);
	_offerSound = g_engine->_snd->get("Job\\bepbeep.wav");
	_tabSound = g_engine->_snd->get("Job\\com.wav");
}

void JobEngine::update() {
	Mechanic *m = g_engine->_mechanic;
	if (!_current) {
		if (m->_autoJobRequest)
			offerJob();
		return;
	}
	// Cheat: PgUp + Insert (written as 'I' + 'R', but the key table is indexed by scan code)
	Input *in = g_engine->_input;
	bool cheat = false;
	if (!_cheatLatch)
		cheat = in->isKeyDown(kScanPageUp) && in->isKeyDown(kScanInsert);
	_cheatLatch = cheat;
	if (_cheatLatch || _completionRequested || _current->isComplete()) {
		_completionRequested = false;
		endJob(true, false);
	}
}

void JobEngine::offerJob() {
	Mechanic *m = g_engine->_mechanic;
	// -JobID n (n >= 1) offers that job; otherwise the cheapest one
	Job *job = g_engine->_cmdJobId < 1 ? g_engine->_jobs.pickNext(*m, g_engine->_cars) : g_engine->_jobs.findById(g_engine->_cmdJobId);
	if (!job && g_engine->_cmdJobId >= 1)
		return;
	if (!job) {
		if (m->_id > kMechanicIdCarDev - 1)
			return;
		g_engine->_msgBox->message("No More Jobs.",
			"There are no more jobs to complete right now. Try buying, fixing, and re-selling cars from the auction to make money.",
			kDlgButtonOk);
		m->_autoJobRequest = false;
		return;
	}
	offer(job);
}

void JobEngine::offer(Job *job) {
	Mechanic *m = g_engine->_mechanic;
	g_engine->_jobs.loadPhaseImages(*job, *g_engine->_res);
	_offerSound->play();
	if (askRequest(*job, m->_autoJobRequest))
		startJob(job);
}

void JobEngine::startJob(Job *job) {
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	debugC(1, kDebugGame, "Starting job %d (car %d, fee %.2f)", job->id, job->carId, job->fee);
	_current = job;
	job->createCar(g_engine->_cars, g_engine->_rnd);
	// The save file keeps the state before the job
	m->saveToDisk();
	m->_car = job->car;
	if (job->car)
		m->ensureBank(job->car);
	_savedCash = (int)m->_cash;
	m->_cash = job->fee; // the fee is the budget of the job
	m->_job = job;
	ws->_region = 0;
	showTabPortrait(0);
	ws->addChild(&_tab);
	ws->activate();
}

void JobEngine::onTabClick(Button *b) {
	if (!_current)
		return;
	drawTab(Button::kStateDisabled);
	_tabSound->play();
	if (askUpdate(*_current, g_engine->_mechanic->_autoJobRequest))
		drawTab(Button::kStateUp);
	else
		endJob(false, false);
}

void JobEngine::endJob(bool success, bool silent) {
	if (!_current)
		return;
	Mechanic *m = g_engine->_mechanic;
	WorkShopScreen *ws = g_engine->_workShop;
	debugC(1, kDebugGame, "Ending job %d: %s", _current->id, success ? "complete" : "abandoned");
	if (!success) {
		// Everything done during the job is discarded
		if (!silent)
			m->reloadFromDisk();
	} else {
		if (g_engine->_cmdJobId > 0)
			g_engine->_cmdJobId++;
		showTabPortrait(2);
		_tab.refresh();
		if (m->_car)
			m->_car->tryStart();
		g_engine->_jukebox->playJingle();
		celebrate();
		drawTab(Button::kStateDisabled);
		g_engine->_snd->play("misc\\cashreg.wav");
		showComplete(*_current);
		if (m->_car)
			m->_car->stopEngine();
		m->_cash = (float)_savedCash + _current->fee + m->_cash;
		m->setJobCompleted(_current->id);
		m->saveToDisk();
	}
	ws->forgetCar();
	_current->destroyCar();
	_current = nullptr;
	m->_job = nullptr;
	m->_car = nullptr;
	m->_bank = nullptr;
	ws->removeChild(&_tab);
	if (!silent)
		ws->activate();
}

void JobEngine::celebrate() {
	WorkShopScreen *ws = g_engine->_workShop;
	Input *in = g_engine->_input;
	ws->setMode(0, true);
	// An empty modal pane: only the backdrop (render + clock) and the view get events
	Pane pane;
	pane.addChild(&ws->_backdrop);
	pane.addChild(&ws->_carView);
	in->releaseCapture();
	in->setRoot(&pane);
	in->setKeyTarget(nullptr);
	ws->_carView.setGoalPitch(1.2566371f);
	ws->_carView._inputDisabled = true;
	const uint32 start = g_engine->gameTimeMs();
	while (g_engine->gameTimeMs() - start < 3000 && g_engine->runFrame())
		ws->_carView.onArrowKey(kScanRight, g_engine->frameDelta());
	ws->_carView._inputDisabled = false;
	// The original leaves the (destroyed) pane as the modal handler until the
	// workshop is activated again; the workshop is the safe equivalent
	in->setRoot(ws);
	in->releaseCapture();
}

void JobEngine::showTabPortrait(int phaseType) {
	if (!_current)
		return;
	JobPhase *phase = _current->nextPhase(phaseType);
	if (!phase || !phase->image)
		return;
	_tabImage->simpleBlitFrom(*phase->image, Common::Point(21, 33));
}

void JobEngine::drawTab(int state) {
	const int old = _tab.state();
	_tab.setStateSilently(state);
	_tab.onRefresh();
	_tab.setStateSilently(old);
}

} // End of namespace Gearhead
