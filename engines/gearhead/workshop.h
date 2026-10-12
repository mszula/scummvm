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

#ifndef GEARHEAD_WORKSHOP_H
#define GEARHEAD_WORKSHOP_H

#include "gearhead/cardata.h"
#include "gearhead/carview.h"
#include "gearhead/partsbin.h"
#include "gearhead/ui.h"

namespace Gearhead {

class DecalBrowser;
class PaintPane;
class Renderer3D;
class Sound;
class SprayCan;
struct Part;

/** Workshop tool (Tool, vt 0x4d04cc): the car view forwards its mouse events to the current tool */
class Tool {
public:
	Tool() : _cursor(nullptr) {}
	virtual ~Tool() {}
	virtual void onHover(const Common::Point &p) {}
	virtual void onLeftDown(const Common::Point &p) {}
	virtual void onLeftUp(const Common::Point &p) {}
	virtual void onLeftHeld(const Common::Point &p) {}
	virtual void onRightDown(const Common::Point &p) {}

	/** Tool_setCursor (0x418020) */
	void setCursor(const Common::String &image);
	Graphics::ManagedSurface *_cursor;
};

class AirRatchet;

/** "Air Ratchet Tool" (AirRatchetDialog, vt 0x4d0290): Frame.tga at (390,95) with a Cancel button */
class AirRatchetDialog : public Dialog {
public:
	AirRatchetDialog(AirRatchet *owner);
	/** AirRatchetDialog_init (0x40eeff) */
	void init();

	ImageButton _cancel;

private:
	void onCancel(Button *b);
	AirRatchet *_owner;
};

/**
 * The Air Ratchet (AirRatchet, vt 0x4d0740, WorkShop+0xc558): pick a part,
 * click every bolt to take it off, or drag a part from the bin onto the car
 * and click every bolt to put it on.
 */
class AirRatchet : public Tool {
public:
	enum Mode {
		kModeIdle = 0,
		kModeRemoving = 1,
		kModeAttaching = 2
	};

	AirRatchet();
	~AirRatchet() override;

	/** AirRatchet_init (0x41a9f1) */
	void init();

	void onHover(const Common::Point &p) override;
	void onLeftDown(const Common::Point &p) override;
	void onLeftUp(const Common::Point &p) override;

	/** AirRatchet_beginRemove (0x41b026) */
	void beginRemove(Part *part);
	/** AirRatchet_beginAttach (0x41b16b) */
	void beginAttach(Part *part);
	/** AirRatchetDialog_onCancel (0x40ef6d) */
	void cancel();
	/** AirRatchet_clearHover (0x41adaa): the car is going away */
	void clearHover() { _hovered = _prevHovered = nullptr; }

	/** Bolt cubes and arrows, drawn with the car */
	void render3D(Renderer3D &r3d) const;
	/** Nearest visible bolt cube hit by the ray (exact cube), -1 if none */
	int pickBoltCube(const Vec3 &origin, const Vec3 &dir, float &t) const;
	/** Test helper: the screen positions of the clickable bolts */
	void debugDumpBolts() const;
	bool debugBoltScreenPos(int i, Common::Point &out) const;

	AirRatchetDialog _dialog;
	int _mode;

private:
	struct Bolt {
		Vec3 pos;
		bool visible = false;
		float arrowSize = 0.0f;
		Material3D material;
	};

	Part *pickPart(const Common::Point &p) const;
	/** AirRatchet_pickBoltObject (0x41c041) */
	int pickBolt(const Common::Point &p) const;
	/** AirRatchet_findBoltUnderCursor (0x41b5ad) */
	int findBoltUnderCursor(const Common::Point &p) const;
	/** AirRatchet_setupBolts (0x41b77d) */
	void setupBolts(const Part *part);
	/** AirRatchet_hideBolts (0x41b968) */
	void hideBolts();
	/** AirRatchet_onHoverOverview (0x41c40e) */
	void onHoverOverview(const Common::Point &p);
	/** AirRatchet_afterPartChange (0x41afe8) */
	void afterPartChange();

	Part *_hovered;      ///< +0x15c
	Part *_prevHovered;  ///< +0x160
	int _remaining;      ///< +0x158
	Bolt _bolts[16];
	int _arrowCount;
	Mesh3D _cube;
	Texture3D _arrowTexture;
	Material3D _arrowMaterial;
	Sound *_tighten, *_loosen, *_attachStart, *_removeStart, *_miss;
};

/** A tool that does nothing yet (the camera) */
class PlaceholderTool : public Tool {
};

/** g_WorkShopBackdrop (0x4ed960): full-screen; its hover and tick keep the workshop drawn under modal panes */
class WorkShopBackdrop : public Widget {
public:
	WorkShopBackdrop() { setRect(0, 0, 640, 480); }
	void onHover(const Common::Point &p) override { onRefresh(); }
	void onRefresh() override;
};

/** COMPLETE / ENGINE / BODY / RUNNING GEAR (ModeTabs, vt 0x4d0410): a radio group */
class RegionTabs : public Pane {
public:
	RegionTabs();
	void init();
	/** ModeTabs_updateStates (0x41118d) */
	void updateStates();

	ImageButton _tabs[4];
	Sound *_sounds[4];

private:
	void onTab(Button *b);
};

/** Show Condition, Put Car In Lot, Get A Job, Auction Car, Catalog (CarActionsPane, vt 0x4d0338) */
class CarActionsPane : public Pane {
public:
	CarActionsPane();
	void init();
	/** CarActionsPane_updateStates (0x40fd00) */
	void updateStates();

	ImageButton _showCondition, _putInLot, _catalog, _auctionCar, _getJob;

private:
	void onShowConditionPress(Button *b);
	void onShowConditionRelease(Button *b);
	void onPutInLot(Button *b);
	void onGetJob(Button *b);
	void onAuctionCar(Button *b);
	void onCatalog(Button *b);
};

/** Go To Car Lot / JunkYard / Auction, Exit (NavPane, vt 0x4d0380) */
class NavPane : public Pane {
public:
	NavPane();
	void init();
	/** NavPane_updateStates (0x4107a1) */
	void updateStates();

	ImageButton _auction, _carLot, _junkYard, _exit;

private:
	void onAuction(Button *b);
	void onCarLot(Button *b);
	void onJunkYard(Button *b);
	void onExit(Button *b);
};

/** The running engine (EnginePane, WorkShop+0x1264): a modal root while the start sound plays */
class EnginePane : public Pane {
public:
	EnginePane();
	/** EnginePane_initChildren (0x410c0f) */
	void initChildren();
	/** EnginePane_start (0x410ced) */
	void start();
	/** EnginePane_onButtonRelease (0x410d85) */
	void onButtonRelease();
	/** EnginePane_end (0x410dae) */
	void end();
	/** EnginePane_onRefresh (0x410c84) */
	void onRefresh() override;

private:
	Widget *_savedRoot;
	uint32 _startTime;
	uint32 _lastUpdate;
	uint32 _holdMs;
	bool _active;
};

/** Air Ratchet / Body Paint / Camera / Model in Photo / Start Engine (ToolPalette, vt 0x4d0644) */
class ToolPalette : public Pane {
public:
	ToolPalette();
	~ToolPalette() override;
	void init();
	/** ToolPalette_refresh (0x4184c7): rebuilds the visible buttons on every refresh */
	void onRefresh() override;

	ImageButton _airRatchet, _bodyPaint, _camera, _modelInPhoto, _startEngine;
	EnginePane _engine;
	PaintPane *_paintPane; ///< +0x34 of the palette (WorkShop+0xf98), owned

private:
	void onAirRatchet(Button *b);
	void onBodyPaint(Button *b);
	void onCameraTool(Button *b);
	void onModelInPhoto(Button *b);
	void onEngineDown(Button *b);
	void onEngineUp(Button *b);

	Graphics::ManagedSurface *_background;
};

/**
 * The workshop (WorkShop, vt 0x4d068c, workshop.cpp): the car view, the
 * parts bin, the region tabs, the tools and the menus.
 */
class WorkShopScreen : public Pane {
public:
	WorkShopScreen();
	~WorkShopScreen() override;
	/** WorkShop_init (0x419248) */
	void init();
	/** WorkShop_setActive (0x4192f6) */
	void activate();
	/** WorkShop_setMode (0x411270): false if the job forbids the region (or there is no car) */
	bool setMode(int region, bool redrawTools);
	/** WorkShop_render (0x4196aa) */
	void render();
	/** WorkShop_setTool (0x4180ce) */
	void setTool(Tool *tool);
	Tool *tool() const { return _tool; }
	/** The cursor shows the tool while the pointer is over the view */
	void showToolCursor(bool show);
	/** The tool cursor for the pointer position (the Spray Can's decal ghost follows the pointer) */
	void updateToolCursor(const Common::Point &mouse);
	/** The current car is about to be destroyed */
	void forgetCar();
	/** g_PartInfoBox: name of the hovered part */
	void drawPartInfo(const Common::String &text);
	void clearPartInfo();
	/** Test helper (autoscript "dump parts|bolts|bin"): screen positions in the log */
	void debugDump(const Common::String &what);
	/** Test helper: a screen point whose pick ray hits the installed part @p id */
	bool debugPartScreenPos(int id, Common::Point &out);

	/** WorkShop_onKeyHeld (0x4198e6) */
	void onKeyDown(int key) override;
	/** WorkShop_onTick (0x4195fe), also run by the refresh event */
	void onRefresh() override;

	CarView _carView;
	PartsBin _bin;
	RegionTabs _tabs;
	CarActionsPane _actions;
	NavPane _nav;
	ToolPalette _toolPalette;
	WorkShopBackdrop _backdrop;
	AirRatchet _airRatchet;
	SprayCan *_sprayCan;          ///< +0x18f0 (owned)
	PlaceholderTool _cameraTool;
	DecalBrowser *_decalBrowser;  ///< +0x12a8 (owned)
	int _region;          ///< +0x18dc: 0 COMPLETE, 1 ENGINE, 2 BODY, 3 RUNNING GEAR
	bool _modelInPhoto;   ///< +0xc72c

private:
	Graphics::ManagedSurface *_background;
	Tool *_tool;
	TextBox _partInfo;
	uint32 _lastJobTick;  ///< g_WorkShopLastJobTick
	bool _toolCursor;
};

} // End of namespace Gearhead

#endif // GEARHEAD_WORKSHOP_H
