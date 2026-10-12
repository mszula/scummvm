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

#ifndef GEARHEAD_SCREENS_H
#define GEARHEAD_SCREENS_H

#include "gearhead/ui.h"

namespace Gearhead {

/** Name and cash shown on every game screen (Mechanic HUD, main_video §5.2) */
void drawMechanicHud();

/** Credits (0x4504a7): full-screen image, a back button, Esc returns to the sign-in screen */
class CreditsScreen : public Pane {
public:
	CreditsScreen();
	void init();
	void show();
	void onKeyDown(int key) override;

private:
	void onBack(Button *b);
	void back();

	Graphics::ManagedSurface *_background;
	ImageButton _back;
};

} // End of namespace Gearhead

#endif // GEARHEAD_SCREENS_H
