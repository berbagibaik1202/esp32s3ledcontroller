#include "file_image.h"
#include <cstring>

namespace media {
bool FileImage::validPath(const char* path) {
  if (!path || std::strncmp(path, "/media/", 7)) return false;
  size_t n = 7;
  for (; path[n] && n < 96; ++n) {
    const char c = path[n];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
  }
  if (n >= 96 || n <= 11 || std::strstr(path + 7, "..")) return false;
  const char* ext = std::strrchr(path, '.');
  return ext && (!std::strcmp(ext, ".bmp") || !std::strcmp(ext, ".jpg") || !std::strcmp(ext, ".jpeg"));
}
bool FileImage::load(const char* path, FileReader& reader, const char*& error) {
  error = nullptr;
  if (!validPath(path)) { error = "Invalid media path/extension"; return false; }
  const size_t length = reader.size();
  if (!length || length > 1024U * 1024U) { error = "File size outside 1..1 MiB"; return false; }
  if (!allocate_ || !release_) { error = "File allocator unavailable"; return false; }
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
  const bool jpeg = std::strcmp(std::strrchr(path, '.'), ".bmp") != 0;
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
