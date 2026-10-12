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
#include "common/textconsole.h"
#include "common/util.h"
#include "graphics/managed_surface.h"

#include "gearhead/auction.h"
#include "gearhead/car.h"
#include "gearhead/font.h"
#include "gearhead/gearhead.h"
#include "gearhead/gfx.h"
#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/random.h"
#include "gearhead/renderer3d.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/** MSVC __ftol: truncation through a 64-bit integer, of which the low half is kept */
static int ftol(double v) {
	if (!(v > -9.2e18 && v < 9.2e18))
		return 0; // the "integer indefinite" 0x8000000000000000 has a zero low half
	return (int)(uint32)(uint64)(int64)v;
}

static const int kAuctionRegions[3] = { 1, 2, 3 }; // g_AuctionRegionIds 0x4e1dc8

int auctionRoundDown5(float x) {
	const double t = (double)x * 0.1;
	const int i = ftol(t);
	return i * 10 + (t - (float)i < 0.5 ? 0 : 5);
}

// ---------------------------------------------------------------- Bidder

void Bidder::startCar(int lo, int hi, MsvcRandom &rnd) {
	minPrice = ftol((double)lo * 0.75 + (double)((float)lo * aggressiveness) * 0.1);
	maxPrice = ftol((double)hi * 0.75 + (double)((float)hi * aggressiveness) * 0.25);
	leader = false;
	eagerness = rnd.frand(1.0f);
}

void Bidder::updateInterest(int asking) {
	if (asking <= minPrice)
		interest = 1.0f;
	else
		interest = (float)(1.0 - (double)(float)(asking - minPrice) / (double)(float)(maxPrice - minPrice));
	if (!(interest >= 0.0f)) // NaN too
		interest = 0.0f;
	if (interest > 1.0f)
		interest = 1.0f;
}

bool Bidder::wantsToBid(MsvcRandom &rnd) const {
	if (leader)
		return false;
	return rnd.frand(interest + eagerness) > 1.0f;
}

// ---------------------------------------------------------------- Auctioneer

Auctioneer::Auctioneer() : _currentBid(0), _askingPrice(0), _yourBid(0), _timeLeft(0.0f), _partsValue(0), _lo(0), _hi(0),
	_sold(false), _timerPos(266, 294), _timer(nullptr), _callIndex(0), _startTime(0), _duration(20),
	_timerLastFrame(0), _lastPriceDrop(0), _lastBidderTick(0) {
	_currentBox.setRect(Common::Rect(519, 125, 623, 153));
	_askingBox.setRect(Common::Rect(519, 205, 623, 233));
	_yourBox.setRect(Common::Rect(519, 283, 623, 311));
}

void Auctioneer::createBidders(MsvcRandom &rnd) {
	_bidders.resize(6);
	for (uint i = 0; i < _bidders.size(); i++)
		_bidders[i].aggressiveness = (float)(rnd.frand(0.5f) + 0.5);
}

void Auctioneer::init() {
	_timer = g_engine->_res->getImage("Overlays\\Timer.tga");
	static const char *const kCalls[] = { "1", "4", "6", "7", "8", "9", "a", "d", "e" };
	for (uint i = 0; i < ARRAYSIZE(kCalls); i++)
		_calls.push_back(g_engine->_snd->get(Common::String::format("auction\\bid%s.wav", kCalls[i])));
}

void Auctioneer::startCar(const CarInstance *car) {
	MsvcRandom &rnd = g_engine->_rnd;
	// The parts' value and their average condition
	_partsValue = 0;
	float condSum = 0.0f;
	for (uint i = 0; i < car->_parts.size(); i++) {
		_partsValue = ftol((double)car->_parts[i]->value() + (double)(float)_partsValue);
		condSum = condSum + (float)car->_parts[i]->condition;
	}
	// Quirk: the average divides by the part count minus one
	const int divisor = g_engine->fixOriginalBugs() ? (int)car->_parts.size() : (int)car->_parts.size() - 1;
	float avg = condSum / (float)divisor;
	avg = avg / 3.0f;
	_lo = _partsValue;
	_lo = ftol((1.02 - (double)rnd.frand(0.04f)) * (double)avg * (double)_lo);
	// A complete engine, body or running gear adds 10 % of its stock parts' value
	for (int r = 0; r < 3; r++) {
		if (!car->isComplete(kAuctionRegions[r]))
			continue;
		for (uint i = 0; i < car->_parts.size(); i++) {
			const Part *p = car->_parts[i];
			if (p->def->region == kAuctionRegions[r] && !p->def->custom)
				_lo = ftol((double)p->value() * 0.1 + (double)_lo);
		}
	}
	// The maximum: a fifth of the way from the estimate to the sum of every part's top price
	const CarModel *model = car->model();
	_hi = 0;
	for (uint i = 0; i < model->_parts.size(); i++)
		_hi = ftol((double)(float)_hi + (double)model->_parts[i]->costMax);
	_hi = ftol((double)(float)(_hi - _lo) * 0.2 + (double)_lo);
	if (_hi <= _lo)
		_hi = _lo + 100;

	_currentBid = 100;
	_yourBid = 0;
	_sold = false;
	_startTime = g_engine->gameTimeMs();
	for (uint i = 0; i < _bidders.size(); i++)
		_bidders[i].startCar(_lo, _hi, rnd);
	computeAsking();
	refreshDisplay();
	_callIndex = 0;
	shuffleCalls();
}

void Auctioneer::computeAsking() {
	_askingPrice = ftol((double)_hi / 18.0);
	_askingPrice += _currentBid;
	_askingPrice = auctionRoundDown5((float)_askingPrice);
}

void Auctioneer::playerBid() {
	Mechanic *m = g_engine->_mechanic;
	if (!m->canAfford(_askingPrice)) {
		m->msgNoCash();
		return;
	}
	_currentBid = _askingPrice;
	_yourBid = _currentBid;
	computeAsking();
	refreshDisplay();
}

void Auctioneer::aiBid() {
	_currentBid = _askingPrice;
	computeAsking();
	refreshDisplay();
}

void Auctioneer::refreshDisplay() {
	for (uint i = 0; i < _bidders.size(); i++)
		_bidders[i].updateInterest(_askingPrice);
	const BlitFont &cash = g_engine->_fonts[kFontCash];
	_currentBox.draw(cash, "$ " + Common::String::format("%i", auctionRoundDown5((float)_currentBid)), kAlignEnd, kAlignCenter,
	                 kKeyDefault, kPageFront);
	_askingBox.draw(cash, "$ " + Common::String::format("%i", auctionRoundDown5((float)_askingPrice)), kAlignEnd, kAlignCenter,
	                kKeyDefault, kPageFront);
	_yourBox.draw(cash, "$ " + Common::String::format("%i", auctionRoundDown5((float)_yourBid)), kAlignEnd, kAlignCenter,
	              kKeyDefault, kPageFront);
	if (playerLeads())
		g_engine->_fonts[kFontStandard].drawText(*g_engine->_gfx, ">>", _currentBox.rect(), kAlignStart, kAlignCenter, kKeyColor,
		                                         kPageFront);
}

void Auctioneer::updateTimer() {
	const uint32 now = g_engine->gameTimeMs();
	_timeLeft = (float)_duration - (float)(int)(now - _startTime) / 1000.0f;
	if (_timeLeft <= 0.0f)
		_sold = true;
	// Timer.tga: 16 frames of 23x23, blitted only when the frame changes
	const int x = ftol(15.0 - (double)(_timeLeft / (float)_duration) * 15.0) * 23;
	if (x != _timerLastFrame && _timer)
		g_engine->_gfx->blitRect(kPageBoth, *_timer, Common::Rect(x, 0, x + 23, 23), _timerPos.x, _timerPos.y);
	_timerLastFrame = x;
}

void Auctioneer::shuffleCalls() {
	MsvcRandom &rnd = g_engine->_rnd;
	_shuffled.clear();
	Common::Array<Sound *> left = _calls;
	for (uint i = 0; i < _calls.size(); i++) {
		const int k = ftol(rnd.frand((float)left.size()));
		if (k >= (int)left.size()) {
			// frand returned its maximum: the original reads the zeroed slot past the end
			_shuffled.push_back(nullptr);
			continue;
		}
		_shuffled.push_back(left[k]);
		left.remove_at(k);
	}
}

void Auctioneer::playNextCall() {
	if (_shuffled.empty())
		return;
	_callIndex = (_callIndex + 1) % (int)_shuffled.size();
	if (_shuffled[_callIndex])
		_shuffled[_callIndex]->play();
}

void Auctioneer::update() {
	MsvcRandom &rnd = g_engine->_rnd;
	const int now = (int)g_engine->gameTimeMs();
	updateTimer();
	// Every 2 s (absolute, also right after a bid) the asking price drops by 25, not below the bid + 25
	if ((double)_duration * 0.1 * 1000.0 + (double)_lastPriceDrop <= (double)now) {
		_lastPriceDrop = now;
		_askingPrice -= 25;
		if (_askingPrice <= _currentBid + 25)
			_askingPrice = _currentBid + 25;
		refreshDisplay();
	}
	// Every 750 ms: of the bidders that want to bid, the least aggressive one bids
	if (_lastBidderTick + 750 <= now) {
		_lastBidderTick = now;
		float minAggr = 99.0f;
		bool any = false;
		for (uint i = 0; i < _bidders.size(); i++) {
			if (_bidders[i].wantsToBid(rnd)) {
				_bidders[i].leader = true;
				minAggr = MIN(minAggr, _bidders[i].aggressiveness);
				any = true;
			}
		}
		if (any) {
			for (uint i = 0; i < _bidders.size(); i++) {
				if (!_bidders[i].leader)
					continue;
				if (fabs((double)(_bidders[i].aggressiveness - minAggr)) >= 0.0001) {
					_bidders[i].leader = false;
				} else {
					playNextCall();
					aiBid();
					refreshDisplay();
				}
			}
		}
	}
}

// ---------------------------------------------------------------- BidPane

BidPane::BidPane() : _zoom(1.0f), _car(nullptr), _startX(0.0f), _endX(0.0f), _moving(false), _resetPosition(false) {
	setRect(12, 95, 512, 285);
}

void BidPane::init() {
	Resources *res = g_engine->_res;
	Graphics::ManagedSurface *sky = res->loadImage("Data\\Scenes\\Clouds.jpg", nullptr);
	if (sky) {
		textureFromSurface(*sky, _skyTexture);
		delete sky;
	} else {
		_skyTexture.create(1);
	}
	_skyTexture.name = "Clouds.jpg";
	_skyMaterial.texture = &_skyTexture;
	_skyMaterial.shadeColor = 0;

	if (!_scene.load(*res, "Data\\Scenes\\", "Auction.3ds"))
		error("error loading Auction mesh");
	// Camera #0 aimed at its target node every update, far 5000
	const Scene3DS::Camera &cam = _scene._camera;
	_zoom = cam.valid ? cam.zoom : 1.0f;
	const SceneNode *cn = _scene.findNode(cam.name);
	const SceneNode *tn = _scene.findNode(cam.name + ".target");
	const Vec3 eye = cn ? cn->pos : cam.pos;
	const Vec3 target = tn ? tn->pos : cam.target;
	Vec3 fwd = (target - eye).normalized();
	if (fwd.lengthSq() == 0.0f)
		fwd = Vec3(1, 0, 0);
	const Vec3 rgt = Vec3(0, 1, 0).cross(fwd).normalized();
	const Vec3 up = fwd.cross(rgt).normalized();
	_camera = Mat4::fromAxes(rgt, up, fwd, eye);

	const SceneNode *floor = _scene.findNode("Floor");
	_floor = floor ? floor->pos : Vec3();
	// The BoundBox mesh only gives the extent of the block (its object-space bounds); it is hidden
	_boxMin = _boxMax = Vec3();
	SceneMesh *box = _scene.findMesh("BoundBox");
	if (box) {
		box->visible = false;
		_boxMin = box->objMin;
		_boxMax = box->objMax;
	}
	debugC(1, kDebugGame, "Auction block: floor (%.1f %.1f %.1f), box x %.1f..%.1f, zoom %.3f, %d light(s)", _floor.x, _floor.y,
	       _floor.z, _boxMin.x, _boxMax.x, _zoom, _scene._lights.size());
	for (uint i = 0; i < _scene._lights.size(); i++)
		debugC(1, kDebugGame, "  light %s at (%.1f %.1f %.1f) range %.0f intensity %.2f", _scene._lights[i].name.c_str(),
		       _scene._lights[i].pos.x, _scene._lights[i].pos.y, _scene._lights[i].pos.z, _scene._lights[i].range,
		       _scene._lights[i].intensity);
}

void BidPane::setCar(CarInstance *car) {
	if (!car)
		return;
	_car = car;
	car->showRegion(0, false);
	const CarModel *model = car->model();
	// The lowest point of every installed part's mesh goes on the floor
	float minY = 1e13f;
	for (uint i = 0; i < car->_parts.size(); i++) {
		const int mi = car->partMesh(car->_parts[i]->def);
		if (mi < 0 || mi >= (int)model->_meshes.size() || !model->_meshes[mi])
			continue;
		const Mesh3D *m = model->_meshes[mi];
		for (uint v = 0; v < m->vertices.size(); v++)
			minY = MIN(minY, m->local.transformPoint(m->vertices[v].pos).y);
	}
	_carPos = _floor;
	_carPos.y -= minY;
	_carPos.x = -999999.0f;
	// Bounds of the visible meshes (Scene_computeBounds, model space)
	Vec3 bmin(1e6f, 1e6f, 1e6f), bmax(-1e6f, -1e6f, -1e6f);
	for (uint i = 0; i < model->_meshes.size(); i++) {
		const Mesh3D *m = model->_meshes[i];
		if (!m || i >= car->_meshVisible.size() || !car->_meshVisible[i])
			continue;
		for (uint v = 0; v < m->vertices.size(); v++) {
			const Vec3 w = m->local.transformPoint(m->vertices[v].pos);
			bmin = Vec3(MIN(bmin.x, w.x), MIN(bmin.y, w.y), MIN(bmin.z, w.z));
			bmax = Vec3(MAX(bmax.x, w.x), MAX(bmax.y, w.y), MAX(bmax.z, w.z));
		}
	}
	// Quirk: the half length uses min.z instead of min.x
	const float half = (bmax.x - (g_engine->fixOriginalBugs() ? bmin.x : bmin.z)) * 0.5f;
	_startX = half + _boxMax.x + 400.0f;
	_endX = (_boxMin.x - half) - 400.0f;
	_resetPosition = true;
	_materials.clear();
	for (uint i = 0; i < model->_meshes.size(); i++)
		if (model->_meshes[i] && i < car->_meshVisible.size() && car->_meshVisible[i])
			_materials.push_back(car->_polyMaterials[i]);
}

void BidPane::animateFromPlusX() {
	if (!_car)
		return;
	float &x = _carPos.x;
	if (_resetPosition) {
		_resetPosition = false;
		x = _startX;
	}
	if (_floor.x + 10.0f < x) {
		// Arriving: 15 % of the way per frame (not time based)
		x = (float)((double)(_floor.x - x) * 0.15 + x);
		_moving = fabs((double)(x - _floor.x)) >= 10.0;
	} else {
		// At the block or leaving towards -X, accelerating
		x = x - 10.0f;
		x = (float)((double)(x - _floor.x) * 0.15 + x);
		_moving = _endX <= x;
	}
	setDirty(true);
}

void BidPane::animateFromMinusX() {
	if (!_car)
		return;
	float &x = _carPos.x;
	if (_resetPosition) {
		_resetPosition = false;
		x = _endX;
	}
	if (x < _floor.x - 10.0f) {
		x = (float)((double)(_floor.x - x) * 0.15 + x);
		_moving = fabs((double)(x - _floor.x)) >= 10.0;
	} else {
		x = x + 10.0f;
		x = (float)((double)(x - _floor.x) * 0.15 + x);
		_moving = x <= _startX;
	}
	setDirty(true);
}

void BidPane::updateObjects() {
	_objects.clear();
	if (!_car)
		return;
	const CarModel *model = _car->model();
	Mat4 root;
	root.setRow(3, _carPos, 1.0f);
	uint k = 0;
	for (uint i = 0; i < model->_meshes.size(); i++) {
		const Mesh3D *m = model->_meshes[i];
		if (!m || i >= _car->_meshVisible.size() || !_car->_meshVisible[i])
			continue;
		if (k >= _materials.size())
			break;
		SceneObject o;
		o.mesh = m;
		o.world = m->local * root;
		o.materials = &_materials[k++];
		_objects.push_back(o);
	}
}

void BidPane::render() {
	Renderer3D *r3d = g_engine->_renderer3d;
	Gfx *gfx = g_engine->_gfx;
	if (!r3d)
		return;
	r3d->beginFrame(_rect, Vec3(0.2f, 0.2f, 0.4f));
	r3d->setCamera(_camera, _zoom, 1.0f, 5000.0f);
	// Sky sprite (12,95)-(512,275), mirrored, depth 10000
	r3d->drawScreenQuad(Common::Rect(_rect.left, _rect.top, _rect.right, _rect.bottom - 10), 0.99f, 0.0f, 0.0f, 0.99f,
	                    &_skyMaterial, 10000.0f);
	// Scene_render replaces the ambient (0.25) set just before with the scene's own (0.1)
	r3d->setAmbient(Vec3(0.1f, 0.1f, 0.1f));
	r3d->clearLights();
	for (uint i = 0; i < _scene._lights.size(); i++) {
		const SceneLight &sl = _scene._lights[i];
		Light3D l;
		l.type = Light3D::kPoint;
		l.position = sl.pos;
		l.colour = sl.colour;
		l.k1 = sl.intensity;
		l.k2 = sl.intensity / sl.range;
		r3d->addLight(l);
	}
	const Mat4 identity;
	for (uint i = 0; i < _scene._meshes.size(); i++) {
		const SceneMesh *m = _scene._meshes[i];
		if (m->visible)
			r3d->drawMesh(m->mesh, identity, m->materials);
	}
	updateObjects();
	for (uint i = 0; i < _objects.size(); i++)
		r3d->drawMesh(*_objects[i].mesh, _objects[i].world, *_objects[i].materials);
	r3d->endFrame(*gfx);
}

// ---------------------------------------------------------------- result dialogs

AuctionResultDialog::AuctionResultDialog(const char *title) {
	_title = title;
	_ok.setOnClick(makeCallback(this, &AuctionResultDialog::onOk));
}

void AuctionResultDialog::init() {
	Resources *res = g_engine->_res;
	setFonts(&g_engine->_fonts[kFontDialog], &g_engine->_fonts[kFontDialogTitle]);
	setFrame("Overlays\\Dialog\\Frame.tga");
	setPos(200, 135);
	_ok.setImages(res->getImage("Overlays\\Dialog\\OkUP.tga"), res->getImage("Overlays\\Dialog\\OkDN.tga"),
	              res->getImage("Overlays\\Dialog\\OkUP.tga"));
	_ok.setPos(_anchorCenter.x, _anchorCenter.y);
	Common::Array<Widget *> own, extra;
	own.push_back(&_ok);
	setButtons(own, extra);
}

void AuctionResultDialog::show(const Common::String &message) {
	_message = message;
	_ok.setStateSilently(Button::kStateUp);
	open();
}

void AuctionResultDialog::onOk(Button *b) {
	// The dialog is never closed: the workshop takes over the input and repaints the screen
	_isOpen = false;
	g_engine->_auction->stopChatter();
	g_engine->_workShop->activate();
}

// ---------------------------------------------------------------- AuctionScreen

AuctionScreen::AuctionScreen() : _mode(kModeBuy), _state(kStateBidding), _background(nullptr), _soldDialog("Sold!"),
	_winDialog("Winning Bid!"), _car(nullptr), _soldCar(nullptr), _nextCarPending(false), _chatter(nullptr) {
	setRect(0, 0, 640, 480);
	_back.setOnClick(makeCallback(this, &AuctionScreen::onBack));
	_bid.setOnClick(makeCallback(this, &AuctionScreen::onBid));
	_skip.setOnClick(makeCallback(this, &AuctionScreen::onSkip));
	addChild(&_back);
	addChild(&_bidPane);
	addChild(&_bid);
	addChild(&_skip);
}

AuctionScreen::~AuctionScreen() {
	releaseCars();
}

void AuctionScreen::init() {
	Resources *res = g_engine->_res;
	_auctioneer.createBidders(g_engine->_rnd);
	_background = res->getImage("Auction.tga");
	// Button frames: up AuctionUP, pressed AuctionDN, disabled = the plain background (invisible)
	const Graphics::ManagedSurface &up = *res->getImage("AuctionUP.tga");
	const Graphics::ManagedSurface &dn = *res->getImage("AuctionDN.tga");
	const Graphics::ManagedSurface &off = *_background;
	_back.setFromSheets(Common::Rect(332, 48, 499, 81), up, dn, off);
	_bid.setFromSheets(Common::Rect(516, 330, 633, 382), up, dn, off);
	_skip.setFromSheets(Common::Rect(516, 403, 633, 434), up, dn, off);
	_bidPane.init();
	_auctioneer.init();
	_soldDialog.init();
	_winDialog.init();
	_chatter = g_engine->_snd->get("misc\\chatter.wav");
	_chatter->setLoop(true);
}

void AuctionScreen::releaseCars() {
	if (_soldCar) {
		if (_bidPane.car() == _soldCar)
			_bidPane.detachCar();
		delete _soldCar;
		_soldCar = nullptr;
	}
	if (_car) {
		if (_bidPane.car() == _car)
			_bidPane.detachCar();
		delete _car;
		_car = nullptr;
	}
}

void AuctionScreen::activate(int mode) {
	debugC(1, kDebugGame, "Setting Auction Active..");
	Mechanic *m = g_engine->_mechanic;
	releaseCars();
	m->saveIfReal();
	_chatter->play();
	Gfx *gfx = g_engine->_gfx;
	gfx->blit(kPageBoth, *_background, 0, 0);
	m->drawName();
	m->drawCash();
	g_engine->_input->activateScreen(this);
	g_engine->_workShop->showToolCursor(false);
	_mode = mode;
	if (_mode == kModeSell && m->_car) {
		// The clock already runs while the car drives in
		_state = kStateMoving;
		_nextCarPending = false;
		_bidPane.setCar(m->_car);
		_auctioneer.startCar(m->_car);
		_back.setStateSilently(Button::kStateDisabled);
		_skip.setStateSilently(Button::kStateDisabled);
	} else {
		_mode = kModeBuy;
		_state = kStateMoving;
		_nextCarPending = false;
		generateCar();
		_bidPane.setCar(_car);
		_back.setStateSilently(Button::kStateUp);
		_skip.setStateSilently(Button::kStateUp);
	}
	refresh();
	_bidPane.render();
	debugC(1, kDebugGame, "..Auction Set Active.");
}

void AuctionScreen::generateCar() {
	Mechanic *m = g_engine->_mechanic;
	MsvcRandom &rnd = g_engine->_rnd;
	const Common::Array<CarModel *> &models = g_engine->_cars.models();
	if (models.empty())
		error("Auction: no car models");
	// A random model the mechanic's skill allows (any model for the dev mechanics), at most 1000 tries
	const int skill = skillLevel(m->_skillIndex).index;
	int id = 0;
	for (int tries = 0; tries < 1000; tries++) {
		id = models[ftol(rnd.frand((float)models.size() - 0.001f))]->_id; // AutoDataList_randomId
		if (m->isDev())
			break;
		const CarModel *cd = g_engine->_cars.find(id, false);
		if (cd && skill >= cd->_minSkill)
			break;
	}
	CarModel *model = g_engine->_cars.find(id, true);
	if (!model)
		error("Auction: cannot load car %d", id);
	_car = new CarInstance(model, rnd);
	randomizeForSkill(_car, m->_skillIndex, rnd);
}

void AuctionScreen::skipCar() {
	if (_state == kStateBidding) {
		_state = kStateMoving;
		_nextCarPending = true;
	}
}

void AuctionScreen::updateCarMotion() {
	if (_state == kStateMoving)
		_bidPane.animateFromPlusX();
	else
		_bidPane.animateFromMinusX();
	if (_bidPane.moving())
		return;
	if (!_nextCarPending) {
		if (_mode == kModeBuy)
			_auctioneer.startCar(_car);
		_state = kStateBidding;
	} else {
		// The car has left: the next one
		if (_bidPane.car() == _car)
			_bidPane.detachCar();
		delete _car;
		_car = nullptr;
		generateCar();
		_bidPane.setCar(_car);
		_nextCarPending = false;
	}
}

void AuctionScreen::onRefresh() {
	Mechanic *m = g_engine->_mechanic;
	if (_state == kStateBidding) {
		_auctioneer.update();
		if (!_auctioneer._sold) {
			if (_auctioneer.playerLeads()) {
				_back.setState(Button::kStateDisabled);
				_skip.setState(Button::kStateDisabled);
			} else if (_mode != kModeSell) {
				if (_back.state() == Button::kStateDisabled)
					_back.setState(Button::kStateUp);
				if (_skip.state() == Button::kStateDisabled)
					_skip.setState(Button::kStateUp);
			}
		} else if (_mode == kModeSell) {
			if (_auctioneer.playerLeads()) {
				// Bought back: a 10 % fee, the car stays
				m->_cash -= (float)auctionRoundDown5((float)_auctioneer._currentBid * 0.1f);
				_winDialog.show("You've won the bid for this car!");
				return;
			}
			// The text uses the car's costs before it is gone
			CarInstance *car = m->_car;
			const float profit = (float)_auctioneer._currentBid - car->_repairCost - car->_purchaseCost;
			Common::String text = "Congratulations. Somebody just shelled out $" + Common::String::format("%i", _auctioneer._currentBid) +
			                      " for your car.";
			if (profit >= 0.0f) {
				text += " You made a profit of $" + Common::String::format("%4.2f", profit) + ".";
				if (profit > 5000.0f)
					text += " Man alive!";
				else if (profit > 1000.0f)
					text += " Wow!";
			} else {
				text += " You lost $" + Common::String::format("%4.2f", -profit) + ".";
				if (profit < -5000.0f)
					text += " Pick another hobby.";
				else if (profit < -1000.0f)
					text += " Ouch!";
			}
			_soldDialog.show(text);
			g_engine->_workShop->forgetCar();
			m->sellCurrentCar((float)auctionRoundDown5((float)_auctioneer._currentBid));
			_soldCar = car;
			return;
		} else {
			if (_auctioneer.playerLeads()) {
				m->buyCar(_car, (float)auctionRoundDown5((float)_auctioneer._currentBid));
				_car = nullptr; // the mechanic's now
				_winDialog.show("You've won the bid for this car!");
				return;
			}
			skipCar();
		}
	} else {
		updateCarMotion();
	}
	if (_bidPane.isDirty()) {
		_bidPane.setDirty(false);
		_bidPane.render();
	}
}

void AuctionScreen::stopChatter() {
	_chatter->stop();
}

void AuctionScreen::onBack(Button *b) {
	_bidPane.detachCar();
	_chatter->stop();
	g_engine->_workShop->activate();
}

void AuctionScreen::onBid(Button *b) {
	_auctioneer.playerBid();
}

void AuctionScreen::onSkip(Button *b) {
	skipCar();
}

void AuctionScreen::debugDump() const {
	const CarInstance *car = _bidPane.car();
	debug("dump auction mode %d state %d car %d parts %d x %.1f moving %d | bid %d asking %d yours %d | value %d lo %d hi %d | "
	      "left %.2f sold %d",
	      _mode, _state, car ? car->model()->_id : -1, car ? (int)car->_parts.size() : 0, _bidPane.carX(), _bidPane.moving(),
	      _auctioneer._currentBid, _auctioneer._askingPrice, _auctioneer._yourBid, _auctioneer._partsValue, _auctioneer._lo,
	      _auctioneer._hi, _auctioneer._timeLeft, _auctioneer._sold);
	for (uint i = 0; i < _auctioneer._bidders.size(); i++) {
		const Bidder &bd = _auctioneer._bidders[i];
		debug("dump bidder %d aggr %.3f min %d max %d interest %.3f eager %.3f leader %d", i, bd.aggressiveness, bd.minPrice,
		      bd.maxPrice, bd.interest, bd.eagerness, bd.leader);
	}
}

} // End of namespace Gearhead
