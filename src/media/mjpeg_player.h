#pragma once
#include <Arduino.h>
#include <cstring>
#include <LittleFS.h>
#include "storage/sd_card.h"
#include "display/framebuffer.h"
#include "media/jpeg.h"
#include "media/image_renderer.h"

namespace media {

class MjpegPlayer {
 public:
  using Allocate = void* (*)(size_t);
  using Release = void (*)(void*);

  MjpegPlayer(Allocate allocate, Release release)
      : allocate_(allocate), release_(release), decoder_(allocate, release) {}

  ~MjpegPlayer() {
    close();
  }

  bool open(const char* path, uint8_t fps = 15) {
    close();
    if (!path || !path[0]) return false;
    if (strncmp(path, "/sd", 3) == 0) {
      file_ = storage::sd().open(path, "r");
    } else {
      file_ = LittleFS.open(path, "r");
    }
    if (!file_ || file_.isDirectory()) {
      if (file_) file_.close();
      return false;
    }
    if (file_.size() < 10) {
      file_.close();
      return false;
    }
    frameBuf_ = static_cast<uint8_t*>(allocate_(kMaxFrameSize));
    if (!frameBuf_) {
      file_.close();
      return false;
    }
    path_ = path;
    fps_ = (fps >= 1 && fps <= 30) ? fps : 15;
    playing_ = true;
    frameCount_ = 0;
    lastFrameAt_ = 0;
    return true;
  }

  void close() {
    playing_ = false;
    if (file_) file_.close();
    if (frameBuf_ && release_) {
      release_(frameBuf_);
      frameBuf_ = nullptr;
    }
    path_ = "";
    frameCount_ = 0;
  }

  bool isPlaying() const {
    return playing_ && file_;
  }

  const String& currentPath() const {
    return path_;
  }

  uint8_t fps() const {
    return fps_;
  }

  void setFps(uint8_t fps) {
    if (fps >= 1 && fps <= 30) fps_ = fps;
  }

  uint32_t frameCount() const {
    return frameCount_;
  }

  bool renderNextFrame(display::Framebuffer& canvas) {
    if (!playing_ || !file_ || !frameBuf_) return false;

    // Search for SOI marker 0xFF 0xD8
    int prev = -1;
    bool foundSoi = false;
    size_t searchBytes = 0;
    while (file_.available() && !foundSoi && searchBytes < 32768) {
      int b = file_.read();
      ++searchBytes;
      if (prev == 0xFF && b == 0xD8) {
        foundSoi = true;
        break;
      }
      prev = b;
    }

    if (!foundSoi) {
      // Loop to beginning of file
      file_.seek(0);
      prev = -1;
      searchBytes = 0;
      while (file_.available() && !foundSoi && searchBytes < 32768) {
        int b = file_.read();
        ++searchBytes;
        if (prev == 0xFF && b == 0xD8) {
          foundSoi = true;
          break;
        }
        prev = b;
      }
      if (!foundSoi) return false;
    }

    // Accumulate JPEG payload until EOI marker 0xFF 0xD9
    frameBuf_[0] = 0xFF;
    frameBuf_[1] = 0xD8;
    size_t len = 2;
    bool foundEoi = false;
    prev = -1;

    while (file_.available() && !foundEoi && len < kMaxFrameSize) {
      int b = file_.read();
      frameBuf_[len++] = static_cast<uint8_t>(b);
      if (prev == 0xFF && b == 0xD9) {
        foundEoi = true;
        break;
      }
      prev = b;
    }

    if (!foundEoi) {
      // Reached EOF before finding EOI, loop around
      file_.seek(0);
      return false;
    }

    const char* err = nullptr;
    if (decoder_.open(frameBuf_, len, err)) {
      media::ImageRenderOptions opts;
      opts.width = canvas.width();
      opts.height = canvas.height();
      opts.mode = media::ScaleMode::Fit;
      if (media::renderImage(decoder_.source(), canvas, opts, err)) {
        canvas.present();
        ++frameCount_;
        lastFrameAt_ = millis();
        return true;
      }
    }
    return false;
  }

  static bool isMjpegPath(const char* path) {
    if (!path) return false;
    const char* ext = strrchr(path, '.');
    if (!ext) return false;
    return (!strcasecmp(ext, ".mjpeg") || !strcasecmp(ext, ".mjpg"));
  }

 private:
  static constexpr size_t kMaxFrameSize = 64U * 1024U;
  Allocate allocate_;
  Release release_;
  JpegImage decoder_;
  fs::File file_;
  uint8_t* frameBuf_ = nullptr;
  String path_;
  uint8_t fps_ = 15;
  bool playing_ = false;
  uint32_t frameCount_ = 0;
  uint32_t lastFrameAt_ = 0;
};

}  // namespace media
