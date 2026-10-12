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

#ifndef GEARHEAD_TLV_H
#define GEARHEAD_TLV_H

#include "common/array.h"
#include "common/str.h"

#include "gearhead/tags.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Gearhead {

/**
 * One chunk of the VMech tagged file format:
 *   uint32 tag  -- tag >> 28 = value type, tag & 0x0FFFFFFF = field id (TagId)
 *   uint32 size -- payload size
 *   payload     -- raw bytes, or a sequence of chunks for lists
 */
class TLVChunk {
public:
	enum Type {
		kTypeBlob = 0x0,
		kTypeInt = 0x2,
		kTypeFloat = 0x4,
		kTypeString = 0x6,
		kTypeList = 0x8
	};

	TLVChunk(uint32 tag) : _tag(tag) {}
	~TLVChunk();

	uint32 tag() const { return _tag; }
	uint type() const { return _tag >> 28; }
	uint id() const { return _tag & 0x0FFFFFFF; }
	bool isList() const { return type() == kTypeList; }

	const Common::Array<byte> &data() const { return _data; }
	const Common::Array<TLVChunk *> &children() const { return _children; }

	int32 asInt() const;
	float asFloat() const;
	Common::String asString() const;
	/** Raw blob interpreted as consecutive little-endian values */
	Common::Array<int32> asIntArray() const;
	Common::Array<float> asFloatArray() const;

	const TLVChunk *find(uint id) const;
	Common::Array<const TLVChunk *> findAll(uint id) const;
	int32 getInt(uint id, int32 def = 0) const;
	float getFloat(uint id, float def = 0.0f) const;
	Common::String getString(uint id, const Common::String &def = Common::String()) const;

private:
	friend class TLVReader;
	uint32 _tag;
	Common::Array<byte> _data;
	Common::Array<TLVChunk *> _children;
};

class TLVReader {
public:
	/**
	 * Parse a whole file. Returns the first top-level chunk (the root, e.g.
	 * CARFILE) or nullptr if the data is malformed. The caller owns the result.
	 */
	static TLVChunk *parse(Common::SeekableReadStream &stream);

	/** parse() plus a check of the root id and VERSION == 1 */
	static TLVChunk *parseFile(Common::SeekableReadStream &stream, uint rootId);

private:
	static bool parseRange(Common::SeekableReadStream &stream, uint32 end, Common::Array<TLVChunk *> &out, int depth);
};

/**
 * In-memory writer with the semantics of the original TagWriter: a chunk
 * becomes a list (bit 31) only if sub-chunks were written into it, so raw
 * int arrays are written as begin() + appendInt()... + end() blobs.
 */
class TLVWriter {
public:
	TLVWriter() {}

	/** Starts the root chunk and writes VERSION = 1, like TagWriter_open */
	void open(uint rootId);
	/** Ends the root chunk and writes the buffer */
	bool close(Common::WriteStream &out);

	void begin(uint id);
	void end();
	void appendInt(int32 v);
	void appendFloat(float v);

	void writeInt(uint id, int32 v);
	void writeFloat(uint id, float v);
	void writeString(uint id, const Common::String &s);
	void writeBlob(uint id, const byte *data, uint32 size);
	/** Re-serialise a parsed chunk tree unchanged */
	void writeChunk(const TLVChunk &chunk);

	const Common::Array<byte> &buffer() const { return _buf; }

private:
	void put32(uint32 v);
	void markParent();

	struct Open {
		uint32 sizeOffset;
		bool hasChildren;
	};
	Common::Array<byte> _buf;
	Common::Array<Open> _stack;
};

} // End of namespace Gearhead

#endif // GEARHEAD_TLV_H
