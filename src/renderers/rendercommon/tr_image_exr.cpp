/*
===========================================================================
OpenEXR loader (RGBA -> 8-bit RGBA) for idTech3
===========================================================================
*/

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_image_loaders.h"
#include "tr_fs_compat.h"
#include "tr_public.h"
}

#ifdef USE_OPENEXR
#include <ImfRgbaFile.h>
#include <ImfIO.h>
#include <Iex.h>
#include <ImathBox.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
class MemoryIStream : public OPENEXR_IMF_INTERNAL_NAMESPACE::IStream {
public:
	MemoryIStream(const char *name, const unsigned char *data, size_t size)
		: OPENEXR_IMF_INTERNAL_NAMESPACE::IStream(name), data_(data), size_(size), pos_(0) {}

	bool read(char c[], int n) override {
		if (n < 0) {
			throw IEX_NAMESPACE::InputExc("OpenEXR read: invalid size");
		}
		if (pos_ + static_cast<size_t>(n) > size_) {
			throw IEX_NAMESPACE::InputExc("OpenEXR read: unexpected EOF");
		}
		memcpy(c, data_ + pos_, static_cast<size_t>(n));
		pos_ += static_cast<size_t>(n);
		return pos_ < size_;
	}

	uint64_t tellg() override {
		return static_cast<uint64_t>(pos_);
	}

	void seekg(uint64_t pos) override {
		if (pos > size_) {
			throw IEX_NAMESPACE::InputExc("OpenEXR read: invalid seek");
		}
		pos_ = static_cast<size_t>(pos);
	}

	int64_t size() override {
		return static_cast<int64_t>(size_);
	}

private:
	const unsigned char *data_;
	size_t size_;
	size_t pos_;
};

static inline unsigned char ClampToByte(float v) {
	if (!std::isfinite(v)) {
		return 0;
	}
	v = std::max(0.0f, std::min(v, 1.0f));
	return static_cast<unsigned char>(v * 255.0f + 0.5f);
}
} // namespace
#endif

extern "C" void R_LoadEXR(const char *name, byte **pic, int *width, int *height)
{
	if (!name || !pic) {
		return;
	}

	*pic = NULL;
	if (width) {
		*width = 0;
	}
	if (height) {
		*height = 0;
	}

#ifndef USE_OPENEXR
	(void)name;
	return;
#else
	void *fileData = NULL;
	int length = FS_ReadFileConst(name, &fileData);
	if (length <= 0 || !fileData) {
		return;
	}

	byte *out = NULL;
	try {
		MemoryIStream stream(name, static_cast<const unsigned char *>(fileData), static_cast<size_t>(length));
		OPENEXR_IMF_NAMESPACE::RgbaInputFile file(stream);
		IMATH_NAMESPACE::Box2i dw = file.dataWindow();

		const int w = dw.max.x - dw.min.x + 1;
		const int h = dw.max.y - dw.min.y + 1;
		if (w <= 0 || h <= 0) {
			throw IEX_NAMESPACE::InputExc("OpenEXR read: invalid data window");
		}

		std::vector<OPENEXR_IMF_NAMESPACE::Rgba> pixels;
		pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
		OPENEXR_IMF_NAMESPACE::Rgba *base = pixels.data() - dw.min.x - dw.min.y * w;
		file.setFrameBuffer(base, 1, w);
		file.readPixels(dw.min.y, dw.max.y);

		const size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
		out = static_cast<byte *>(ri.Malloc(total * 4u));
		if (!out) {
			throw IEX_NAMESPACE::InputExc("OpenEXR read: allocation failed");
		}
		for (size_t i = 0; i < total; ++i) {
			const OPENEXR_IMF_NAMESPACE::Rgba &p = pixels[i];
			const size_t o = i * 4u;
			out[o + 0] = ClampToByte(static_cast<float>(p.r));
			out[o + 1] = ClampToByte(static_cast<float>(p.g));
			out[o + 2] = ClampToByte(static_cast<float>(p.b));
			out[o + 3] = ClampToByte(static_cast<float>(p.a));
		}

		*pic = out;
		if (width) {
			*width = w;
		}
		if (height) {
			*height = h;
		}
	} catch (const std::exception &e) {
		ri.Printf(PRINT_WARNING, "OpenEXR load failed for %s: %s\n", name, e.what());
		if (out) {
			ri.Free(out);
		}
	} catch (...) {
		ri.Printf(PRINT_WARNING, "OpenEXR load failed for %s: unknown error\n", name);
		if (out) {
			ri.Free(out);
		}
	}

	ri.FS_FreeFile(fileData);
#endif
}
