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

#ifndef GEARHEAD_AUCTION_H
#define GEARHEAD_AUCTION_H

#include "gearhead/scene3ds.h"
#include "gearhead/ui.h"

namespace Gearhead {

class CarInstance;
class MsvcRandom;
class Sound;

/** Auction_roundDown5 (0x420ca3): down to a multiple of 5 */
int auctionRoundDown5(float x);

/** One AI bidder (Bidder, 0xac bytes; six are created once per program run) */
struct Bidder {
	float aggressiveness = 0.5f; ///< +0xa0, 0.5 + frand(0.5), fixed
	float interest = 0.0f;       ///< +0xa4
	float eagerness = 0.0f;      ///< +0xa8, frand(1) per car
	int minPrice = 0;            ///< +0x98
	int maxPrice = 0;            ///< +0x9c
	bool leader = false;         ///< +0x91: made the last AI bid

	/** Bidder_startCar (0x421c22) */
	void startCar(int lo, int hi, MsvcRandom &rnd);
	/** Bidder_updateInterest (0x421e40) */
	void updateInterest(int asking);
	/** Bidder_wantsToBid (0x421f8c) */
	bool wantsToBid(MsvcRandom &rnd) const;
};

/** Price, clock and AI of one car on the block (Auctioneer, Auction+0x430, 0xc4 bytes) */
class Auctioneer {
public:
	Auctioneer();
	/** Auctioneer_ctor (0x420970): the six bidders */
	void createBidders(MsvcRandom &rnd);
	/** Auctioneer_loadAssets (0x420b4f) */
	void init();

	/** Auctioneer_startCar (0x420f54): valuation, bidders, opening price */
	void startCar(const CarInstance *car);
	/** Auctioneer_update (0x42167c) */
	void update();
	/** Auctioneer_playerBid (0x421315) */
	void playerBid();
	/** Auctioneer_refreshDisplay (0x420d06) */
	void refreshDisplay();

	bool playerLeads() const { return _yourBid == _currentBid; }

	int _currentBid;  ///< +0x14
	int _askingPrice; ///< +0x18
	int _yourBid;     ///< +0x1c
	float _timeLeft;  ///< +0xa8
	int _partsValue;  ///< +0xb4
	int _lo;          ///< +0xb8 estimate
	int _hi;          ///< +0xbc maximum
	bool _sold;       ///< +0xc0 the hammer fell
	Common::Array<Bidder> _bidders;

private:
	/** Auctioneer_computeAsking (0x4212bb) */
	void computeAsking();
	/** Auctioneer_aiBid (0x421370) */
	void aiBid();
	/** Auctioneer_updateTimer (0x421397) */
	void updateTimer();
	/** Auctioneer_shuffleBidSounds (0x4214fc) */
	void shuffleCalls();
	/** Auctioneer_playNextBidSound (0x421624) */
	void playNextCall();

	TextBox _yourBox, _currentBox, _askingBox;
	Common::Point _timerPos;
	Graphics::ManagedSurface *_timer;
	int _callIndex;
	Common::Array<Sound *> _calls, _shuffled;
	uint32 _startTime;
	int _duration;
	// Globals in the original (0x4ed9f8, 0x4ed9fc, 0x4eda00): never reset between cars or visits
	int _timerLastFrame;
	int _lastPriceDrop;
	int _lastBidderTick;
};

/** The 3D view of the auction block (BidPane, Auction+0x14c, vt 0x4d0944, rect (12,95)-(512,285)) */
class BidPane : public Widget {
public:
	BidPane();
	/** BidPane_loadAssets (0x4229fc) */
	void init();
	/** BidPane_setCar (0x422e51): the car stands on the floor, off screen */
	void setCar(CarInstance *car);
	/** BidPane_detachCar (0x423084) */
	void detachCar() { _car = nullptr; }
	/** BidPane_animateFromPlusX (0x4230b5): drive in from +X, then away towards -X */
	void animateFromPlusX();
	/** BidPane_animateFromMinusX (0x4231de): the mirror image (unused) */
	void animateFromMinusX();
	/** BidPane_render (0x423307) */
	void render();

	bool moving() const { return _moving; }
	CarInstance *car() const { return _car; }
	float carX() const { return _carPos.x; }

private:
	void updateObjects();

	Scene3DS _scene;
	Mat4 _camera;
	float _zoom;
	Vec3 _floor;            ///< +0x4c
	Vec3 _boxMin, _boxMax;  ///< +0x60 / +0x6c, the BoundBox mesh
	Texture3D _skyTexture;
	Material3D _skyMaterial;
	CarInstance *_car;      ///< +0x48
	Vec3 _carPos;           ///< the model root position
	float _startX, _endX;   ///< +0x7c / +0x80
	bool _moving;           ///< +0x84
	bool _resetPosition;    ///< +0x85
	Common::Array<SceneObject> _objects;
	Common::Array<Common::Array<const Material3D *> > _materials;
};

/** "Sold!" (SoldDialog, vt 0x4d084c) and "Winning Bid!" (WinBidDialog, vt 0x4d08a8) */
class AuctionResultDialog : public Dialog {
public:
	AuctionResultDialog(const char *title);
	/** SoldDialog_loadAssets (0x4203e5) / WinBidDialog_loadAssets (0x420726) */
	void init();
	/** vslot 15: set the message and open */
	void show(const Common::String &message);

private:
	/** SoldDialog_onOk (0x420671) / WinBidDialog_onOk (0x4207af): back to the workshop */
	void onOk(Button *b);

	ImageButton _ok;
};

/** The auction (Auction, 0x4f4 bytes, vt 0x4d07f4, auction.cpp): buy random cars, sell your own */
class AuctionScreen : public Pane {
public:
	enum Mode {
		kModeBuy = 0,
		kModeSell = 1  ///< the player's current car is on the block
	};
	enum State {
		kStateMoving = 0,
		kStateMovingMirrored = 1, ///< never set
		kStateBidding = 2
	};

	AuctionScreen();
	~AuctionScreen() override;
	/** Auction_loadAssets (0x41f0c4) */
	void init();
	/** Auction_setActive (0x41f22e) with the mode field (+0x41c) */
	void activate(int mode);
	/** Auction_skipCar (0x41f436) */
	void skipCar();
	void stopChatter();

	/** Auction_update (vslot 14, 0x41f583) */
	void onRefresh() override;

	/** Test helper: the state of the block */
	void debugDump() const;

	Auctioneer _auctioneer;
	BidPane _bidPane;
	int _mode;  ///< +0x41c
	int _state; ///< +0x420

private:
	/** Auction_generateCar (0x41f466) */
	void generateCar();
	/** Auction_updateCarMotion (0x41f7e1) */
	void updateCarMotion();
	/** The original never frees an auction car that was not bought (or the one it sold) */
	void releaseCars();
	void onBack(Button *b);
	void onBid(Button *b);
	void onSkip(Button *b);

	Graphics::ManagedSurface *_background;
	ImageButton _back, _bid, _skip;
	AuctionResultDialog _soldDialog, _winDialog;
	CarInstance *_car;     ///< +0x424 the generated car (owned until bought)
	CarInstance *_soldCar; ///< the player's car after the hammer (owned)
	bool _nextCarPending;  ///< +0x428
	Sound *_chatter;       ///< +0x42c
};

} // End of namespace Gearhead

#endif // GEARHEAD_AUCTION_H
