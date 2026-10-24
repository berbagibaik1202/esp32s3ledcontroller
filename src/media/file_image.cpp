#include "file_image.h"
#include <cstring>

namespace media {
bool FileImage::validPath(const char* path) {
  if (!path) return false;
  size_t n = 0;
  if (!std::strncmp(path, "/media/", 7)) {
    n = 7;
  } else if (!std::strncmp(path, "/sd/media/", 10)) {
    n = 10;
  } else if (!std::strncmp(path, "/sd/", 4)) {
    n = 4;
  } else {
    return false;
  }
  for (; path[n] && n < 120; ++n) {
    const char c = path[n];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/')) return false;
  }
  if (n >= 120 || std::strstr(path, "..")) return false;
  const char* ext = std::strrchr(path, '.');
  return ext && (!std::strcmp(ext, ".bmp") || !std::strcmp(ext, ".jpg") || !std::strcmp(ext, ".jpeg") ||
                 !std::strcmp(ext, ".mjpeg") || !std::strcmp(ext, ".mjpg"));
}

bool FileImage::load(const char* path, FileReader& reader, const char*& error) {
  error = nullptr;
  if (!validPath(path)) { error = "Invalid media path/extension"; return false; }
  const size_t length = reader.size();
  if (!length || length > 4096U * 1024U) { error = "File size outside 1..4 MiB"; return false; }
  if (!allocate_ || !release_) { error = "File allocator unavailable"; return false; }

  const char* ext = std::strrchr(path, '.');
  const bool isMjpeg = ext && (!std::strcmp(ext, ".mjpeg") || !std::strcmp(ext, ".mjpg"));
  const bool isBmp = ext && (!std::strcmp(ext, ".bmp"));

  if (isMjpeg) {
    const size_t testLen = length < 65536U ? length : 65536U;
    auto* data = static_cast<uint8_t*>(allocate_(testLen));
    if (!data) { error = "MJPEG buffer allocation failed"; return false; }
    size_t count = reader.read(data, testLen);
    if (count < 4 || data[0] != 0xFF || data[1] != 0xD8) {
      release_(data);
      error = "File MJPEG harus dimulai dengan header JPEG (0xFF 0xD8)";
      return false;
    }
    size_t eoiIdx = 0;
    for (size_t i = 1; i < count; ++i) {
      if (data[i - 1] == 0xFF && data[i] == 0xD9) {
        eoiIdx = i + 1;
        break;
      }
    }
    if (eoiIdx == 0) {
      release_(data);
      isJpeg_ = false;
      return true;
    }
    const bool success = jpeg_.open(data, eoiIdx, error);
    release_(data);
    if (!success) return false;
    isJpeg_ = true;
    return true;
  }

  auto* data = static_cast<uint8_t*>(allocate_(length));
  if (!data) { error = "File allocation failed"; return false; }
  size_t offset = 0;
  while (offset < length) {
    const size_t count = reader.read(data + offset, length - offset);
    if (!count || count > length - offset) {
      release_(data); error = "Incomplete file read"; return false;
    }
    offset += count;
  }
  const bool jpeg = !isBmp;
  BmpImage candidate;
  const bool success = jpeg ? jpeg_.open(data, length, error) : candidate.open(data, length, error);
  if (!success) { release_(data); return false; }
  if (bmpData_) release_(bmpData_);
  bmpData_ = nullptr;
  if (jpeg) release_(data);
  else { bmp_ = candidate; bmpData_ = data; jpeg_.reset(); }
  isJpeg_ = jpeg;
  return true;
}

ImageSource FileImage::source() const {
  return isJpeg_ ? jpeg_.source() : bmp_.source();
}
}
