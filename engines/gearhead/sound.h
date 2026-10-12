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

#ifndef GEARHEAD_SOUND_H
#define GEARHEAD_SOUND_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

namespace Gearhead {

class Resources;

/**
 * VSound: a WAV kept in memory. Like the two duplicated DirectSound buffers
 * of the original, at most two instances play at once; play() on a sound
 * whose both instances are busy does nothing. No volume or pan control.
 */
class Sound {
public:
	Sound(Audio::Mixer::SoundType type = Audio::Mixer::kSFXSoundType);
	~Sound();

	bool load(const byte *data, uint32 size);
	void play();
	void stop();
	bool isPlaying() const;
	void setLoop(bool loop) { _loop = loop; }

private:
	byte *_data;
	uint32 _size;
	bool _loop;
	Audio::Mixer::SoundType _type;
	Audio::SoundHandle _handles[2];
};

class SoundManager {
public:
	SoundManager(Resources &res);
	~SoundManager();

	/** Resourcer_getSound: cached, relative to "Data\Sound16\". Missing sounds are fatal. */
	Sound *get(const Common::String &name);
	/** Uncached (the caller owns it), used for music */
	Sound *loadUncached(const Common::String &name, Audio::Mixer::SoundType type);
	void play(const Common::String &name);

private:
	Resources &_res;
	typedef Common::HashMap<Common::String, Sound *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> SoundMap;
	SoundMap _cache;
};

/** Music: three looped tracks and two alternating victory jingles (JukeBox 0x45534c) */
class JukeBox {
public:
	JukeBox(SoundManager &snd);
	~JukeBox();

	void playTrack(int index);
	void stopTrack();
	void playJingle();

private:
	SoundManager &_snd;
	Common::StringArray _tracks;
	Sound *_current;
	int _currentIndex;
	int _jingle;
};

} // End of namespace Gearhead

#endif // GEARHEAD_SOUND_H
