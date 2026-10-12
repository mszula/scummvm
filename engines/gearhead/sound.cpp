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

#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/debug.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "gearhead/detection.h"
#include "gearhead/resources.h"
#include "gearhead/sound.h"

namespace Gearhead {

Sound::Sound(Audio::Mixer::SoundType type) : _data(nullptr), _size(0), _loop(false), _type(type) {
}

Sound::~Sound() {
	stop();
	free(_data);
}

bool Sound::load(const byte *data, uint32 size) {
	free(_data);
	_data = (byte *)malloc(size);
	if (!_data)
		return false;
	memcpy(_data, data, size);
	_size = size;
	return true;
}

void Sound::play() {
	Audio::Mixer *mixer = g_system->getMixer();
	for (int i = 0; i < 2; i++) {
		if (mixer->isSoundHandleActive(_handles[i]))
			continue;
		Common::MemoryReadStream *s = new Common::MemoryReadStream(_data, _size, DisposeAfterUse::NO);
		Audio::SeekableAudioStream *wav = Audio::makeWAVStream(s, DisposeAfterUse::YES);
		if (!wav)
			return;
		Audio::AudioStream *stream = _loop ? Audio::makeLoopingAudioStream(wav, 0) : wav;
		mixer->playStream(_type, &_handles[i], stream);
		return;
	}
}

void Sound::stop() {
	for (int i = 0; i < 2; i++)
		g_system->getMixer()->stopHandle(_handles[i]);
}

bool Sound::isPlaying() const {
	return g_system->getMixer()->isSoundHandleActive(_handles[0]) ||
	       g_system->getMixer()->isSoundHandleActive(_handles[1]);
}

SoundManager::SoundManager(Resources &res) : _res(res) {
}

SoundManager::~SoundManager() {
	for (SoundMap::iterator it = _cache.begin(); it != _cache.end(); ++it)
		delete it->_value;
}

Sound *SoundManager::loadUncached(const Common::String &name, Audio::Mixer::SoundType type) {
	Common::SeekableReadStream *s = _res.openSound(name);
	if (!s)
		error("Couldn't load sound: %s", name.c_str());
	const uint32 size = s->size();
	byte *buf = (byte *)malloc(size);
	s->read(buf, size);
	delete s;
	Sound *snd = new Sound(type);
	snd->load(buf, size);
	free(buf);
	debugC(2, kDebugSound, "Loaded sound %s (%d bytes)", name.c_str(), size);
	return snd;
}

Sound *SoundManager::get(const Common::String &name) {
	SoundMap::iterator it = _cache.find(name);
	if (it != _cache.end())
		return it->_value;
	Sound *snd = loadUncached(name, Audio::Mixer::kSFXSoundType);
	_cache[name] = snd;
	return snd;
}

void SoundManager::play(const Common::String &name) {
	get(name)->play();
}

JukeBox::JukeBox(SoundManager &snd) : _snd(snd), _current(nullptr), _currentIndex(-1), _jingle(0) {
	_tracks.push_back("music\\loop1.wav");
	_tracks.push_back("music\\loop2.wav");
	_tracks.push_back("music\\loop4.wav");
}

JukeBox::~JukeBox() {
	stopTrack();
}

void JukeBox::playTrack(int index) {
	stopTrack();
	if (index < 0 || index >= (int)_tracks.size())
		return;
	_current = _snd.loadUncached(_tracks[index], Audio::Mixer::kMusicSoundType);
	_current->setLoop(true);
	_current->play();
	_currentIndex = index;
}

void JukeBox::stopTrack() {
	delete _current;
	_current = nullptr;
	_currentIndex = -1;
}

void JukeBox::playJingle() {
	static const char *const kJingles[2] = { "music\\victory2.wav", "music\\victory3.wav" };
	Sound *s = _snd.get(kJingles[_jingle]);
	s->stop();
	s->play();
	_jingle = (_jingle + 1) % 2;
}

} // End of namespace Gearhead
