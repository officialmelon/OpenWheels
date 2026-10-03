#include "platform/common/AppleImage.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <zlib.h>

USING_NS_CC;

namespace openwheels {
namespace {

uint32_t be32(const unsigned char* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

// Produces premultiplied RGBA (CgBI pixels are premultiplied BGRA).
bool decodeCgBI(const Data& file, std::vector<unsigned char>& rgba, int& width, int& height) {
    const unsigned char* p = file.getBytes();
    const ssize_t n = file.getSize();
    if (n < 8 + 12) return false;
    ssize_t off = 8;
    int bitDepth = 0, colorType = 0, interlace = 0;
    std::vector<unsigned char> idat;
    width = height = 0;
    while (off + 12 <= n) {
        const uint32_t len = be32(p + off);
        const unsigned char* type = p + off + 4;
        const unsigned char* body = p + off + 8;
        if (off + 12 + static_cast<ssize_t>(len) > n) return false;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            width = static_cast<int>(be32(body));
            height = static_cast<int>(be32(body + 4));
            bitDepth = body[8];
            colorType = body[9];
            interlace = body[12];
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), body, body + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        off += 12 + len;
    }
    if (width <= 0 || height <= 0 || bitDepth != 8 || interlace != 0) return false;
    const int bpp = colorType == 6 ? 4 : colorType == 2 ? 3 : 0;
    if (bpp == 0) return false;
    const size_t stride = static_cast<size_t>(width) * bpp;
    std::vector<unsigned char> raw((stride + 1) * height);

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -15) != Z_OK) return false;
    zs.next_in = idat.data();
    zs.avail_in = static_cast<uInt>(idat.size());
    zs.next_out = raw.data();
    zs.avail_out = static_cast<uInt>(raw.size());
    const int rc = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END && zs.avail_out != 0) return false;

    std::vector<unsigned char> prev(stride, 0), cur(stride);
    rgba.assign(static_cast<size_t>(width) * height * 4, 0);
    for (int y = 0; y < height; ++y) {
        const unsigned char* line = raw.data() + y * (stride + 1);
        const int filter = line[0];
        for (size_t x = 0; x < stride; ++x) {
            const int a = x >= static_cast<size_t>(bpp) ? cur[x - bpp] : 0;
            const int b = prev[x];
            const int c = x >= static_cast<size_t>(bpp) ? prev[x - bpp] : 0;
            int v = line[1 + x];
            switch (filter) {
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
                default: break;
            }
            cur[x] = static_cast<unsigned char>(v);
        }
        for (int x = 0; x < width; ++x) {
            unsigned char* out = rgba.data() + (static_cast<size_t>(y) * width + x) * 4;
            const unsigned char* in = cur.data() + static_cast<size_t>(x) * bpp;
            out[0] = in[2];
            out[1] = in[1];
            out[2] = in[0];
            out[3] = bpp == 4 ? in[3] : 255;
        }
        prev.swap(cur);
    }
    return true;
}

}  // namespace

bool isCgBI(const Data& file) {
    return file.getSize() > 16 && std::memcmp(file.getBytes() + 12, "CgBI", 4) == 0;
}

Image* createImage(const Data& file) {
    if (file.isNull()) return nullptr;
    Image* image = new (std::nothrow) Image();
    if (!image) return nullptr;
    bool ok;
    if (isCgBI(file)) {
        std::vector<unsigned char> rgba;
        int w = 0, h = 0;
        ok = decodeCgBI(file, rgba, w, h) &&
             image->initWithRawData(rgba.data(), static_cast<ssize_t>(rgba.size()), w, h, 8, true);
    } else {
        ok = image->initWithImageData(file.getBytes(), file.getSize());
    }
    if (!ok) {
        image->release();
        return nullptr;
    }
    return image;
}

Texture2D* addTexture(const std::string& path) {
    TextureCache* cache = Director::getInstance()->getTextureCache();
    const std::string key = FileUtils::getInstance()->fullPathForFilename(path);
    if (key.empty()) return nullptr;
    if (Texture2D* cached = cache->getTextureForKey(key)) return cached;
    const Data data = FileUtils::getInstance()->getDataFromFile(key);
    if (!isCgBI(data)) return cache->addImage(key);
    Image* image = createImage(data);
    if (!image) {
        log("AppleImage: cannot decode %s", key.c_str());
        return nullptr;
    }
    Texture2D* texture = cache->addImage(image, key);
    image->release();
    return texture;
}

}  // namespace openwheels
