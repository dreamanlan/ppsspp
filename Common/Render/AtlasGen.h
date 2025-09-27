#pragma once

#include <vector>
#include <algorithm>

#include "Common/CommonTypes.h"

#include "Common/Render/TextureAtlas.h"

struct ImageU8 {
	std::vector<std::vector<u8>> dat;
	void resize(int x, int y) {
		dat.resize(y);
		for (int i = 0; i < y; i++)
			dat[i].resize(x);
	}
	int width() const {
		return (int)dat[0].size();
	}
	int height() const {
		return (int)dat.size();
	}
	void set(int sx, int sy, int ex, int ey, unsigned char fil) {
		for (int y = sy; y < ey; y++)
			std::fill(dat[y].begin() + sx, dat[y].begin() + ex, fil);
	}
};

struct Image {
	Image() = default;
	Image(const Image &) = delete;
	Image &operator=(const Image &) = delete;
	Image(Image &&) = default;
	Image &operator=(Image &&) = default;

	float scale = 1.0f;

	int w = 0;
	int h = 0;

	// WARNING: This only preserves data correctly if w stays the same. Which it does, in our application.
	void resize(int x, int y) {
		w = x;
		h = y;
		dat.resize(w * h);
	}
	int width() const {
		return w;
	}
	int height() const {
		return h;
	}
	void set1(int x, int y, u32 col) {
		dat[y * w + x] = col;
	}
	void fill(u32 col) {
		for (int i = 0; i < w * h; i++) {
			dat[i] = col;
		}
	}
	const u32 *data() const {
		return dat.data();
	}
	u32 get1(int x, int y) const { return dat[y * w + x]; }
	void copyfrom(const Image &img, int ox, int oy, bool redToWhiteAlpha);
	bool LoadPNG(const char *png_name);
	void SavePNG(const char *png_name);
	void SaveZIM(const char *zim_name, int zim_format);
	bool IsEmpty() const { return dat.empty(); }
// private:
	std::vector<u32> dat;
};

// Slow.
void AddDropShadow(Image &img, int shadowSize, float intensity);

struct Data {
	// item ID
	int id;
	// item width and height
	int w, h;
	// dimensions of its spot in the world
	int sx, sy, ex, ey;
	// offset from the origin
	float ox, oy;
	float voffset;  // to apply at the end
	// distance to move the origin forward
	float wx;

	float scale;

	bool redToWhiteAlpha;
	int charNum;
};

struct Bucket {
	std::vector<Image> images;
	std::vector<Data> data;
	void AddItem(Image &&img, const Data &dat) {
		images.emplace_back(std::move(img));
		data.emplace_back(dat);
	}
	void AddImage(Image &&img, int id);
	std::vector<Data> Resolve(int image_width, Image &dest);
};

AtlasImage ToAtlasImage(int id, std::string_view name, float tw, float th, const std::vector<Data> &results);
