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

#ifndef GEARHEAD_SIGNIN_H
#define GEARHEAD_SIGNIN_H

#include "gearhead/input.h"
#include "gearhead/mechanic.h"
#include "gearhead/ui.h"

namespace Gearhead {

class SignInScreen;
class Sound;

/** One roster line (ProfileRow, vt 0x4d0c18) */
class ProfileRow : public Pane {
public:
	ProfileRow();
	~ProfileRow() override;

	void init(SignInScreen *owner, int slot, int y);
	void setProfile(const MechSummary &summary);
	void clearProfile();
	bool hasProfile() const { return _summary != nullptr; }
	const MechSummary *summary() const { return _summary; }
	int slot() const { return _slot; }

	bool _highlighted;
	Common::Rect _clickRect;

	void onLUp(const Common::Point &p) override;
	void onHover(const Common::Point &p) override;
	void onRefresh() override;

private:
	void onNew(Button *b);
	void onDelete(Button *b);

	SignInScreen *_owner;
	int _slot;
	int _y;
	Graphics::ManagedSurface *_strip;
	Graphics::ManagedSurface *_stripHighlighted;
	MechSummary *_summary;
	TextBox _columns[5];
	ImageButton _delete;
	ImageButton _new;
};

/** Name entry (TextInput 0x42b07f): the text always ends with the cursor character '-' */
class TextInput : public KeyHook {
public:
	TextInput() { reset(); }
	void reset();
	void onKeyHook(int scanCode, bool released, int ascii) override;
	const Common::String &text() const { return _text; }

	Common::Functor0<void> *_onOk;
	Common::Functor0<void> *_onCancel;
	Common::Functor0<void> *_onChange;

private:
	Common::String _text;
	bool _keyDown[256];
};

/** "Create New Mechanic" (NewMechDlg, vt 0x4d0b40) */
class NewMechDialog : public Dialog {
public:
	NewMechDialog(SignInScreen *owner);
	~NewMechDialog() override;

	void init();
	void open() override;
	void close() override;

private:
	void onOk(Button *b);
	void onCancel(Button *b);
	void ok();
	void cancel();
	void drawField();

	SignInScreen *_owner;
	ImageButton _ok, _cancel;
	TextInput _input;
	TextBox _field;
};

/** The sign-in screen (SignIn, vt 0x4d0b88, signin.cpp) */
class SignInScreen : public Pane {
public:
	SignInScreen();
	~SignInScreen() override;

	void init();
	/** SignIn_activate (0x42b8c1) */
	void activate(bool saveFirst);
	void refreshRows();
	void repaint();

	void signIn(ProfileRow *row);
	void createMechanic(const Common::String &name);
	void askDelete(ProfileRow *row);

	void onRefresh() override;
	void onKeyDown(int key) override {}

	ProfileRow *_currentRow;
	Graphics::ManagedSurface *_background;
	NewMechDialog *_newMechDialog;
	PreFabDialog *_prefab;

private:
	void onExit(Button *b);
	void onCredits(Button *b);

	ImageButton _exit, _credits;
	ProfileRow _rows[kNumMechanicSlots];
	Sound *_zipper;
	bool _active;
};

} // End of namespace Gearhead

#endif // GEARHEAD_SIGNIN_H
