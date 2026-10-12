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

#ifndef GEARHEAD_RESOURCES_H
#define GEARHEAD_RESOURCES_H

#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"
#include "common/str-array.h"
#include "graphics/pixelformat.h"

namespace Common {
class Archive;
class SeekableReadStream;
}

namespace Graphics {
class ManagedSurface;
}

namespace Gearhead {

class CLibArchive;

/**
 * File access with the semantics of the original (Resourcer + ALIBI lstream):
 * paths are DOS style ("Data\Gfx24\Fonts\Standard.tga"). A real file always
 * wins; otherwise each path prefix is tried as an archive ("Data.dat",
 * "Data\Gfx24.dat", ...) and the rest of the path is looked up inside it.
 * When the game runs from the CD, files are additionally looked up by bare
 * name in the (flat) InstallShield cabinet.
 */
class Resources {
public:
	Resources();
	~Resources();

	/** @param cabinet InstallShield archive with the installed files, or nullptr (owned) */
	void init(Common::Archive *cabinet, const Graphics::PixelFormat &screenFormat);

	Common::SeekableReadStream *open(const Common::String &path);
	bool exists(const Common::String &path);

	/** Names (without directory) of the files in @p dir matching @p pattern (e.g. "Data\Cars", "*.car"), sorted */
	Common::StringArray list(const Common::String &dir, const Common::String &pattern);

	/**
	 * Resourcer_getImage: cached and reference counted. With @p useGfxDir the
	 * name is relative to "Data\Gfx24\" (the engine always runs the true-colour
	 * mode of the original). The surface is in the screen pixel format.
	 * Missing images are fatal, like in the original.
	 */
	Graphics::ManagedSurface *getImage(const Common::String &name, bool useGfxDir = true);
	void release(Graphics::ManagedSurface *image);
	/** Resourcer_purge: free every image whose reference count dropped below 1 */
	void purge();

	/** Uncached load, converted to @p format (default: screen format). Returns nullptr on failure. */
	Graphics::ManagedSurface *loadImage(const Common::String &path, const Graphics::PixelFormat *format = nullptr);

	/** Raw bytes of a sound file below "Data\Sound16\" (Resourcer_getSound) */
	Common::SeekableReadStream *openSound(const Common::String &name);

	const Graphics::PixelFormat &screenFormat() const { return _format; }

	static Common::String normalize(const Common::String &path);

private:
	Common::SeekableReadStream *openRaw(const Common::String &norm);
	CLibArchive *getArchive(const Common::String &normArchivePath);

	struct CachedImage {
		Graphics::ManagedSurface *surface;
		int refs;
	};

	typedef Common::HashMap<Common::String, CLibArchive *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> ArchiveMap;
	typedef Common::HashMap<Common::String, CachedImage, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> ImageMap;

	ArchiveMap _archives;
	ImageMap _images;
	Common::Archive *_cabinet;
	Graphics::PixelFormat _format;
};

} // End of namespace Gearhead

#endif // GEARHEAD_RESOURCES_H
