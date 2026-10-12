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

#ifndef GEARHEAD_ARCHIVE_H
#define GEARHEAD_ARCHIVE_H

#include "common/archive.h"
#include "common/hashmap.h"
#include "common/path.h"

namespace Common {
class SeekableReadStream;
}

namespace Gearhead {

/**
 * The "lstream" data archives of the ALIBI library (Gfx24.dat, Sound16.dat, ...).
 *
 * After a NUL-terminated text banner come 16 bytes a[8], b[8] with
 * a ^ b == "cLib!317", followed by a linked list of 0x13C-byte directory
 * entries. Every buffer (directory entry or file body) is XOR-ed with a
 * 1024-byte key stream starting at index 0; file bodies are additionally
 * zlib-compressed (flag bit 1). Member names use '/' and are matched
 * case-insensitively.
 */
class CLibArchive : public Common::Archive {
public:
	CLibArchive();
	~CLibArchive() override;

	/** Takes ownership of the stream. */
	bool open(Common::SeekableReadStream *stream, const Common::String &name);

	bool hasFile(const Common::Path &path) const override;
	int listMembers(Common::ArchiveMemberList &list) const override;
	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override;
	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override;

	const Common::String &getName() const { return _name; }

private:
	enum {
		kFlagBlockCipher = 1,
		kFlagCompressed = 2,
		kEntrySize = 0x13C
	};

	struct Entry {
		uint32 flags;
		uint32 offset;
		uint32 storedSize;
		uint32 size;
	};

	typedef Common::HashMap<Common::Path, Entry, Common::Path::IgnoreCase_Hash, Common::Path::IgnoreCase_EqualTo> EntryMap;

	void xorKeyStream(byte *buf, uint32 size) const;

	EntryMap _entries;
	Common::SeekableReadStream *_stream;
	Common::String _name;
	byte _keyStream[1024];
};

} // End of namespace Gearhead

#endif // GEARHEAD_ARCHIVE_H
