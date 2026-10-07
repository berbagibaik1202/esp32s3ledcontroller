#include "test_assert.h"
#include "media/file_image.h"
#include "media/builtin_image.h"
#include "media/builtin_jpeg.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
namespace {
int allocations = 0;
bool rejectAllocation = false;
void* allocate(size_t bytes) {
  if (rejectAllocation) return nullptr;
  void* p = std::malloc(bytes); if (p) ++allocations; return p;
}
void release(void* p) { if (p) { --allocations; std::free(p); } }
class Reader : public media::FileReader {
 public:
  Reader(const uint8_t* data, size_t length) : data(data), length(length) {}
  size_t size() const override { return length; }
  size_t read(uint8_t* out, size_t count) override {
    if (offset >= stop) return 0;
    if (count > 7) count = 7;
    if (count > length - offset) count = length - offset;
    if (count > stop - offset) count = stop - offset;
    std::memcpy(out, data + offset, count); offset += count; return count;
  }
  const uint8_t* data; size_t length, offset = 0, stop = 1024 * 1024;
};
}
void testFileImages() {
  const char* error = nullptr;
  {
    media::FileImage image(allocate, release);
    Reader bmp(media::kExampleBmp, sizeof media::kExampleBmp);
    assert(image.load("/media/test.bmp", bmp, error));
    assert(image.source().width == 4);
    Reader shortRead(media::kExampleJpeg, sizeof media::kExampleJpeg); shortRead.stop = 20;
    assert(!image.load("/media/test.jpg", shortRead, error));
    assert(image.source().width == 4);
    Reader jpeg(media::kExampleJpeg, sizeof media::kExampleJpeg);
    rejectAllocation = true;
    assert(!image.load("/media/test.jpg", jpeg, error)); rejectAllocation = false;
    assert(image.source().width == 4);
    assert(image.load("/media/test.jpg", jpeg, error));
    assert(image.source().width == 32);
    Reader wrong(media::kExampleJpeg, sizeof media::kExampleJpeg);
    assert(!image.load("/media/wrong.bmp", wrong, error));
    assert(image.source().width == 32);
    Reader again(media::kExampleBmp, sizeof media::kExampleBmp);
    assert(image.load("/media/again.bmp", again, error));
    assert(allocations == 1);
    Reader huge(media::kExampleBmp, 1024U * 1024U + 1);
    assert(!image.load("/media/huge.bmp", huge, error));
    assert(!media::FileImage::validPath("/media/../test.bmp"));
    assert(!media::FileImage::validPath("/other/test.bmp"));
    assert(!media::FileImage::validPath("/media/sub/test.bmp"));
    assert(!media::FileImage::validPath("/media/test.gif"));
  }
  assert(allocations == 0);
  std::cout << "PASS: file image partial reads, rollback, format switching, size/path limits, ownership\n";
}
