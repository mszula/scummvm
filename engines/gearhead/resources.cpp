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

#include "common/algorithm.h"
#include "common/archive.h"
#include "common/debug.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"
#include "image/bmp.h"
#include "image/jpeg.h"
#include "image/tga.h"

#include "gearhead/archive.h"
#include "gearhead/detection.h"
#include "gearhead/resources.h"

namespace Gearhead {

Resources::Resources() : _cabinet(nullptr) {
}

Resources::~Resources() {
	for (ImageMap::iterator it = _images.begin(); it != _images.end(); ++it)
		delete it->_value.surface;
	for (ArchiveMap::iterator it = _archives.begin(); it != _archives.end(); ++it)
		delete it->_value;
	delete _cabinet;
}

void Resources::init(Common::Archive *cabinet, const Graphics::PixelFormat &screenFormat) {
	_cabinet = cabinet;
	_format = screenFormat;
}

Common::String Resources::normalize(const Common::String &path) {
	Common::String out;
	for (uint i = 0; i < path.size(); i++)
		out += (path[i] == '\\') ? '/' : path[i];
	while (out.hasPrefix("./"))
		out.erase(0, 2);
	return out;
}

Common::SeekableReadStream *Resources::openRaw(const Common::String &norm) {
	const Common::Path p(norm, '/');
	if (SearchMan.hasFile(p))
		return SearchMan.createReadStreamForMember(p);
	if (_cabinet) {
		// The InstallShield cabinet has no directories: look the file up by name
		const Common::Path base(p.baseName());
		if (_cabinet->hasFile(base))
			return _cabinet->createReadStreamForMember(base);
	}
	return nullptr;
}

CLibArchive *Resources::getArchive(const Common::String &normArchivePath) {
	ArchiveMap::iterator it = _archives.find(normArchivePath);
	if (it != _archives.end())
		return it->_value;

	CLibArchive *archive = nullptr;
	Common::SeekableReadStream *s = openRaw(normArchivePath);
	if (s) {
		archive = new CLibArchive();
		if (!archive->open(s, normArchivePath)) {
			delete archive;
			archive = nullptr;
		}
	}
	_archives[normArchivePath] = archive;
	return archive;
}

Common::SeekableReadStream *Resources::open(const Common::String &path) {
	const Common::String norm = normalize(path);
	Common::SeekableReadStream *s = openRaw(norm);
	if (s) {
		debugC(2, kDebugResources, "open %s: file", norm.c_str());
		return s;
	}

	// Try every path prefix as an archive; the first existing one is used
	for (size_t pos = norm.findFirstOf('/'); pos != Common::String::npos; pos = norm.findFirstOf('/', pos + 1)) {
		CLibArchive *archive = getArchive(Common::String(norm.c_str(), pos) + ".dat");
		if (!archive)
			continue;
		const Common::Path member(norm.substr(pos + 1), '/');
		s = archive->createReadStreamForMember(member);
		debugC(2, kDebugResources, "open %s: %s in %s", norm.c_str(), s ? "found" : "NOT found", archive->getName().c_str());
		return s;
	}
	debugC(1, kDebugResources, "open %s: not found", norm.c_str());
	return nullptr;
}

bool Resources::exists(const Common::String &path) {
	Common::SeekableReadStream *s = open(path);
	delete s;
	return s != nullptr;
}

Common::StringArray Resources::list(const Common::String &dir, const Common::String &pattern) {
	Common::StringArray names;
	Common::ArchiveMemberList members;
	SearchMan.listMatchingMembers(members, Common::Path(normalize(dir) + "/" + pattern, '/'));
	if (_cabinet)
		_cabinet->listMatchingMembers(members, Common::Path(pattern, '/'));

	for (Common::ArchiveMemberList::const_iterator it = members.begin(); it != members.end(); ++it) {
		const Common::String name = (*it)->getFileName();
		bool dup = false;
		for (uint i = 0; i < names.size() && !dup; i++)
			dup = names[i].equalsIgnoreCase(name);
		if (!dup)
			names.push_back(name);
	}
	Common::sort(names.begin(), names.end());
	return names;
}

Graphics::ManagedSurface *Resources::loadImage(const Common::String &path, const Graphics::PixelFormat *format) {
	Common::SeekableReadStream *s = open(path);
	if (!s)
		return nullptr;

	Common::String ext;
	const size_t dot = path.findLastOf('.');
	if (dot != Common::String::npos)
		ext = path.substr(dot + 1);
	ext.toLowercase();

	Image::ImageDecoder *dec = nullptr;
	if (ext == "tga") {
		dec = new Image::TGADecoder();
	} else if (ext == "bmp") {
		dec = new Image::BitmapDecoder();
	} else if (ext == "jpg" || ext == "jpeg") {
		Image::JPEGDecoder *jpeg = new Image::JPEGDecoder();
		jpeg->setOutputPixelFormat(Graphics::PixelFormat::createFormatRGB24());
		dec = jpeg;
	} else {
		warning("Resources: unsupported image type %s", path.c_str());
		delete s;
		return nullptr;
	}

	Graphics::ManagedSurface *out = nullptr;
	if (dec->loadStream(*s) && dec->getSurface()) {
		const Graphics::Surface *src = dec->getSurface();
		const Graphics::Palette &pal = dec->getPalette();
		Graphics::Surface *conv = src->convertTo(format ? *format : _format, pal.size() ? pal.data() : nullptr, pal.size());
		out = new Graphics::ManagedSurface();
		out->copyFrom(*conv);
		conv->free();
		delete conv;
	} else {
		warning("Resources: cannot decode %s", path.c_str());
	}
	delete dec;
	delete s;
	return out;
}

Graphics::ManagedSurface *Resources::getImage(const Common::String &name, bool useGfxDir) {
	const Common::String path = useGfxDir ? "Data\\Gfx24\\" + name : name;
	const Common::String key = normalize(path);
	ImageMap::iterator it = _images.find(key);
	if (it != _images.end()) {
		it->_value.refs++;
		return it->_value.surface;
	}

	debugC(1, kDebugResources, "Loading Resource: %s", path.c_str());
	Graphics::ManagedSurface *img = loadImage(path);
	if (!img)
		error("Error loading bitmap resource. %s", path.c_str());

	CachedImage c;
	c.surface = img;
	c.refs = 1;
	_images[key] = c;
	return img;
}

void Resources::release(Graphics::ManagedSurface *image) {
	for (ImageMap::iterator it = _images.begin(); it != _images.end(); ++it) {
		if (it->_value.surface == image) {
			it->_value.refs--;
			return;
		}
	}
}

void Resources::purge() {
	Common::StringArray dead;
	for (ImageMap::iterator it = _images.begin(); it != _images.end(); ++it) {
		if (it->_value.refs < 1) {
			delete it->_value.surface;
			dead.push_back(it->_key);
		}
	}
	for (uint i = 0; i < dead.size(); i++)
		_images.erase(dead[i]);
}

Common::SeekableReadStream *Resources::openSound(const Common::String &name) {
	return open("Data\\Sound16\\" + name);
}

} // End of namespace Gearhead
