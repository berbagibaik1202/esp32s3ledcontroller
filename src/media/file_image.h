#pragma once
#include "bmp.h"
#include "jpeg.h"

namespace media {
// Reader lifetime belongs to caller. Zero read before declared size means failure.
class FileReader {
 public:
  virtual ~FileReader() {}
  virtual size_t size() const = 0;
  virtual size_t read(uint8_t* output, size_t count) = 0;
};
class FileImage {
 public:
  FileImage(JpegImage::Allocate allocate, JpegImage::Release release)
      : allocate_(allocate), release_(release), jpeg_(allocate, release) {}
  ~FileImage() { if (bmpData_) release_(bmpData_); }
  FileImage(const FileImage&) = delete;
  FileImage& operator=(const FileImage&) = delete;
  // Reader starts at offset zero. Failed load retains the active image.
  bool load(const char* path, FileReader& reader, const char*& error);
  ImageSource source() const;
  static bool validPath(const char* path);
 private:
  JpegImage::Allocate allocate_;
  JpegImage::Release release_;
  BmpImage bmp_;
  JpegImage jpeg_;
  uint8_t* bmpData_ = nullptr;
  bool isJpeg_ = false;
};
}
