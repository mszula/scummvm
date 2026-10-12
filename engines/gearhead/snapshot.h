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

#ifndef GEARHEAD_SNAPSHOT_H
#define GEARHEAD_SNAPSHOT_H

#include "gearhead/workshop.h"

namespace Gearhead {

/**
 * The camera (CameraTool, vt 0x4d06e0, WorkShop+0xc700): a click in the car
 * view flashes, shows the car full screen with the Mekada logo while the
 * button is held and saves the picture (SnapShots\Shots_<name>\Shot<N>).
 */
class CameraTool : public Tool {
public:
	CameraTool();
	~CameraTool() override;
	/** CameraTool_init (0x408cb0) */
	void init();
	/** CameraTool_onLDown (0x408fad) */
	void onLeftDown(const Common::Point &p) override;

	/** Test helper: the names of the files written by the last picture */
	Common::StringArray _lastFiles;

private:
	/** Snapshot_flashWhite (0x409379) */
	void flashWhite();
	/** The car full screen with the logo, then the frame is presented (and returned) */
	Graphics::ManagedSurface *takePicture(const Vec3 &clearColour);
	/** Snapshot_save (0x40945a) */
	void save(Graphics::ManagedSurface *img, bool printable);

	Sound *_sound;                  ///< +0x28 misc\camera.wav
	Graphics::ManagedSurface *_logo; ///< the logo sprite (5,5)-(69,111)
};

} // End of namespace Gearhead

#endif // GEARHEAD_SNAPSHOT_H
