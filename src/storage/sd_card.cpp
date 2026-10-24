#include "sd_card.h"

namespace storage {

SdStorage::SdStorage() : spiBus_(FSPI) {}

SdStorage& SdStorage::instance() {
  static SdStorage s_instance;
  return s_instance;
}

bool SdStorage::begin(int csPin, int sckPin, int mosiPin, int misoPin) {
  csPin_ = csPin;
  sckPin_ = sckPin;
  mosiPin_ = mosiPin;
  misoPin_ = misoPin;
  return remount();
}

bool SdStorage::remount() {
  if (csPin_ < 0 || sckPin_ < 0 || mosiPin_ < 0 || misoPin_ < 0) return false;
  end();

  int testMosi[2] = {mosiPin_, misoPin_};
  int testMiso[2] = {misoPin_, mosiPin_};

  for (int attempt = 0; attempt < 2; ++attempt) {
    int curMosi = testMosi[attempt];
    int curMiso = testMiso[attempt];

    // Reset pins to clear any leftover peripheral matrix states
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, HIGH);
    pinMode(curMiso, INPUT_PULLUP);

    // Initialize custom SPI bus
    spiBus_.begin(sckPin_, curMiso, curMosi, csPin_);

    // SD card SPI specification requires at least 74 dummy clock pulses with CS=HIGH and MOSI=HIGH
    digitalWrite(csPin_, HIGH);
    for (int i = 0; i < 16; ++i) {
      spiBus_.transfer(0xFF);
    }
    delay(20);

    // Standard SD Card SPI initialization frequency is 400 kHz (safe for breadboards & jumper wires)
    bool success = SD.begin(csPin_, spiBus_, 400000, "/sd", 8, false);
    if (!success) {
      digitalWrite(csPin_, HIGH);
      delay(40);
      success = SD.begin(csPin_, spiBus_, 4000000, "/sd", 8, false);
    }

    if (success) {
      uint8_t cardType = SD.cardType();
      if (cardType != CARD_NONE) {
        mounted_ = true;
        lastError_ = "Siap";
        if (attempt == 1) {
          Serial.printf("[SD] Auto-detect: MOSI & MISO otomatis dibalik (MOSI=%d, MISO=%d)\n",
                        curMosi, curMiso);
        }
        mkdir("/media");
        mkdir("/media/audio");
        mkdir("/media/images");
        mkdir("/media/video");
        mkdir("/playlists");
        mkdir("/config");

        Serial.printf("[SD] PASS: microSD terpasang (Tipe=%u, %llu MB total, %llu MB bebas)\n",
                      cardType,
                      SD.totalBytes() / (1024ULL * 1024ULL),
                      (SD.totalBytes() - SD.usedBytes()) / (1024ULL * 1024ULL));
        return true;
      }
      SD.end();
    }
  }

  lastError_ = "Kartu microSD tidak terdeteksi atau gagal mount (pastikan format FAT32)";
  Serial.printf("[SD] %s (CS=%d SCK=%d MOSI=%d MISO=%d)\n",
                lastError_.c_str(), csPin_, sckPin_, mosiPin_, misoPin_);
  return false;
}

void SdStorage::end() {
  if (mounted_) {
    SD.end();
    mounted_ = false;
    lastError_ = "microSD dilepas";
  }
}

SdStatus SdStorage::status() {
  SdStatus s;
  s.mounted = mounted_;
  s.cardType = mounted_ ? SD.cardType() : CARD_NONE;
  s.totalBytes = mounted_ ? SD.totalBytes() : 0;
  s.usedBytes = mounted_ ? SD.usedBytes() : 0;
  s.freeBytes = (s.totalBytes >= s.usedBytes) ? (s.totalBytes - s.usedBytes) : 0;
  s.lastError = lastError_;

  switch (s.cardType) {
    case CARD_MMC: s.cardTypeName = "MMC"; break;
    case CARD_SD: s.cardTypeName = "SDSC"; break;
    case CARD_SDHC: s.cardTypeName = "SDHC/SDXC"; break;
    default: s.cardTypeName = mounted_ ? "UNKNOWN" : "NONE"; break;
  }
  return s;
}

bool SdStorage::isValidPath(const char* path) {
  if (!path || !path[0]) return false;
  // Prevent directory traversal
  if (strstr(path, "..")) return false;
  if (strstr(path, "//")) return false;

  // Basic character validity check
  const size_t len = strlen(path);
  if (len > 120) return false;
  for (size_t i = 0; i < len; ++i) {
    char c = path[i];
    if (c < 32 || c > 126 || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      return false;
    }
  }
  return true;
}

bool SdStorage::sanitizePath(const char* input, String& cleanPath) {
  if (!isValidPath(input)) return false;
  String s = input;
  s.trim();
  if (!s.startsWith("/")) s = "/" + s;
  // If user passed "/sd/media/...", normalize to "/media/..." for SD.open if mounted at /sd
  if (s.startsWith("/sd/")) s = s.substring(3);
  cleanPath = s;
  return true;
}

bool SdStorage::isAudioFile(const char* path) {
  if (!path) return false;
  String s = path;
  s.toLowerCase();
  return s.endsWith(".wav") || s.endsWith(".mp3") || s.endsWith(".aac") || s.endsWith(".pcm");
}

bool SdStorage::isVideoFile(const char* path) {
  if (!path) return false;
  String s = path;
  s.toLowerCase();
  return s.endsWith(".mjpeg") || s.endsWith(".mjpg");
}

bool SdStorage::isImageFile(const char* path) {
  if (!path) return false;
  String s = path;
  s.toLowerCase();
  return s.endsWith(".bmp") || s.endsWith(".jpg") || s.endsWith(".jpeg");
}

bool SdStorage::listFiles(const char* dirPath, std::vector<FileItem>& items) {
  items.clear();
  if (!mounted_) return false;

  String path;
  if (!sanitizePath(dirPath, path)) return false;

  File dir = SD.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }

  File f = dir.openNextFile();
  while (f) {
    FileItem item;
    item.name = f.name();
    // Normalize path
    String fullPath = f.path();
    if (!fullPath.startsWith("/sd")) {
      fullPath = "/sd" + fullPath;
    }
    item.path = fullPath;
    item.size = f.size();
    item.isDirectory = f.isDirectory();
    item.isAudio = isAudioFile(f.name());
    item.isVideo = isVideoFile(f.name());
    item.isImage = isImageFile(f.name());

    items.push_back(item);
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  return true;
}

bool SdStorage::exists(const char* path) {
  if (!mounted_) return false;
  String clean;
  if (!sanitizePath(path, clean)) return false;
  return SD.exists(clean.c_str());
}

bool SdStorage::remove(const char* path) {
  if (!mounted_) return false;
  String clean;
  if (!sanitizePath(path, clean)) return false;
  return SD.remove(clean.c_str());
}

bool SdStorage::rename(const char* oldPath, const char* newPath) {
  if (!mounted_) return false;
  String cleanOld, cleanNew;
  if (!sanitizePath(oldPath, cleanOld) || !sanitizePath(newPath, cleanNew)) return false;
  return SD.rename(cleanOld.c_str(), cleanNew.c_str());
}

bool SdStorage::mkdir(const char* dirPath) {
  if (!mounted_) return false;
  String clean;
  if (!sanitizePath(dirPath, clean)) return false;
  if (SD.exists(clean.c_str())) return true;
  return SD.mkdir(clean.c_str());
}

File SdStorage::open(const char* path, const char* mode) {
  if (!mounted_) return File();
  String clean;
  if (!sanitizePath(path, clean)) return File();
  return SD.open(clean.c_str(), mode);
}

}  // namespace storage
