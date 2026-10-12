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

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "gearhead/tlv.h"

namespace Gearhead {

TLVChunk::~TLVChunk() {
	for (uint i = 0; i < _children.size(); i++)
		delete _children[i];
}

int32 TLVChunk::asInt() const {
	return _data.size() >= 4 ? (int32)READ_LE_UINT32(_data.data()) : 0;
}

float TLVChunk::asFloat() const {
	if (_data.size() < 4)
		return 0.0f;
	const uint32 bits = READ_LE_UINT32(_data.data());
	float f;
	memcpy(&f, &bits, 4);
	return f;
}

Common::String TLVChunk::asString() const {
	return _data.empty() ? Common::String() : Common::String((const char *)_data.data(), _data.size());
}

Common::Array<int32> TLVChunk::asIntArray() const {
	Common::Array<int32> out;
	for (uint i = 0; i + 4 <= _data.size(); i += 4)
		out.push_back((int32)READ_LE_UINT32(_data.data() + i));
	return out;
}

Common::Array<float> TLVChunk::asFloatArray() const {
	Common::Array<float> out;
	for (uint i = 0; i + 4 <= _data.size(); i += 4) {
		const uint32 bits = READ_LE_UINT32(_data.data() + i);
		float f;
		memcpy(&f, &bits, 4);
		out.push_back(f);
	}
	return out;
}

const TLVChunk *TLVChunk::find(uint id) const {
	for (uint i = 0; i < _children.size(); i++)
		if (_children[i]->id() == id)
			return _children[i];
	return nullptr;
}

Common::Array<const TLVChunk *> TLVChunk::findAll(uint id) const {
	Common::Array<const TLVChunk *> out;
	for (uint i = 0; i < _children.size(); i++)
		if (_children[i]->id() == id)
			out.push_back(_children[i]);
	return out;
}

int32 TLVChunk::getInt(uint id, int32 def) const {
	const TLVChunk *c = find(id);
	return c ? c->asInt() : def;
}

float TLVChunk::getFloat(uint id, float def) const {
	const TLVChunk *c = find(id);
	return c ? c->asFloat() : def;
}

Common::String TLVChunk::getString(uint id, const Common::String &def) const {
	const TLVChunk *c = find(id);
	return c ? c->asString() : def;
}

bool TLVReader::parseRange(Common::SeekableReadStream &stream, uint32 end, Common::Array<TLVChunk *> &out, int depth) {
	if (depth > 32)
		return false;
	while ((uint32)stream.pos() + 8 <= end) {
		const uint32 tag = stream.readUint32LE();
		const uint32 size = stream.readUint32LE();
		const uint32 body = stream.pos();
		if (body + size > end)
			return false;
		TLVChunk *c = new TLVChunk(tag);
		out.push_back(c);
		if (c->isList()) {
			if (!parseRange(stream, body + size, c->_children, depth + 1))
				return false;
		} else {
			c->_data.resize(size);
			if (size && stream.read(c->_data.data(), size) != size)
				return false;
		}
		stream.seek(body + size);
	}
	return true;
}

TLVChunk *TLVReader::parse(Common::SeekableReadStream &stream) {
	Common::Array<TLVChunk *> top;
	const bool ok = parseRange(stream, stream.size(), top, 0);
	TLVChunk *root = nullptr;
	for (uint i = 0; i < top.size(); i++) {
		if (ok && !root)
			root = top[i];
		else
			delete top[i];
	}
	return root;
}

TLVChunk *TLVReader::parseFile(Common::SeekableReadStream &stream, uint rootId) {
	TLVChunk *root = parse(stream);
	if (root && (root->id() != rootId || !root->isList() || root->getInt(kTag_VERSION, -1) != 1)) {
		delete root;
		root = nullptr;
	}
	return root;
}

void TLVWriter::put32(uint32 v) {
	byte b[4];
	WRITE_LE_UINT32(b, v);
	for (int i = 0; i < 4; i++)
		_buf.push_back(b[i]);
}

void TLVWriter::markParent() {
	if (!_stack.empty())
		_stack.back().hasChildren = true;
}

void TLVWriter::open(uint rootId) {
	_buf.clear();
	_stack.clear();
	begin(rootId);
	writeInt(kTag_VERSION, 1);
}

bool TLVWriter::close(Common::WriteStream &out) {
	while (!_stack.empty())
		end();
	return out.write(_buf.data(), _buf.size()) == _buf.size();
}

void TLVWriter::begin(uint id) {
	markParent();
	put32(id & 0x0FFFFFFF);
	Open o;
	o.sizeOffset = _buf.size();
	o.hasChildren = false;
	_stack.push_back(o);
	put32(0);
}

void TLVWriter::end() {
	assert(!_stack.empty());
	const Open o = _stack.back();
	_stack.pop_back();
	const uint32 size = _buf.size() - o.sizeOffset - 4;
	WRITE_LE_UINT32(_buf.data() + o.sizeOffset, size);
	if (o.hasChildren) {
		uint32 tag = READ_LE_UINT32(_buf.data() + o.sizeOffset - 4);
		WRITE_LE_UINT32(_buf.data() + o.sizeOffset - 4, tag | 0x80000000);
	}
}

void TLVWriter::appendInt(int32 v) {
	put32((uint32)v);
}

void TLVWriter::appendFloat(float v) {
	uint32 bits;
	memcpy(&bits, &v, 4);
	put32(bits);
}

void TLVWriter::writeInt(uint id, int32 v) {
	markParent();
	put32((id & 0x0FFFFFFF) | 0x20000000);
	put32(4);
	put32((uint32)v);
}

void TLVWriter::writeFloat(uint id, float v) {
	markParent();
	put32((id & 0x0FFFFFFF) | 0x40000000);
	put32(4);
	appendFloat(v);
}

void TLVWriter::writeString(uint id, const Common::String &s) {
	markParent();
	put32((id & 0x0FFFFFFF) | 0x60000000);
	put32(s.size());
	for (uint i = 0; i < s.size(); i++)
		_buf.push_back((byte)s[i]);
}

void TLVWriter::writeBlob(uint id, const byte *data, uint32 size) {
	markParent();
	put32(id & 0x0FFFFFFF);
	put32(size);
	for (uint32 i = 0; i < size; i++)
		_buf.push_back(data[i]);
}

void TLVWriter::writeChunk(const TLVChunk &chunk) {
	if (chunk.isList()) {
		begin(chunk.id());
		for (uint i = 0; i < chunk.children().size(); i++)
			writeChunk(*chunk.children()[i]);
		end();
		return;
	}
	markParent();
	put32(chunk.tag());
	put32(chunk.data().size());
	for (uint i = 0; i < chunk.data().size(); i++)
		_buf.push_back(chunk.data()[i]);
}

} // End of namespace Gearhead
