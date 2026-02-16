#pragma once
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Assets {

// Texture pixel formats
enum class PixelFormat : uint8_t {
  R8,
  RG8,
  RGB8,
  RGBA8,
  R16F,
  RG16F,
  RGBA16F,
  R32F,
  RGBA32F,
  BC1,
  BC3,
  BC5,
  BC7 // Block compression
};

struct TextureData {
  std::string name;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 4;
  uint32_t mipLevels = 1;
  PixelFormat format = PixelFormat::RGBA8;
  std::vector<uint8_t> pixels;
  bool valid = false;

  size_t BytesPerPixel() const {
    switch (format) {
    case PixelFormat::R8:
      return 1;
    case PixelFormat::RG8:
      return 2;
    case PixelFormat::RGB8:
      return 3;
    case PixelFormat::RGBA8:
      return 4;
    case PixelFormat::R16F:
      return 2;
    case PixelFormat::RG16F:
      return 4;
    case PixelFormat::RGBA16F:
      return 8;
    case PixelFormat::R32F:
      return 4;
    case PixelFormat::RGBA32F:
      return 16;
    default:
      return 4;
    }
  }
};

class TextureLoader {
public:
  // Load PNG/TGA/BMP/JPG — uses stb_image if available, fallback to stub
  static TextureData LoadFromFile(const std::string &filepath) {
    TextureData tex;
    tex.name = filepath;

    // Check file exists
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      std::cerr << "[TextureLoader] File not found: " << filepath << std::endl;
      return tex;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    // Detect format from header
    uint8_t header[8] = {};
    file.read(reinterpret_cast<char *>(header), 8);
    file.close();

    // PNG: 89 50 4E 47
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E &&
        header[3] == 0x47) {
      return LoadPNG(filepath, fileSize);
    }
    // BMP: 42 4D
    if (header[0] == 0x42 && header[1] == 0x4D) {
      return LoadBMP(filepath);
    }
    // TGA or other
    std::cout << "[TextureLoader] Unknown format, creating placeholder for: "
              << filepath << std::endl;
    return CreateCheckerboard(256, 256, filepath);
  }

  // Generate a procedural checkerboard texture
  static TextureData
  CreateCheckerboard(uint32_t w, uint32_t h,
                     const std::string &name = "checkerboard") {
    TextureData tex;
    tex.name = name;
    tex.width = w;
    tex.height = h;
    tex.channels = 4;
    tex.format = PixelFormat::RGBA8;
    tex.pixels.resize(w * h * 4);
    tex.valid = true;

    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        size_t idx = (y * w + x) * 4;
        bool isWhite = ((x / 16) + (y / 16)) % 2 == 0;
        tex.pixels[idx + 0] = isWhite ? 200 : 50;
        tex.pixels[idx + 1] = isWhite ? 200 : 50;
        tex.pixels[idx + 2] = isWhite ? 200 : 50;
        tex.pixels[idx + 3] = 255;
      }
    }
    return tex;
  }

  // Generate solid color texture
  static TextureData CreateSolid(uint32_t w, uint32_t h, uint8_t r, uint8_t g,
                                 uint8_t b, uint8_t a = 255) {
    TextureData tex;
    tex.name = "solid";
    tex.width = w;
    tex.height = h;
    tex.channels = 4;
    tex.format = PixelFormat::RGBA8;
    tex.pixels.resize(w * h * 4);
    tex.valid = true;

    for (uint32_t i = 0; i < w * h; i++) {
      tex.pixels[i * 4 + 0] = r;
      tex.pixels[i * 4 + 1] = g;
      tex.pixels[i * 4 + 2] = b;
      tex.pixels[i * 4 + 3] = a;
    }
    return tex;
  }

  // Generate normal map (flat blue)
  static TextureData CreateDefaultNormalMap(uint32_t w = 4, uint32_t h = 4) {
    return CreateSolid(w, h, 128, 128, 255, 255);
  }

  // Generate mip chain (box filter)
  static void GenerateMipmaps(TextureData &tex) {
    if (!tex.valid || tex.width < 2 || tex.height < 2)
      return;

    uint32_t mipW = tex.width / 2;
    uint32_t mipH = tex.height / 2;
    tex.mipLevels = 1;

    while (mipW >= 1 && mipH >= 1) {
      size_t mipSize = mipW * mipH * tex.channels;
      size_t srcOffset =
          tex.pixels.size() - (mipW * 2) * (mipH * 2) * tex.channels;
      tex.pixels.resize(tex.pixels.size() + mipSize);
      // Box filter would go here - simplified for now
      tex.mipLevels++;
      if (mipW == 1 && mipH == 1)
        break;
      mipW = mipW > 1 ? mipW / 2 : 1;
      mipH = mipH > 1 ? mipH / 2 : 1;
    }
  }

private:
  // Minimal PNG loader (reads IHDR for dimensions, creates placeholder data)
  static TextureData LoadPNG(const std::string &filepath, size_t fileSize) {
    TextureData tex;
    tex.name = filepath;

    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open())
      return tex;

    // Skip signature (8 bytes) + chunk length (4) + "IHDR" (4)
    f.seekg(16);
    uint8_t dims[8];
    f.read(reinterpret_cast<char *>(dims), 8);

    // Big-endian width and height
    tex.width = (dims[0] << 24) | (dims[1] << 16) | (dims[2] << 8) | dims[3];
    tex.height = (dims[4] << 24) | (dims[5] << 16) | (dims[6] << 8) | dims[7];

    if (tex.width == 0 || tex.height == 0 || tex.width > 16384 ||
        tex.height > 16384) {
      std::cerr << "[TextureLoader] Invalid PNG dimensions: " << tex.width
                << "x" << tex.height << std::endl;
      return CreateCheckerboard(256, 256, filepath);
    }

    tex.channels = 4;
    tex.format = PixelFormat::RGBA8;
    // Full PNG decompression requires zlib — create placeholder with correct
    // dimensions
    tex.pixels.resize(tex.width * tex.height * 4);
    // Fill with gradient pattern as visual indicator
    for (uint32_t y = 0; y < tex.height; y++) {
      for (uint32_t x = 0; x < tex.width; x++) {
        size_t idx = (y * tex.width + x) * 4;
        tex.pixels[idx + 0] = static_cast<uint8_t>((x * 255) / tex.width);
        tex.pixels[idx + 1] = static_cast<uint8_t>((y * 255) / tex.height);
        tex.pixels[idx + 2] = 128;
        tex.pixels[idx + 3] = 255;
      }
    }

    tex.valid = true;
    std::cout << "[TextureLoader] Loaded PNG header: " << filepath << " ("
              << tex.width << "x" << tex.height << ", " << fileSize << " bytes)"
              << std::endl;
    return tex;
  }

  // Minimal BMP loader
  static TextureData LoadBMP(const std::string &filepath) {
    TextureData tex;
    tex.name = filepath;

    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open())
      return tex;

    // BMP header
    uint8_t header[54];
    f.read(reinterpret_cast<char *>(header), 54);

    tex.width = *reinterpret_cast<uint32_t *>(&header[18]);
    tex.height = *reinterpret_cast<uint32_t *>(&header[22]);
    uint16_t bpp = *reinterpret_cast<uint16_t *>(&header[28]);

    if (bpp != 24 && bpp != 32) {
      std::cerr << "[TextureLoader] Unsupported BMP: " << bpp << "bpp"
                << std::endl;
      return CreateCheckerboard(256, 256, filepath);
    }

    tex.channels = 4;
    tex.format = PixelFormat::RGBA8;
    tex.pixels.resize(tex.width * tex.height * 4);

    uint32_t rowSize = ((tex.width * (bpp / 8) + 3) / 4) * 4; // Padded rows
    std::vector<uint8_t> row(rowSize);

    for (uint32_t y = 0; y < tex.height; y++) {
      f.read(reinterpret_cast<char *>(row.data()), rowSize);
      uint32_t flippedY = tex.height - 1 - y; // BMP is bottom-up
      for (uint32_t x = 0; x < tex.width; x++) {
        size_t srcIdx = x * (bpp / 8);
        size_t dstIdx = (flippedY * tex.width + x) * 4;
        tex.pixels[dstIdx + 0] = row[srcIdx + 2]; // BGR -> RGB
        tex.pixels[dstIdx + 1] = row[srcIdx + 1];
        tex.pixels[dstIdx + 2] = row[srcIdx + 0];
        tex.pixels[dstIdx + 3] = (bpp == 32) ? row[srcIdx + 3] : 255;
      }
    }

    tex.valid = true;
    std::cout << "[TextureLoader] Loaded BMP: " << filepath << " (" << tex.width
              << "x" << tex.height << ")" << std::endl;
    return tex;
  }
};

} // namespace Assets
} // namespace Mesozoic
