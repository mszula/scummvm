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

#ifndef GEARHEAD_PAINT_H
#define GEARHEAD_PAINT_H

#include "gearhead/decals.h"
#include "gearhead/math3d.h"
#include "gearhead/workshop.h"

namespace Gearhead {

/** A brush of the paint pane (BrushButton, 0x68 bytes): 22x22 picture, the 24x24 brush mask */
class BrushButton : public ImageButton {
public:
	BrushButton();
	~BrushButton() override;
	/** BrushButton_init (0x415734) */
	void init(const Common::Rect &r, const Graphics::ManagedSurface &brush, int radius);

	Common::Array<byte> _mask; ///< +0x5c: 1 where the brush paints
	int _maskW, _maskH;
	int _radius;               ///< +0x60
	bool _fill;                ///< +0x64 "Part": the whole part

private:
	Graphics::ManagedSurface *_img[3];
};

/** A paint colour swatch (ColorButton, 0x68 bytes) */
class ColorButton : public ImageButton {
public:
	ColorButton();
	~ColorButton() override;
	/** ColorButton_init (0x4159e6) */
	void init(const Common::Rect &r, const Vec3 &colour);

	Vec3 _colour; ///< +0x5c

private:
	Graphics::ManagedSurface *_img[3];
};

/** The decal being edited and stamped (DecalEdit, 0x18 bytes, DecalBrowser+0x604) */
class DecalEdit {
public:
	DecalEdit();
	~DecalEdit();

	/** DecalEdit_setDecal (0x42dab3) */
	void setDecal(OwnedDecal *od);
	/** DecalEdit_flip (0x42dcef): true = mirror left/right, false = upside down */
	void flip(bool mirror);
	/** DecalEdit_rotate (0x42dfcd): true = 90 degrees clockwise */
	void rotate(bool clockwise);
	/** DecalEdit_recolor (0x42e2e0) */
	void recolor(Vec3 &colour);
	/** DecalEdit_buildCursor (0x42daf8): the stamp at the chosen size and its see-through ghost */
	void buildCursor();

	OwnedDecal *_owned;  ///< +0x00
	int _w, _h;          ///< work image (+0x04), RGB
	Common::Array<byte> _work;
	int _sw, _sh;        ///< scaled image (+0x0c), RGB
	Common::Array<byte> _scaled;
	Graphics::ManagedSurface *_ghost; ///< +0x08, screen format, 3 of 4 pixels blank
	int _orient;         ///< +0x10
	float _scale;        ///< +0x14

private:
	/** DecalEdit_resetImage (0x42d9b7) */
	void resetImage();
	/** DecalEdit_reapplyOrient (0x42e586) */
	void reapplyOrient();
};

/**
 * The Spray Can (SprayCan, vt 0x4d0458, WorkShop+0x18f0): sprays the selected
 * brush and colour onto the car's paint texture, or stamps a decal. Screen
 * points reach texels through a 12x12 grid of the projected paint triangles.
 */
class SprayCan : public Tool {
public:
	SprayCan();
	~SprayCan() override;
	/** SprayCan_init (0x412428) */
	void init();

	/** SprayCan_onHover (0x41254a): shaking the can rattles it */
	void onHover(const Common::Point &p) override;
	/** SprayCan_onLDown (0x412632): stamp the decal */
	void onLeftDown(const Common::Point &p) override;
	/** SprayCan_onLUp (0x412617) */
	void onLeftUp(const Common::Point &p) override;
	/** SprayCan_onLHeld (0x4129a9): spray */
	void onLeftHeld(const Common::Point &p) override;

	/** SprayCan_setDecal (0x4124dd) */
	void setDecal(DecalEdit *edit);
	/** SprayCan_clearDecal (0x41251b) */
	void clearDecal();
	/** Test helper: the triangle grid */
	void debugDump();
	/** Test helper: the centre of the n-th largest paint triangle on screen */
	bool debugPaintPos(int n, Common::Point &out);
	/** The pointer over the view carries the decal's ghost, cut to the view (Pointer_draw) */
	bool ghostCursor(const Common::Point &mouse, Graphics::ManagedSurface *&img, Common::Point &hotspot);

	const BrushButton *_brush; ///< +0xac2c mask (radius +0xac30, fill +0xac34 are copied)
	int _radius;
	bool _fill;
	Vec3 _colour;              ///< +0xac35
	DecalEdit *_decal;         ///< +0xac64
	bool _blocked;             ///< +0xac60

private:
	struct Tri {
		float x[3], y[3]; ///< screen
		float d[3];       ///< distance of each vertex from the camera
		float area2;
		int mesh, poly;
	};
	struct Hit {
		float b[3];
		float depth;
		const Tri *tri;
	};

	/** TriGrid_build (0x41191c) */
	void buildGrid();
	void updateGrid();
	/** Projector_project (0x450e38) */
	void project(const Vec3 &p, float &sx, float &sy) const;
	/** SprayCan_paintAt (0x41308d) */
	void paintAt(float x, float y, int radius, const Vec3 &colour);
	void paintDone();

	enum {
		kGridSize = 12,
		kMaxTris = 1000
	};
	Common::Array<Tri> _tris;
	Common::Array<const Tri *> _cells[kGridSize][kGridSize];
	float _cellW, _cellH, _originX, _originY;
	Vec3 _lastAngles, _lastTarget;
	float _lastDist;
	bool _gridValid;
	Sound *_spray, *_bead1, *_bead2;
	bool _shakeDown;           ///< g_SprayShakeDown
	Graphics::ManagedSurface *_ghostPart;
};

/** Brushes, colours and the Decals button (PaintPane, vt 0x4d04e4, rect (387,227)-(506,362)) */
class PaintPane : public Pane {
public:
	PaintPane();
	~PaintPane() override;
	/** PaintPane_init (0x414f55) */
	void init();
	/** PaintPane_enable (0x4155eb) */
	void enable();
	/** PaintPane_disable (0x41553f) */
	void disable();
	/** PaintPane_onLHeld (0x415521): holding the button over the pane drops the decal */
	void onLHeld(const Common::Point &p) override;

	ImageButton _decals;
	Common::Array<BrushButton *> _brushes;
	Common::Array<ColorButton *> _colours;
	BrushButton *_selectedBrush;  ///< +0xf0
	ColorButton *_selectedColour; ///< +0xf4

private:
	void onDecals(Button *b);
	void onBrush(Button *b);
	void onColor(Button *b);
};

/** "Choose a Decal:" / "Modify Decal:" (DecalBrowser, vt 0x4d0cdc, WorkShop+0x12a8, (120,90)-(520,390)) */
class DecalBrowser : public Dialog {
public:
	DecalBrowser();
	~DecalBrowser() override;
	/** DecalBrowser_init (0x42ec82) */
	void init();
	/** DecalBrowser_open (0x42f8ce) */
	void openBrowser();

	DecalEdit _edit;
	DecalList _list;

private:
	/** DecalBrowser_showChooser (0x42f592) */
	void showChooser();
	/** DecalBrowser_showModify (0x42f5f1) */
	void showModify();
	/** DecalBrowser_drawPreview (0x42f7a2) */
	void drawPreview();

	void onDone(Button *b);
	void onCancel(Button *b);
	void onUp(Button *b);
	void onDown(Button *b);
	void onMirror(Button *b);
	void onFlip(Button *b);
	void onTurnRight(Button *b);
	void onTurnLeft(Button *b);
	void onBigger(Button *b);
	void onNormal(Button *b);
	void onSmaller(Button *b);
	void onLastUsed(Button *b);
	void onBack(Button *b);
	void onCell(Button *b);
	void onSwatch(Button *b);

	ImageButton _done, _cancel, _up, _down, _mirror, _flip, _turnRight, _turnLeft, _bigger, _normal, _smaller, _lastUsed, _back;
	Common::Array<ColorButton *> _swatches;
	Graphics::ManagedSurface *_page2;
	Common::Rect _preview;
};

} // End of namespace Gearhead

#endif // GEARHEAD_PAINT_H
