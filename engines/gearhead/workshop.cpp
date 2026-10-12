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

#include "gearhead/auction.h"
#include "gearhead/car.h"
#include "gearhead/carlot.h"
#include "gearhead/catalog.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/jobengine.h"
#include "gearhead/jobs.h"
#include "gearhead/junkyard.h"
#include "gearhead/mechanic.h"
#include "gearhead/resources.h"
#include "gearhead/signin.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/** Button_initFromScreens with the three workshop sheets */
static void initFromWorkshopSheets(ImageButton &b, const Common::Rect &r) {
	Resources *res = g_engine->_res;
	b.setFromSheets(r, *res->getImage("WorkShopUP.tga"), *res->getImage("WorkShopDN.tga"), *res->getImage("WorkShop.tga"));
}

static void notImplemented(const char *what) {
	warning("%s is not implemented yet", what);
}

// ---------------------------------------------------------------- Tool

void Tool::setCursor(const Common::String &image) {
	_cursor = g_engine->_res->getImage(image);
}

// ---------------------------------------------------------------- WorkShopBackdrop

void WorkShopBackdrop::onRefresh() {
	// WorkShopBackdrop_onTick (0x419665): the clock and the 3D view keep going
	g_engine->_mechanic->update();
	g_engine->_workShop->render();
}

// ---------------------------------------------------------------- RegionTabs

RegionTabs::RegionTabs() {
	setRect(2, 44, 508, 92);
	for (int i = 0; i < 4; i++) {
		_tabs[i]._tag = i;
		_tabs[i].setRadio(this, 1, makeCallback(this, &RegionTabs::onTab));
		_sounds[i] = nullptr;
	}
	// ModeTabs_ctor (0x410ed0): child order BODY, RUNNING GEAR, ENGINE, COMPLETE
	addChild(&_tabs[2]);
	addChild(&_tabs[3]);
	addChild(&_tabs[1]);
	addChild(&_tabs[0]);
}

void RegionTabs::init() {
	static const Common::Rect kRects[4] = {
		Common::Rect(3, 46, 124, 76), Common::Rect(139, 46, 229, 76), Common::Rect(244, 46, 315, 76), Common::Rect(330, 46, 504, 76)
	};
	static const char *const kSounds[4] = { "lock\\lock10.wav", "klunk\\klunk14.wav", "button\\button3.wav", "klunk\\klunk13.wav" };
	for (int i = 0; i < 4; i++) {
		initFromWorkshopSheets(_tabs[i], kRects[i]);
		_sounds[i] = g_engine->_snd->get(kSounds[i]);
		_tabs[i].setPressSound(_sounds[i]);
	}
}

void RegionTabs::updateStates() {
	const int region = g_engine->_workShop->_region;
	for (int i = 0; i < 4; i++)
		_tabs[i].setStateSilently(region == i ? Button::kStateDown : Button::kStateUp);
	const Job *job = g_engine->_mechanic->_job;
	if (job) {
		for (int i = 1; i < 4; i++)
			if (!job->legalModes[i])
				_tabs[i].setStateSilently(Button::kStateDisabled);
	}
}

void RegionTabs::onTab(Button *b) {
	g_engine->_workShop->setMode(b->_tag, true);
}

// ---------------------------------------------------------------- CarActionsPane

CarActionsPane::CarActionsPane() {
	setRect(518, 99, 638, 316);
	_showCondition.setOnPress(makeCallback(this, &CarActionsPane::onShowConditionPress));
	_showCondition.setOnRelease(makeCallback(this, &CarActionsPane::onShowConditionRelease));
	_putInLot.setOnClick(makeCallback(this, &CarActionsPane::onPutInLot));
	_catalog.setOnClick(makeCallback(this, &CarActionsPane::onCatalog));
	_auctionCar.setOnClick(makeCallback(this, &CarActionsPane::onAuctionCar));
	_getJob.setOnClick(makeCallback(this, &CarActionsPane::onGetJob));
	addChild(&_showCondition);
	addChild(&_catalog);
	addChild(&_auctionCar);
	addChild(&_putInLot);
	addChild(&_getJob);
}

void CarActionsPane::init() {
	initFromWorkshopSheets(_showCondition, Common::Rect(522, 103, 640, 133));
	initFromWorkshopSheets(_putInLot, Common::Rect(522, 147, 640, 177));
	initFromWorkshopSheets(_auctionCar, Common::Rect(522, 235, 640, 265));
	initFromWorkshopSheets(_catalog, Common::Rect(522, 279, 640, 309));
	initFromWorkshopSheets(_getJob, Common::Rect(522, 191, 640, 221));
	SoundManager *snd = g_engine->_snd;
	_catalog.setSounds(snd->get("catalog\\catalog13.wav"), snd->get("button\\button10.wav"), nullptr);
	_showCondition.setSounds(nullptr, snd->get("misc\\condition1.wav"), snd->get("misc\\condition2.wav"));
}

void CarActionsPane::updateStates() {
	Mechanic *m = g_engine->_mechanic;
	_showCondition.setStateSilently(Button::kStateUp);
	_auctionCar.setStateSilently(Button::kStateUp);
	_catalog.setStateSilently(Button::kStateUp);
	_putInLot.setStateSilently(Button::kStateUp);
	_getJob.setStateSilently(Button::kStateUp);
	if (m->_id == kMechanicIdCarDev)
		_getJob.setStateSilently(Button::kStateDisabled);
	if (!m->_car) {
		_showCondition.setStateSilently(Button::kStateDisabled);
		_auctionCar.setStateSilently(Button::kStateDisabled);
		_catalog.setStateSilently(Button::kStateDisabled);
		_putInLot.setStateSilently(Button::kStateDisabled);
	} else if (m->_job) {
		_catalog.setStateSilently(Button::kStateUp);
		_auctionCar.setStateSilently(Button::kStateDisabled);
		_putInLot.setStateSilently(Button::kStateDisabled);
		_getJob.setStateSilently(Button::kStateDisabled);
	}
}

void CarActionsPane::onShowConditionPress(Button *b) {
	// CarActions_showConditionPress (0x40fddc)
	Mechanic *m = g_engine->_mechanic;
	CarInstance *car = m->_car;
	if (!car)
		return;
	for (uint i = 0; i < car->_parts.size(); i++) {
		const Part *p = car->_parts[i];
		const int mi = p->def->meshIndex;
		if (!m->_job || m->_job->legalModes[CLIP(p->def->region, 0, 3)])
			car->setPartMaterial(p, conditionHighlight(p->condition));
		else if (mi >= 0 && mi < (int)car->_meshVisible.size())
			car->_meshVisible[mi] = false;
	}
	const Common::Array<Mesh3D *> &meshes = car->model()->_meshes;
	for (uint i = 0; i < meshes.size(); i++) {
		if (meshes[i] && meshes[i]->isShell())
			car->_meshVisible[i] = false;
		if (i < car->_specVisible.size())
			car->_specVisible[i] = false;
	}
	WorkShopScreen *ws = g_engine->_workShop;
	ws->_carView.autoFrame();
	ws->_carView.setDirty(true);
}

void CarActionsPane::onShowConditionRelease(Button *b) {
	// CarActions_showConditionRelease (0x410000)
	CarInstance *car = g_engine->_mechanic->_car;
	if (!car)
		return;
	WorkShopScreen *ws = g_engine->_workShop;
	for (uint i = 0; i < car->_parts.size(); i++)
		car->applyPartMaterials(car->_parts[i]);
	car->showRegion(ws->_region, true);
	ws->_carView.autoFrame();
	ws->_carView.setDirty(true);
}

void CarActionsPane::onPutInLot(Button *b) {
	// CarActions_onPutCarInLot (0x41014d): the car stays in the lot list
	Mechanic *m = g_engine->_mechanic;
	if (!m->_car)
		return;
	g_engine->_workShop->forgetCar();
	m->_car = nullptr;
	g_engine->_carLot->activate();
}

void CarActionsPane::onGetJob(Button *b) {
	// CarActions_onGetJob (0x41017d)
	if (!g_engine->_mechanic->_job)
		g_engine->_jobEngine->offerJob();
}

void CarActionsPane::onAuctionCar(Button *b) {
	// CarActions_onAuctionCar (0x41009b)
	Mechanic *m = g_engine->_mechanic;
	if (!m->_car)
		return;
	if (g_engine->_msgBox->ask("Auction Car", "You are about to auction your car. Once it's on the block, the only way to get it back "
	                           "is to bid on it. Do you want to continue?", kDlgButtonOk, kDlgButtonCancel))
		g_engine->_auction->activate(AuctionScreen::kModeSell);
}

void CarActionsPane::onCatalog(Button *b) {
	// CarActions_onCatalog (0x41008b)
	g_engine->_catalog->activate();
}

// ---------------------------------------------------------------- NavPane

NavPane::NavPane() {
	setRect(0, 0, 640, 480);
	_auction.setOnClick(makeCallback(this, &NavPane::onAuction));
	_carLot.setOnClick(makeCallback(this, &NavPane::onCarLot));
	_junkYard.setOnClick(makeCallback(this, &NavPane::onJunkYard));
	_exit.setOnClick(makeCallback(this, &NavPane::onExit));
	addChild(&_auction);
	addChild(&_carLot);
	addChild(&_junkYard);
	addChild(&_exit);
}

void NavPane::init() {
	initFromWorkshopSheets(_carLot, Common::Rect(522, 328, 640, 358));
	initFromWorkshopSheets(_junkYard, Common::Rect(522, 372, 640, 402));
	initFromWorkshopSheets(_auction, Common::Rect(522, 416, 640, 446));
	initFromWorkshopSheets(_exit, Common::Rect(591, 456, 640, 477));
}

void NavPane::updateStates() {
	Mechanic *m = g_engine->_mechanic;
	_carLot.setStateSilently(Button::kStateUp);
	_junkYard.setStateSilently(Button::kStateUp);
	_auction.setStateSilently(Button::kStateUp);
	if (!m->_car)
		_junkYard.setStateSilently(Button::kStateDisabled);
	if (m->_job) {
		_carLot.setStateSilently(Button::kStateDisabled);
		_auction.setStateSilently(Button::kStateDisabled);
	}
}

void NavPane::onAuction(Button *b) {
	// Nav_onAuction (0x410811)
	Mechanic *m = g_engine->_mechanic;
	if (m->_job)
		return;
	if (m->numCars() >= kMaxLotCars) {
		m->msgCarLotFull();
		return;
	}
	g_engine->_auction->activate(AuctionScreen::kModeBuy);
}

void NavPane::onCarLot(Button *b) {
	if (!g_engine->_mechanic->_job)
		g_engine->_carLot->activate();
}

void NavPane::onJunkYard(Button *b) {
	g_engine->_junkYard->activate();
}

void NavPane::onExit(Button *b) {
	// Nav_onExit (0x410894)
	bool save = true;
	if (g_engine->_mechanic->_job) {
		if (!g_engine->_msgBox->ask("Job Active", "If you leave now, you'll have to restart this job when you return!",
		                            kDlgButtonOk, kDlgButtonCancel))
			return;
		save = false;
		g_engine->_jobEngine->endJob(false, true);
	}
	g_engine->_signIn->activate(save);
}

// ---------------------------------------------------------------- EnginePane

EnginePane::EnginePane() : _savedRoot(nullptr), _startTime(0), _lastUpdate(0), _holdMs(500), _active(false) {
	setRect(0, 0, 640, 480);
}

void EnginePane::initChildren() {
	WorkShopScreen *ws = g_engine->_workShop;
	addChild(&ws->_backdrop);
	addChild(&ws->_carView);
	addChild(&ws->_toolPalette);
	addChild(&ws->_tabs);
	addChild(&ws->_actions._showCondition);
}

void EnginePane::start() {
	CarInstance *car = g_engine->_mechanic->_car;
	if (!car || car->isFalseStarting() || car->isRunning())
		return;
	Input *in = g_engine->_input;
	_savedRoot = in->root();
	in->setRoot(this);
	_active = true;
	car->tryStart();
	if (!car->isFalseStarting() && !car->isRunning())
		end();
	_startTime = g_engine->gameTimeMs();
}

void EnginePane::onButtonRelease() {
	if (_active && g_engine->gameTimeMs() <= _startTime + _holdMs)
		end();
}

void EnginePane::end() {
	if (!_active)
		return;
	_active = false;
	Input *in = g_engine->_input;
	in->setRoot(_savedRoot);
	in->releaseCapture();
	if (g_engine->_mechanic->_car)
		g_engine->_mechanic->_car->stopEngine();
}

void EnginePane::onRefresh() {
	WorkShopScreen *ws = g_engine->_workShop;
	const uint32 now = g_engine->gameTimeMs();
	if (_lastUpdate + 10 < now) {
		_lastUpdate = now;
		CarInstance *car = g_engine->_mechanic->_car;
		if (!car || !car->updateEngine())
			end();
		ws->_carView.setDirty(true);
	}
	ws->_backdrop.onRefresh();
}

// ---------------------------------------------------------------- ToolPalette

ToolPalette::ToolPalette() : _background(nullptr) {
	setRect(385, 81, 508, 361);
	_airRatchet.setRadio(this, 1, makeCallback(this, &ToolPalette::onAirRatchet));
	_bodyPaint.setRadio(this, 1, makeCallback(this, &ToolPalette::onBodyPaint));
	_camera.setRadio(this, 1, makeCallback(this, &ToolPalette::onCameraTool));
	_modelInPhoto.setOnPress(makeCallback(this, &ToolPalette::onModelInPhoto));
	_startEngine.setOnPress(makeCallback(this, &ToolPalette::onEngineDown));
	_startEngine.setOnRelease(makeCallback(this, &ToolPalette::onEngineUp));
}

ToolPalette::~ToolPalette() {
	delete _background;
}

void ToolPalette::init() {
	Resources *res = g_engine->_res;
	initFromWorkshopSheets(_airRatchet, Common::Rect(401, 97, 490, 161));
	initFromWorkshopSheets(_bodyPaint, Common::Rect(401, 165, 490, 229));
	initFromWorkshopSheets(_startEngine, Common::Rect(401, 290, 490, 354));
	_camera.setImages(res->getImage("Overlays\\CameraUP.tga"), res->getImage("Overlays\\CameraDN.tga"),
	                  res->getImage("Overlays\\CameraUP.tga"));
	_camera.setPos(401, 165);
	initFromWorkshopSheets(_modelInPhoto, Common::Rect(400, 236, 496, 273));
	const Graphics::ManagedSurface *ws = res->getImage("WorkShop.tga");
	_background = new Graphics::ManagedSurface(_rect.width(), _rect.height(), ws->format);
	_background->simpleBlitFrom(*ws, _rect, Common::Point(0, 0));
	_startEngine.setPressSound(nullptr);
	Sound *cond2 = g_engine->_snd->get("misc\\condition2.wav");
	_airRatchet.setPressSound(cond2);
	_camera.setPressSound(cond2);
	_bodyPaint.setPressSound(cond2);
	_engine.initChildren();
}

static bool hasModelMesh(const CarModel *model) {
	for (uint i = 0; i < model->_meshes.size(); i++)
		if (model->_meshes[i] && model->_meshes[i]->name.equalsIgnoreCase("model"))
			return true;
	return false;
}

void ToolPalette::onRefresh() {
	WorkShopScreen *ws = g_engine->_workShop;
	Mechanic *m = g_engine->_mechanic;
	g_engine->_gfx->blit(kPageBoth, *_background, _rect.left, _rect.top);
	const bool sprayCan = ws->tool() == &ws->_sprayCan;
	const bool camera = ws->tool() == &ws->_cameraTool;
	clearChildren();
	_airRatchet.setStateSilently(Button::kStateUp);
	if (!m->_car)
		return;
	if (!m->_job) {
		switch (ws->_region) {
		case 0:
			addChild(&_airRatchet);
			addChild(&_camera);
			if (hasModelMesh(m->_car->model())) {
				addChild(&_modelInPhoto);
				_modelInPhoto.setStateSilently(ws->_modelInPhoto ? Button::kStateDown : Button::kStateUp);
			}
			addChild(&_startEngine);
			if (!camera) {
				ws->setTool(&ws->_airRatchet);
				_airRatchet.setStateSilently(Button::kStateDown);
				_camera.setStateSilently(Button::kStateUp);
				_modelInPhoto.setStateSilently(Button::kStateDisabled);
			}
			break;
		case 1:
			addChild(&_airRatchet);
			addChild(&_startEngine);
			ws->setTool(&ws->_airRatchet);
			_airRatchet.setStateSilently(Button::kStateDown);
			break;
		case 2:
			addChild(&_bodyPaint);
			addChild(&_airRatchet);
			if (!sprayCan) {
				ws->setTool(&ws->_airRatchet);
				_airRatchet.setStateSilently(Button::kStateDown);
				_bodyPaint.setStateSilently(Button::kStateUp);
			}
			break;
		case 3:
			addChild(&_airRatchet);
			ws->setTool(&ws->_airRatchet);
			_airRatchet.setStateSilently(Button::kStateDown);
			break;
		default:
			break;
		}
	} else {
		addChild(&_airRatchet);
		ws->setTool(&ws->_airRatchet);
		_airRatchet.setStateSilently(Button::kStateDown);
		if (ws->_region == 0 || ws->_region == 1)
			addChild(&_startEngine);
	}
}

void ToolPalette::onAirRatchet(Button *b) {
	WorkShopScreen *ws = g_engine->_workShop;
	if (!g_engine->_mechanic->_car)
		return;
	ws->setTool(&ws->_airRatchet);
	if (ws->_region == 0)
		_modelInPhoto.setState(Button::kStateDisabled);
}

void ToolPalette::onBodyPaint(Button *b) {
	WorkShopScreen *ws = g_engine->_workShop;
	notImplemented("Body Paint");
	ws->setTool(&ws->_sprayCan);
}

void ToolPalette::onCameraTool(Button *b) {
	WorkShopScreen *ws = g_engine->_workShop;
	notImplemented("The camera");
	ws->setTool(&ws->_cameraTool);
}

void ToolPalette::onModelInPhoto(Button *b) {
	WorkShopScreen *ws = g_engine->_workShop;
	ws->_modelInPhoto = !ws->_modelInPhoto;
}

void ToolPalette::onEngineDown(Button *b) {
	if (g_engine->_mechanic->_car)
		_engine.start();
}

void ToolPalette::onEngineUp(Button *b) {
	if (g_engine->_mechanic->_car)
		_engine.onButtonRelease();
}

// ---------------------------------------------------------------- WorkShopScreen

WorkShopScreen::WorkShopScreen() : _region(0), _modelInPhoto(false), _background(nullptr), _tool(nullptr),
	_lastJobTick(0), _toolCursor(false) {
	setRect(0, 0, 640, 480);
	_partInfo.setRect(Common::Rect(85, 345, 385, 369));
	// WorkShop_ctor (0x41907f): child order
	addChild(&_bin);
	addChild(&_carView);
	addChild(&_tabs);
	addChild(&_actions);
	addChild(&_nav);
	addChild(&_toolPalette);
}

WorkShopScreen::~WorkShopScreen() {
}

void WorkShopScreen::init() {
	_background = g_engine->_res->getImage("WorkShop.tga");
	_bin.init();
	_tabs.init();
	_actions.init();
	_nav.init();
	_toolPalette.init();
	_airRatchet.init();
}

void WorkShopScreen::setTool(Tool *tool) {
	_tool = tool;
	if (_toolCursor)
		g_engine->_gfx->setCursor(_tool && _tool->_cursor ? _tool->_cursor : g_engine->_pointer, 0, 0);
}

void WorkShopScreen::showToolCursor(bool show) {
	if (show == _toolCursor)
		return;
	_toolCursor = show;
	g_engine->_gfx->setCursor(show && _tool && _tool->_cursor ? _tool->_cursor : g_engine->_pointer, 0, 0);
}

void WorkShopScreen::forgetCar() {
	_airRatchet.clearHover();
	_carView.setCar(nullptr);
	_carView.clearFocus();
	_bin.clear();
}

void WorkShopScreen::drawPartInfo(const Common::String &text) {
	_partInfo.draw(g_engine->_fonts[kFontStandard], text, kAlignStart, kAlignStart, kKeyDefault, kPageFront);
}

void WorkShopScreen::clearPartInfo() {
	g_engine->_gfx->restoreFromBack(_partInfo.rect());
}

void WorkShopScreen::activate() {
	debugC(1, kDebugGame, "Setting WorkShop Active..");
	g_engine->_jukebox->stopTrack(); // the workshop has no music
	Mechanic *m = g_engine->_mechanic;
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageBoth, *_background, 0, 0);
	m->drawName();
	m->drawCash();
	g_engine->_input->activateScreen(this);
	showToolCursor(false);
	_carView.setCar(m->_car);
	if (!m->_car)
		_bin.clear();
	setMode(_region, false);
	_actions.updateStates();
	_nav.updateStates();
	refresh();
	debugC(1, kDebugGame, "..WorkShop Set Active.");
}

bool WorkShopScreen::setMode(int region, bool redrawTools) {
	Mechanic *m = g_engine->_mechanic;
	if (m->_job && !m->_job->legalModes[CLIP(region, 0, 3)])
		return false;
	_region = region;
	if (!m->_car)
		return false;
	m->updateCurrentCar(region);
	_bin.refill();
	_carView.autoFrame();
	_tabs.updateStates();
	_tabs.refresh();
	if (region >= 1 && region <= 3 && !_tabs._sounds[region]->isPlaying())
		_tabs._sounds[region]->play();
	if (redrawTools)
		_toolPalette.refresh();
	return true;
}

void WorkShopScreen::render() {
	const Common::Point &mouse = g_engine->_input->mouse();
	if (_carView.isDirty() || _bin._list.listDirty()) {
		_carView.setDirty(false);
		if (_bin._list.listDirty()) {
			_bin._list.setListDirty(false);
			_bin.render();
		}
		_carView.draw();
	}
	// The pointer left the view: one more hover and a button release so the tool unhighlights
	if (_carView._mouseOver && !_carView.contains(mouse)) {
		_carView.setDirty(true);
		_carView.onHover(mouse);
		_carView.onLUp(mouse);
		_carView._mouseOver = false;
		showToolCursor(false);
	}
	if (_bin._list._hover && !_bin._list.contains(mouse)) {
		_bin._list._hover = false;
		_bin._list.setListDirty(true);
	}
}

void WorkShopScreen::onKeyDown(int key) {
	_carView.onArrowKey(key, g_engine->frameDelta());
	_carView.setDirty(true);
}

void WorkShopScreen::onRefresh() {
	Mechanic *m = g_engine->_mechanic;
	m->update();
	render();
	const uint32 now = g_engine->gameTimeMs();
	if (m->_id != kMechanicIdCarDev && now > 2000 && now - _lastJobTick > 1000) {
		_lastJobTick = now;
		g_engine->_jobEngine->update();
	}
}

// ---------------------------------------------------------------- test helper

void WorkShopScreen::debugDump(const Common::String &what) {
	CarInstance *car = g_engine->_mechanic ? g_engine->_mechanic->_car : nullptr;
	if (what == "bin") {
		for (uint i = 0; i < _bin._list.children().size(); i++) {
			const PartButton *b = (const PartButton *)_bin._list.children()[i];
			if (b->_part)
				debug("dump bin %d: %s cond %d at %d %d", i, b->_part->def->name.c_str(), b->_part->condition,
				      (b->rect().left + b->rect().right) / 2, (b->rect().top + b->rect().bottom) / 2);
		}
		return;
	}
	if (!car)
		return;
	if (what == "bolts") {
		_airRatchet.debugDumpBolts();
		return;
	}
	for (uint i = 0; i < car->_parts.size(); i++) {
		const Part *p = car->_parts[i];
		Common::Point sp;
		if (debugPartScreenPos(p->def->id, sp))
			debug("dump part %d '%s' region %d cond %d at %d %d", p->def->id, p->def->name.c_str(), p->def->region,
			      p->condition, sp.x, sp.y);
		else if (p->def->meshIndex >= 0 && car->_meshVisible[p->def->meshIndex])
			debug("dump part %d '%s' region %d cond %d not clickable", p->def->id, p->def->name.c_str(), p->def->region, p->condition);
	}
}

bool WorkShopScreen::debugPartScreenPos(int id, Common::Point &out) {
	CarInstance *car = g_engine->_mechanic ? g_engine->_mechanic->_car : nullptr;
	const Part *p = car ? car->findPart(id) : nullptr;
	if (!p)
		return false;
	const Common::Array<Mesh3D *> &meshes = car->model()->_meshes;
	const int mi = p->def->meshIndex;
	if (mi < 0 || mi >= (int)meshes.size() || !meshes[mi] || !car->_meshVisible[mi])
		return false;
	const Mesh3D *m = meshes[mi];
	for (uint k = 0; k < m->polys.size(); k++) {
		const MeshPoly &poly = m->polys[k];
		const Vec3 c = (m->local.transformPoint(m->vertices[poly.v[0]].pos) + m->local.transformPoint(m->vertices[poly.v[1]].pos) +
		                m->local.transformPoint(m->vertices[poly.v[2]].pos)) * (1.0f / 3.0f);
		Common::Point sp;
		if (!_carView.project(c, sp) || !_carView.contains(sp))
			continue;
		Vec3 o, d;
		_carView.pickRay(sp, o, d);
		if (_carView.pick(o, d, false).mesh == mi) {
			out = sp;
			return true;
		}
	}
	return false;
}

} // End of namespace Gearhead
