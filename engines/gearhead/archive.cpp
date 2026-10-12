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

#include "common/compression/deflate.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "gearhead/archive.h"
#include "gearhead/detection.h"

namespace Gearhead {

static const char *const kKeyString =
	"98ru91nb98fH98ufkfQkRAnf09i09kmvVnSjzAc9iu28rir09akdnBCn289ua9f09aRFnf029ur09akQRtan"
	"mzlkAvjb983ur9jaopVAkfmlzrWknlRzxc90i3r09uqtARankfnQdf";

static const char kMagic[8] = { 'c', 'L', 'i', 'b', '!', '3', '1', '7' };

CLibArchive::CLibArchive() : _stream(nullptr) {
	// Key stream generator (7 rounds over a 1024-byte table)
	const byte *key = (const byte *)kKeyString;
	const uint len = strlen(kKeyString);
	for (uint i = 0; i < 1024; i++)
		_keyStream[i] = i & 0xFF;
	uint k = 0;
	for (uint round = 0; round < 7; round++) {
		for (uint i = 0; i < 1024; i++) {
			_keyStream[i] = (byte)(_keyStream[i] + round + 0x11);
			_keyStream[i] = (byte)(key[k] * _keyStream[i]);
			const uint j = (i + 0x13D) & 0x3FF;
			const byte back = key[len - 1 - k];
			k++;
			_keyStream[j] = (byte)(_keyStream[j] + back + _keyStream[i]);
			if (k >= len)
				k = 0;
		}
	}
}

CLibArchive::~CLibArchive() {
	delete _stream;
}

void CLibArchive::xorKeyStream(byte *buf, uint32 size) const {
	for (uint32 i = 0; i < size; i++)
		buf[i] ^= _keyStream[i & 0x3FF];
}

bool CLibArchive::open(Common::SeekableReadStream *stream, const Common::String &name) {
	delete _stream;
	_stream = stream;
	_name = name;
	_entries.clear();

	if (!_stream)
		return false;

	const uint32 fileSize = _stream->size();

	// Skip the text banner (NUL terminated, within the first 1024 bytes)
	uint32 pos = 0;
	while (pos < 1024 && pos < fileSize && _stream->readByte() != 0)
		pos++;
	pos++;
	if (pos >= 1024 || pos + 16 > fileSize)
		return false;

	byte magic[16];
	_stream->read(magic, 16);
	for (int i = 0; i < 8; i++) {
		if ((magic[i] ^ magic[i + 8]) != (byte)kMagic[i]) {
			warning("CLibArchive: %s is not a cLib archive", name.c_str());
			return false;
		}
	}
	pos += 16;

	byte raw[kEntrySize];
	while (pos + kEntrySize <= fileSize) {
		_stream->seek(pos);
		if (_stream->read(raw, kEntrySize) != kEntrySize)
			break;
		xorKeyStream(raw, kEntrySize);

		uint32 d[7];
		for (int i = 0; i < 7; i++)
			d[i] = READ_LE_UINT32(raw + i * 4) ^ READ_LE_UINT32(raw + 0x120 + i * 4);

		uint nameLen = 0;
		while (nameLen < 0x104 && raw[0x1C + nameLen])
			nameLen++;
		Common::String member((const char *)raw + 0x1C, nameLen);
		if (!member.equals("the lstream header")) {
			Entry e;
			e.flags = d[0];
			e.offset = d[1];
			e.storedSize = d[2];
			e.size = d[3];
			_entries[Common::Path(member, '/')] = e;
			debugC(3, kDebugResources, "CLibArchive %s: %s flags=%d size=%d", name.c_str(), member.c_str(), e.flags, e.size);
		}

		const uint32 next = d[4];
		if (next <= pos)
			break;
		pos = next;
	}

	debugC(1, kDebugResources, "CLibArchive: opened %s with %d members", name.c_str(), _entries.size());
	return true;
}

bool CLibArchive::hasFile(const Common::Path &path) const {
	return _entries.contains(path);
}

int CLibArchive::listMembers(Common::ArchiveMemberList &list) const {
	for (EntryMap::const_iterator it = _entries.begin(); it != _entries.end(); ++it)
		list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(it->_key, *this)));
	return _entries.size();
}

const Common::ArchiveMemberPtr CLibArchive::getMember(const Common::Path &path) const {
	if (!hasFile(path))
		return Common::ArchiveMemberPtr();
	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
}

Common::SeekableReadStream *CLibArchive::createReadStreamForMember(const Common::Path &path) const {
	EntryMap::const_iterator it = _entries.find(path);
	if (it == _entries.end())
		return nullptr;

	const Entry &e = it->_value;
	if (e.flags & kFlagBlockCipher) {
		// Thin-ICE block cipher: never used by the shipped data
		warning("CLibArchive: %s in %s uses the unsupported block cipher", path.toString().c_str(), _name.c_str());
		return nullptr;
	}

	byte *stored = (byte *)malloc(e.storedSize ? e.storedSize : 1);
	_stream->seek(e.offset);
	if (_stream->read(stored, e.storedSize) != e.storedSize) {
		free(stored);
		warning("CLibArchive: short read of %s", path.toString().c_str());
		return nullptr;
	}
	xorKeyStream(stored, e.storedSize);

	if (!(e.flags & kFlagCompressed))
		return new Common::MemoryReadStream(stored, e.storedSize, DisposeAfterUse::YES);

	byte *data = (byte *)malloc(e.size ? e.size : 1);
	unsigned long outLen = e.size;
	const bool ok = Common::inflateZlib(data, &outLen, stored, e.storedSize);
	free(stored);
	if (!ok || outLen != e.size) {
		free(data);
		warning("CLibArchive: failed to inflate %s", path.toString().c_str());
		return nullptr;
	}
	return new Common::MemoryReadStream(data, e.size, DisposeAfterUse::YES);
}

} // End of namespace Gearhead
