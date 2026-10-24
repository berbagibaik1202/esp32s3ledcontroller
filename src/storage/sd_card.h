#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>

namespace storage {

struct FileItem {
  String name;
  String path;
  size_t size;
  bool isDirectory;
  bool isAudio;
  bool isVideo;
  bool isImage;
};

struct SdStatus {
  bool mounted;
  uint8_t cardType; // 0=none, 1=MMC, 2=SDSC, 3=SDHC/SDXC
  uint64_t totalBytes;
  uint64_t usedBytes;
  uint64_t freeBytes;
  String cardTypeName;
  String lastError;
};

class SdStorage {
 public:
  static SdStorage& instance();

  // Initializes SD card with SPI pins without blocking/hanging if card is missing
  bool begin(int csPin, int sckPin, int mosiPin, int misoPin);
  bool remount();
  void end();

  bool isMounted() const { return mounted_; }
  SdStatus status();

  // Directory & file operations
  bool listFiles(const char* dirPath, std::vector<FileItem>& items);
  bool exists(const char* path);
  bool remove(const char* path);
  bool rename(const char* oldPath, const char* newPath);
  bool mkdir(const char* dirPath);

  // File open helper
  File open(const char* path, const char* mode = "r");

  // Validate path against traversal ("..") and illegal characters
  static bool isValidPath(const char* path);
  static bool sanitizePath(const char* input, String& cleanPath);

  // File extension detectors
  static bool isAudioFile(const char* path);
  static bool isVideoFile(const char* path);
  static bool isImageFile(const char* path);

 private:
  SdStorage();
  SPIClass spiBus_;
  bool mounted_{false};
  int csPin_{-1};
  int sckPin_{-1};
  int mosiPin_{-1};
  int misoPin_{-1};
  String lastError_{"Belum diinisialisasi"};
};

inline SdStorage& sd() { return SdStorage::instance(); }

}  // namespace storage
