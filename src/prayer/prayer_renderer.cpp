#include "prayer_renderer.h"
#include "../audio/audio_player.h"
#include "../storage/sd_card.h"
#include "../media/file_image.h"
#include "../media/image_renderer.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace prayer {

namespace {

// 5x7 font table
const uint8_t kLetters[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};

const uint8_t kDigits[][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};

// 5x7 proportional font table for uppercase letters
struct Glyph5x7 {
  uint8_t width;
  uint8_t rows[7];
};

const Glyph5x7 kGlyphs5x7[26] = {
  // A (width 4)
  {4, {0b0110, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001}},
  // B (width 4)
  {4, {0b1110, 0b1001, 0b1001, 0b1110, 0b1001, 0b1001, 0b1110}},
  // C (width 4)
  {4, {0b0111, 0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b0111}},
  // D (width 4)
  {4, {0b1110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b1110}},
  // E (width 4)
  {4, {0b1111, 0b1000, 0b1000, 0b1110, 0b1000, 0b1000, 0b1111}},
  // F (width 4)
  {4, {0b1111, 0b1000, 0b1000, 0b1110, 0b1000, 0b1000, 0b1000}},
  // G (width 4)
  {4, {0b0111, 0b1000, 0b1000, 0b1011, 0b1001, 0b1001, 0b0111}},
  // H (width 4)
  {4, {0b1001, 0b1001, 0b1001, 0b1111, 0b1001, 0b1001, 0b1001}},
  // I (width 3)
  {3, {0b111, 0b010, 0b010, 0b010, 0b010, 0b010, 0b111}},
  // J (width 4)
  {4, {0b0001, 0b0001, 0b0001, 0b0001, 0b1001, 0b1001, 0b0110}},
  // K (width 4)
  {4, {0b1001, 0b1010, 0b1100, 0b1100, 0b1010, 0b1001, 0b1001}},
  // L (width 4)
  {4, {0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b1000, 0b1111}},
  // M (width 5)
  {5, {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
  // N (width 4)
  {4, {0b1001, 0b1101, 0b1101, 0b1011, 0b1011, 0b1001, 0b1001}},
  // O (width 4)
  {4, {0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}},
  // P (width 4)
  {4, {0b1110, 0b1001, 0b1001, 0b1110, 0b1000, 0b1000, 0b1000}},
  // Q (width 4)
  {4, {0b0110, 0b1001, 0b1001, 0b1001, 0b1011, 0b1001, 0b0111}},
  // R (width 4)
  {4, {0b1110, 0b1001, 0b1001, 0b1110, 0b1100, 0b1010, 0b1001}},
  // S (width 4)
  {4, {0b0111, 0b1000, 0b1000, 0b0110, 0b0001, 0b0001, 0b1110}},
  // T (width 4)
  {4, {0b1111, 0b0110, 0b0110, 0b0110, 0b0110, 0b0110, 0b0110}},
  // U (width 4)
  {4, {0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110}},
  // V (width 4)
  {4, {0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110, 0b0110}},
  // W (width 5)
  {5, {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
  // X (width 4)
  {4, {0b1001, 0b1001, 0b0110, 0b0110, 0b0110, 0b1001, 0b1001}},
  // Y (width 4)
  {4, {0b1001, 0b1001, 0b1001, 0b0110, 0b0110, 0b0110, 0b0110}},
  // Z (width 4)
  {4, {0b1111, 0b0001, 0b0010, 0b0100, 0b1000, 0b1000, 0b1111}}
};

// 3x5 font for compact indicators (countdown labels, stacked minutes & seconds)
struct Glyph3x5 {
  uint8_t width;
  uint8_t rows[5];
};

const Glyph3x5 kDigits3x5[10] = {
  {3, {0b111, 0b101, 0b101, 0b101, 0b111}}, // 0
  {3, {0b010, 0b110, 0b010, 0b010, 0b111}}, // 1
  {3, {0b111, 0b001, 0b111, 0b100, 0b111}}, // 2
  {3, {0b111, 0b001, 0b111, 0b001, 0b111}}, // 3
  {3, {0b101, 0b101, 0b111, 0b001, 0b001}}, // 4
  {3, {0b111, 0b100, 0b111, 0b001, 0b111}}, // 5
  {3, {0b111, 0b100, 0b111, 0b101, 0b111}}, // 6
  {3, {0b111, 0b001, 0b010, 0b010, 0b010}}, // 7
  {3, {0b111, 0b101, 0b111, 0b101, 0b111}}, // 8
  {3, {0b111, 0b101, 0b111, 0b001, 0b111}}  // 9
};

const Glyph3x5 kLetters3x5[26] = {
  {3, {0b010, 0b101, 0b111, 0b101, 0b101}}, // A
  {3, {0b110, 0b101, 0b110, 0b101, 0b110}}, // B
  {3, {0b111, 0b100, 0b100, 0b100, 0b111}}, // C
  {3, {0b110, 0b101, 0b101, 0b101, 0b110}}, // D
  {3, {0b111, 0b100, 0b110, 0b100, 0b111}}, // E
  {3, {0b111, 0b100, 0b110, 0b100, 0b100}}, // F
  {3, {0b111, 0b100, 0b101, 0b101, 0b111}}, // G
  {3, {0b101, 0b101, 0b111, 0b101, 0b101}}, // H
  {3, {0b111, 0b010, 0b010, 0b010, 0b111}}, // I
  {3, {0b001, 0b001, 0b001, 0b101, 0b010}}, // J
  {3, {0b101, 0b110, 0b100, 0b110, 0b101}}, // K
  {3, {0b100, 0b100, 0b100, 0b100, 0b111}}, // L
  {5, {0b10001, 0b11011, 0b10101, 0b10001, 0b10001}}, // M
  {3, {0b110, 0b101, 0b101, 0b101, 0b101}}, // N
  {3, {0b111, 0b101, 0b101, 0b101, 0b111}}, // O
  {3, {0b111, 0b101, 0b111, 0b100, 0b100}}, // P
  {3, {0b111, 0b101, 0b101, 0b111, 0b001}}, // Q
  {3, {0b110, 0b101, 0b110, 0b101, 0b101}}, // R
  {3, {0b111, 0b100, 0b111, 0b001, 0b111}}, // S
  {3, {0b111, 0b010, 0b010, 0b010, 0b010}}, // T
  {3, {0b101, 0b101, 0b101, 0b101, 0b111}}, // U
  {3, {0b101, 0b101, 0b101, 0b101, 0b010}}, // V
  {5, {0b10001, 0b10001, 0b10101, 0b10101, 0b01010}}, // W
  {3, {0b101, 0b101, 0b010, 0b101, 0b101}}, // X
  {3, {0b101, 0b101, 0b010, 0b010, 0b010}}, // Y
  {3, {0b111, 0b001, 0b010, 0b100, 0b111}}  // Z
};

void drawChar3x5(display::Framebuffer& canvas, int x, int y, char c, uint16_t color) {
  const Glyph3x5* g = nullptr;
  if (c >= '0' && c <= '9') {
    g = &kDigits3x5[c - '0'];
  } else if (c >= 'A' && c <= 'Z') {
    g = &kLetters3x5[c - 'A'];
  } else if (c >= 'a' && c <= 'z') {
    g = &kLetters3x5[c - 'a'];
  } else if (c == ':') {
    static const Glyph3x5 gColon = {1, {0, 1, 0, 1, 0}};
    g = &gColon;
  } else if (c == '-') {
    static const Glyph3x5 gDash = {3, {0, 0, 7, 0, 0}};
    g = &gDash;
  } else {
    return;
  }

  for (int row = 0; row < 5; ++row) {
    uint8_t bits = g->rows[row];
    for (int col = 0; col < g->width; ++col) {
      if ((bits >> (g->width - 1 - col)) & 1) {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < canvas.width() && py >= 0 && py < canvas.height()) {
          canvas.setPixel(px, py, color);
        }
      }
    }
  }
}

int drawString3x5(display::Framebuffer& canvas, int x, int y, const char* str, uint16_t color, int spacing = 1) {
  if (!str) return 0;
  int curX = x;
  while (*str) {
    char c = *str;
    if (c == ' ') {
      curX += 2 + spacing;
    } else {
      drawChar3x5(canvas, curX, y, c, color);
      uint8_t w = 3;
      if (c >= '0' && c <= '9') w = kDigits3x5[c - '0'].width;
      else if (c >= 'A' && c <= 'Z') w = kLetters3x5[c - 'A'].width;
      else if (c >= 'a' && c <= 'z') w = kLetters3x5[c - 'a'].width;
      else if (c == ':') w = 1;
      curX += w + spacing;
    }
    str++;
  }
  return curX - x;
}

int measureString5x7(const char* str) {
  if (!str) return 0;
  int w = 0;
  int count = 0;
  while (*str) {
    char c = *str;
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') {
      w += kGlyphs5x7[c - 'A'].width;
      count++;
    } else if (c == ' ') {
      w += 2;
      count++;
    }
    str++;
  }
  if (count > 1) {
    w += (count - 1);
  }
  return w;
}

void drawString5x7(display::Framebuffer& canvas, int x, int y, const char* str, uint16_t color,
                   int clipX1, int clipY1, int clipX2, int clipY2) {
  if (!str) return;
  int curX = x;
  while (*str) {
    char c = *str;
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') {
      const auto& g = kGlyphs5x7[c - 'A'];
      for (int row = 0; row < 7; ++row) {
        uint8_t bits = g.rows[row];
        for (int col = 0; col < g.width; ++col) {
          if ((bits >> (g.width - 1 - col)) & 1) {
            int px = curX + col;
            int py = y + row;
            if (px >= clipX1 && px <= clipX2 && py >= clipY1 && py <= clipY2) {
              canvas.setPixel(px, py, color);
            }
          }
        }
      }
      curX += g.width + 1;
    } else if (c == ' ') {
      curX += 3;
    }
    str++;
  }
}

uint8_t glyphRow(char c, unsigned row) {
  if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (c >= 'A' && c <= 'Z') return kLetters[c - 'A'][row];
  if (c >= '0' && c <= '9') return kDigits[c - '0'][row];
  switch (c) {
    case ' ': return 0;
    case ':': return (row == 2 || row == 5) ? 4 : 0;
    case '.': return row == 6 ? 4 : 0;
    case ',': return row == 5 ? 4 : row == 6 ? 8 : 0;
    case '-': return row == 3 ? 14 : 0;
    case '_': return row == 6 ? 31 : 0;
    case '/': return static_cast<uint8_t>(1U << std::min(4U, row * 5 / 7));
    case '*': return (row == 1 || row == 5) ? 10 : (row == 2 || row == 4) ? 4 : (row == 3) ? 31 : 0;
    case '!': return (row <= 4) ? 4 : (row == 6 ? 4 : 0);
    case '?': return (row == 0) ? 14 : (row == 1 ? 17 : (row == 2 ? 1 : (row == 3 ? 2 : (row == 4 ? 4 : (row == 6 ? 4 : 0)))));
    case '(': return (row == 0 || row == 6) ? 2 : ((row >= 1 && row <= 5) ? 4 : 0);
    case ')': return (row == 0 || row == 6) ? 4 : ((row >= 1 && row <= 5) ? 2 : 0);
    case '\'': return (row <= 1) ? 4 : (row == 2 ? 2 : 0);
    case '"': return (row <= 1) ? 10 : (row == 2 ? 5 : 0);
    case '+': return (row == 3) ? 31 : ((row == 2 || row == 4) ? 4 : 0);
    case (char)0xB7: return (row == 3) ? 4 : 0; // Middle dot ·
    default: return 0;
  }
}

void drawCharClipped(display::Framebuffer& canvas, int x, int y, char c, uint16_t color, uint8_t scale,
                     int clipX1, int clipY1, int clipX2, int clipY2) {
  for (unsigned row = 0; row < 7; ++row) {
    uint8_t bits = glyphRow(c, row);
    for (unsigned col = 0; col < 5; ++col) {
      if (bits & (16 >> col)) {
        for (uint8_t sy = 0; sy < scale; ++sy) {
          for (uint8_t sx = 0; sx < scale; ++sx) {
            int px = x + col * scale + sx;
            int py = y + row * scale + sy;
            if (px >= clipX1 && px <= clipX2 && py >= clipY1 && py <= clipY2) {
              canvas.setPixel(px, py, color);
            }
          }
        }
      }
    }
  }
}

int measureMarqueeWidth(const char* str, uint8_t scale, int spacing) {
  if (!str) return 0;
  int count = 0;
  while (*str) {
    if (static_cast<uint8_t>(*str) == 0xC2 && static_cast<uint8_t>(*(str + 1)) == 0xB7) {
      str++;
    }
    count++;
    str++;
  }
  if (count == 0) return 0;
  return count * (5 * scale) + (count - 1) * spacing;
}

void drawStringClipped(display::Framebuffer& canvas, int x, int y, const char* str, uint16_t color, uint8_t scale, int spacing,
                       int clipX1, int clipY1, int clipX2, int clipY2) {
  if (!str) return;
  int curX = x;
  const int charW = 5 * scale;
  while (*str) {
    char c = *str;
    if (static_cast<uint8_t>(c) == 0xC2 && static_cast<uint8_t>(*(str + 1)) == 0xB7) {
      c = static_cast<char>(0xB7);
      str++;
    }
    if (curX + charW >= clipX1 && curX <= clipX2) {
      drawCharClipped(canvas, curX, y, c, color, scale, clipX1, clipY1, clipX2, clipY2);
    }
    curX += charW + spacing;
    str++;
  }
}

void fillRectClipped(display::Framebuffer& canvas, int x, int y, int rw, int rh, uint16_t color,
                     int clipX1, int clipY1, int clipX2, int clipY2) {
  int startY = std::max(y, clipY1);
  int endY = std::min(y + rh - 1, clipY2);
  int startX = std::max(x, clipX1);
  int endX = std::min(x + rw - 1, clipX2);
  for (int py = startY; py <= endY; ++py) {
    for (int px = startX; px <= endX; ++px) {
      canvas.setPixel(px, py, color);
    }
  }
}

void drawString(display::Framebuffer& canvas, int x, int y, const char* str, uint16_t color, uint8_t scale = 1, int spacing = 1) {
  drawStringClipped(canvas, x, y, str, color, scale, spacing, 0, 0, canvas.width() - 1, canvas.height() - 1);
}

void fillRect(display::Framebuffer& canvas, int x, int y, int rw, int rh, uint16_t color) {
  fillRectClipped(canvas, x, y, rw, rh, color, 0, 0, canvas.width() - 1, canvas.height() - 1);
}

void drawRect(display::Framebuffer& canvas, int x, int y, int rw, int rh, uint16_t color) {
  fillRect(canvas, x, y, rw, 1, color);
  fillRect(canvas, x, y + rh - 1, rw, 1, color);
  fillRect(canvas, x, y, 1, rh, color);
  fillRect(canvas, x + rw - 1, y, 1, rh, color);
}

struct PrayerBatchItem {
  const char* name;
  PrayerTimePair time;
  bool isCurrent;
};

void draw4PrayerBatch(display::Framebuffer& canvas, int xOffset, const PrayerBatchItem items[4],
                      uint16_t nameColNormal, uint16_t timeColNormal, uint16_t highlightCol,
                      uint16_t cardBgNormal, uint16_t cardBgNext) {
  constexpr int clipX1 = 0;
  constexpr int clipX2 = 127;
  constexpr int clipY1 = 41;
  constexpr int clipY2 = 62;

  for (int i = 0; i < 4; ++i) {
    int colX = xOffset + i * 32;
    const auto& item = items[i];

    // Card background (30px wide, 22px high, leaving 1px border on each side)
    uint16_t cardBg = item.isCurrent ? cardBgNext : cardBgNormal;
    if (cardBg != 0x0000) {
      fillRectClipped(canvas, colX + 1, 41, 30, 22, cardBg, clipX1, clipY1, clipX2, clipY2);
    }

    // Top & Bottom accent lines
    uint16_t borderCol = item.isCurrent ? highlightCol : 0x1D39;
    fillRectClipped(canvas, colX + 1, 41, 30, 1, borderCol, clipX1, clipY1, clipX2, clipY2);
    fillRectClipped(canvas, colX + 1, 62, 30, 1, borderCol, clipX1, clipY1, clipX2, clipY2);

    // Vertical Divider line between columns
    if (i > 0) {
      fillRectClipped(canvas, colX, 41, 1, 22, 0x1D39, clipX1, clipY1, clipX2, clipY2);
    }

    // Row 1 (y = 43..49): Full prayer name centered (5x7 font)
    int nameW = measureString5x7(item.name);
    int nameX = colX + (32 - nameW) / 2;
    uint16_t nameCol = item.isCurrent ? highlightCol : nameColNormal;
    drawString5x7(canvas, nameX, 43, item.name, nameCol, clipX1, clipY1, clipX2, clipY2);

    // Row 2 (y = 53..59): Prayer time centered (5x7 font)
    char timeBuf[8];
    snprintf(timeBuf, sizeof timeBuf, "%02d:%02d", item.time.hour, item.time.minute);
    int timeX = colX + 2; // 29px wide, centered in 32px
    uint16_t timeCol = item.isCurrent ? highlightCol : timeColNormal;
    drawStringClipped(canvas, timeX, 53, timeBuf, timeCol, 1, 1, clipX1, clipY1, clipX2, clipY2);
  }
}

void* allocPsramOrHeap(size_t bytes) {
  void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = malloc(bytes);
  return ptr;
}

void freePsramOrHeap(void* ptr) {
  if (ptr) heap_caps_free(ptr);
}

class PrayerFileReader : public media::FileReader {
 public:
  explicit PrayerFileReader(fs::File& file) : file_(file) {}
  size_t size() const override { return file_.size(); }
  size_t read(uint8_t* output, size_t count) override { return file_.read(output, count); }
 private:
  fs::File& file_;
};

} // namespace

PrayerRenderer::PrayerRenderer() = default;

PrayerRenderer& PrayerRenderer::instance() {
  static PrayerRenderer s_instance;
  return s_instance;
}

uint32_t PrayerRenderer::refreshInterval() const {
  const auto sched = prayerSchedule().currentStatus();
  if (sched.mosqueState != MOSQUE_STATE_NORMAL) {
    return 100U; // Fast 10 FPS for countdown and visual alerts
  }
  if (middleMode_ == 1) {
    const int spd = prayerSchedule().config().marqueeSpeed;
    if (spd == 1) return 42U; // Lambat (~23.8 px/s)
    if (spd == 3) return 18U; // Cepat (~55.5 px/s)
    return 26U;              // Sedang (~38.5 px/s)
  }
  if (inTransition_) return 20U;
  return 100U;
}

bool PrayerRenderer::render(display::Framebuffer& canvas) {
  const int width = canvas.width();
  const int height = canvas.height();
  if (width < 64 || height < 32) return false;

  const auto sched = prayerSchedule().currentStatus();

  // If in dedicated Mosque Flow State (Adzan, Iqomah, Shalat), render dedicated screens!
  if (sched.mosqueState == MOSQUE_STATE_ADZAN) {
    renderAdzan(canvas, sched);
    canvas.present();
    return true;
  }
  if (sched.mosqueState == MOSQUE_STATE_IQOMAH) {
    renderIqomah(canvas, sched);
    canvas.present();
    return true;
  }
  if (sched.mosqueState == MOSQUE_STATE_SHALAT) {
    renderShalat(canvas, sched);
    canvas.present();
    return true;
  }

  // Clear background to True Black (all LEDs off, zero purple/pink bleed)
  const uint16_t bg = 0x0000;
  canvas.clear(bg);

  const auto clk = clockService().status();
  const uint32_t now = millis();

  // Color Palette (RGB565)
  constexpr uint16_t cWhite   = 0xFFFF;
  constexpr uint16_t cCyan    = 0x07FF;
  constexpr uint16_t cGold    = 0xFEA0;
  constexpr uint16_t cGreen   = 0x2FE7;
  constexpr uint16_t cRed     = 0xF800;

  // =========================================================================
  // 1. TOP SECTION (y: 1 to 17): Big Clock & Alternating Calendar
  // =========================================================================
  // Digital Clock: 87px wide (x: 2..88, y: 2..15)
  char timeStr[16];
  snprintf(timeStr, sizeof timeStr, "%02d:%02d:%02d", clk.hour, clk.minute, clk.second);
  drawString(canvas, 2, 2, timeStr, cCyan, 2, 1);

  // Vertical separator at x=91 (balanced 2px gap from clock and calendar)
  fillRect(canvas, 91, 2, 1, 15, 0x1D39);

  // Date cycle: Alternating Masehi and Hijriah every 4 seconds
  if (now - lastCycleChange_ >= 4000U) {
    lastCycleChange_ = now;
    dateCycle_ = (dateCycle_ + 1) % 2;
  }

  char dateL1[12];
  char dateL2[12];
  uint16_t dateCol = cGold;

  if (dateCycle_ == 0) {
    // Masehi date e.g. "24/10", "2026"
    snprintf(dateL1, sizeof dateL1, "%02d/%02d", clk.day, clk.month);
    snprintf(dateL2, sizeof dateL2, "%04d", clk.year);
    dateCol = cWhite;
  } else {
    // Hijri date e.g. "12/05", "1448H"
    HijriDate h = gregorianToHijri(clk.year, clk.month, clk.day, prayerSchedule().config().hijriAdjustment);
    snprintf(dateL1, sizeof dateL1, "%02d/%02d", h.day, h.month);
    snprintf(dateL2, sizeof dateL2, "%04dH", h.year);
    dateCol = cGold;
  }
  drawString(canvas, 94, 2, dateL1, dateCol, 1);
  drawString(canvas, 94, 10, dateL2, dateCol, 1);

  // Top Divider line
  fillRect(canvas, 0, 18, width, 1, 0x2145);

  // =========================================================================
  // 2. MIDDLE SECTION (y: 19 to 39): Alternating Next Prayer & Marquee Text
  // =========================================================================
  fillRect(canvas, 1, 19, width - 2, 21, 0x0000);
  fillRect(canvas, 1, 19, width - 2, 1, 0x2248);
  fillRect(canvas, 1, 39, width - 2, 1, 0x2248);

  const auto& pcfg = prayerSchedule().config();

  if (audio::player().isPlaying()) {
    // Audio Reminder is currently playing
    fillRect(canvas, 1, 19, width - 2, 21, 0x2108);
    drawString(canvas, 6, 22, "PENGINGAT SUARA AKTIF", cRed, 1);
    drawString(canvas, 6, 30, "MEMUTAR AUDIO ADZAN...", cWhite, 1);
  } else {
    const bool marqueeActive = pcfg.marqueeEnabled && (pcfg.marqueeText.length() > 0);

    if (middleMode_ == 0) {
      if (marqueeActive && (now - middleModeChangeTime_ >= static_cast<uint32_t>(std::max(3, pcfg.marqueeHoldSecs)) * 1000U)) {
        middleMode_ = 1;
        middleModeChangeTime_ = now;
        marqueeInitialized_ = false;
      }
    } else {
      if (!marqueeActive) {
        middleMode_ = 0;
        middleModeChangeTime_ = now;
      }
    }

    if (middleMode_ == 1 && marqueeActive) {
      // ---------------------------------------------------------------------
      // VIEW B: Marquee Running Text Message (di-marquee ke sebelah kanan/kiri)
      // ---------------------------------------------------------------------
      const uint8_t scale = std::max<uint8_t>(1, std::min<uint8_t>(2, pcfg.marqueeFontScale));
      const int textW = measureMarqueeWidth(pcfg.marqueeText.c_str(), scale, 1);
      const bool scrollRight = (pcfg.marqueeDirection == "right");

      if (!marqueeInitialized_) {
        marqueeInitialized_ = true;
        marqueeX_ = scrollRight ? -textW : width;
      } else {
        // Step EXACTLY 1 pixel per frame for silky-smooth motion without micro-stutter
        if (scrollRight) {
          marqueeX_++;
          if (marqueeX_ >= width) {
            middleMode_ = 0;
            middleModeChangeTime_ = now;
          }
        } else {
          marqueeX_--;
          if (marqueeX_ <= -textW) {
            middleMode_ = 0;
            middleModeChangeTime_ = now;
          }
        }
      }

      // Safety timeout: 60 detik maks per putaran marquee
      if (now - middleModeChangeTime_ >= 60000U) {
        middleMode_ = 0;
        middleModeChangeTime_ = now;
      }

      const int textY = (scale == 1) ? 26 : 22;
      const uint16_t marqueeCol = hexToRgb565(pcfg.marqueeColor, cGold);
      drawStringClipped(canvas, marqueeX_, textY, pcfg.marqueeText.c_str(), marqueeCol, scale, 1, 1, 20, width - 2, 38);

    } else {
      // ---------------------------------------------------------------------
      // VIEW A: Next Prayer & Countdown
      // ---------------------------------------------------------------------
      // Vertical divider between left and right column at x=68
      fillRect(canvas, 68, 20, 1, 19, 0x1D39);

      // Left Column (x: 2..67): Next Prayer full name and time
      drawString(canvas, 2, 22, "BERIKUTNYA:", 0x9CD3, 1);

      drawString5x7(canvas, 2, 30, sched.nextPrayerName.c_str(), cGold, 0, 0, 67, 38);
      int nameW = measureString5x7(sched.nextPrayerName.c_str());
      char nextTimeBuf[8];
      snprintf(nextTimeBuf, sizeof nextTimeBuf, "%02d:%02d", sched.nextPrayerTime.hour, sched.nextPrayerTime.minute);
      drawString(canvas, 2 + nameW + 3, 30, nextTimeBuf, cGold, 1);

      // Right Column (x: 70..127): Countdown in user image layout
      // Line 1: SISA (y=22..26)
      // Line 2: WAKTU (y=30..34)
      // Big Hours (y=21..34)
      // Colon (y=24, y=31)
      // Stacked Minutes (y=22..26) & Seconds (y=30..34)
      drawString3x5(canvas, 70, 22, "SISA", 0x9CD3);
      drawString3x5(canvas, 70, 30, "WAKTU", 0x9CD3);

      int cd = std::max(0, sched.countdownSeconds);
      int cdH = std::min(99, cd / 3600);
      int cdM = (cd % 3600) / 60;
      int cdS = cd % 60;

      uint16_t cdColor = (cd <= 600) ? ((now / 500 % 2 == 0) ? cRed : cGold) : cGreen;

      // Big Hours (2 digits, scale 2: width 21px, height 14px, x: 93..113, y: 21..34)
      char hBuf[4];
      snprintf(hBuf, sizeof hBuf, "%02d", cdH);
      drawString(canvas, 93, 21, hBuf, cdColor, 2, 1);

      // Colon dots (2x2 square dots at x: 116..117, aligned with MM and SS)
      fillRect(canvas, 116, 24, 2, 2, cdColor);
      fillRect(canvas, 116, 31, 2, 2, cdColor);

      // Stacked Minutes and Seconds (3x5 digits: width 7px, height 5px, x: 119..125)
      char mBuf[4];
      snprintf(mBuf, sizeof mBuf, "%02d", cdM);
      drawString3x5(canvas, 119, 22, mBuf, cdColor);

      char sBuf[4];
      snprintf(sBuf, sizeof sBuf, "%02d", cdS);
      drawString3x5(canvas, 119, 30, sBuf, cdColor);
    }
  }

  // Middle Divider line
  fillRect(canvas, 0, 40, width, 1, 0x2145);

  // =========================================================================
  // 3. BOTTOM SECTION (y: 41 to 62): 4-Prayer Batch (Alternating & Animated)
  // Batch 0: IMSAK, SHUBUH, TERBIT, DUHA
  // Batch 1: DZUHUR, ASHAR, MAGRIB, ISYA
  // =========================================================================
  // Determine which prayer schedule is CURRENTLY ACTIVE right now
  const int nowMins = clk.hour * 60 + clk.minute;
  const int tImsak = sched.imsak.totalMinutes();
  const int tSubuh = sched.effective[PRAYER_SUBUH].totalMinutes();
  const int tTerbit = sched.syuruq.totalMinutes();
  const int tDuha = sched.duha.totalMinutes();
  const int tDzuhur = sched.effective[PRAYER_ZUHUR].totalMinutes();
  const int tAshar = sched.effective[PRAYER_ASAR].totalMinutes();
  const int tMagrib = sched.effective[PRAYER_MAGRIB].totalMinutes();
  const int tIsya = sched.effective[PRAYER_ISYA].totalMinutes();

  const bool isCurImsak  = (nowMins >= tImsak && nowMins < tSubuh);
  const bool isCurSubuh  = (nowMins >= tSubuh && nowMins < tTerbit);
  const bool isCurTerbit = (nowMins >= tTerbit && nowMins < tDuha);
  const bool isCurDuha   = (nowMins >= tDuha && nowMins < tDzuhur);
  const bool isCurDzuhur = (nowMins >= tDzuhur && nowMins < tAshar);
  const bool isCurAshar  = (nowMins >= tAshar && nowMins < tMagrib);
  const bool isCurMagrib = (nowMins >= tMagrib && nowMins < tIsya);
  const bool isCurIsya   = (nowMins >= tIsya || nowMins < tImsak);

  PrayerBatchItem batch0[4] = {
    { "IMSAK",  sched.imsak, isCurImsak },
    { "SHUBUH", sched.effective[PRAYER_SUBUH], isCurSubuh },
    { "TERBIT", sched.syuruq, isCurTerbit },
    { "DUHA",   sched.duha, isCurDuha }
  };

  PrayerBatchItem batch1[4] = {
    { "DZUHUR", sched.effective[PRAYER_ZUHUR],  isCurDzuhur },
    { "ASHAR",  sched.effective[PRAYER_ASAR],   isCurAshar },
    { "MAGRIB", sched.effective[PRAYER_MAGRIB], isCurMagrib },
    { "ISYA",   sched.effective[PRAYER_ISYA],   isCurIsya }
  };

  // Switch between Batch 0 and Batch 1 every 4.5 seconds
  constexpr uint32_t kBatchHoldMs = 4500U;
  constexpr uint32_t kBatchTransitionMs = 400U;

  if (now - lastBatchSwitch_ >= kBatchHoldMs) {
    lastBatchSwitch_ = now;
    batchPrevious_ = batchIndex_;
    batchIndex_ = (batchIndex_ == 0) ? 1 : 0;
    transitionStart_ = now;
    inTransition_ = true;
  }

  // Read display colors configured from Web UI
  const uint16_t nameColNormal = hexToRgb565(pcfg.nameColor, cCyan);
  const uint16_t timeColNormal = hexToRgb565(pcfg.timeColor, cWhite);
  const uint16_t highlightCol  = hexToRgb565(pcfg.highlightColor, cGold);
  const uint16_t cardBgNormal  = hexToRgb565(pcfg.cardBgColor, 0x0000);
  const uint16_t cardBgNext    = hexToRgb565(pcfg.nextBgColor, 0x0814);

  if (inTransition_) {
    uint32_t elapsed = now - transitionStart_;
    if (elapsed >= kBatchTransitionMs) {
      inTransition_ = false;
      draw4PrayerBatch(canvas, 0, (batchIndex_ == 0) ? batch0 : batch1, nameColNormal, timeColNormal, highlightCol, cardBgNormal, cardBgNext);
    } else {
      float progress = static_cast<float>(elapsed) / static_cast<float>(kBatchTransitionMs);
      // Smooth cubic ease-out
      progress = 1.0f - (1.0f - progress) * (1.0f - progress);
      int offset = static_cast<int>((1.0f - progress) * 128.0f);

      // Slide outgoing batch to left
      draw4PrayerBatch(canvas, -(128 - offset), (batchPrevious_ == 0) ? batch0 : batch1, nameColNormal, timeColNormal, highlightCol, cardBgNormal, cardBgNext);
      // Slide incoming batch in from right
      draw4PrayerBatch(canvas, offset, (batchIndex_ == 0) ? batch0 : batch1, nameColNormal, timeColNormal, highlightCol, cardBgNormal, cardBgNext);
    }
  } else {
    draw4PrayerBatch(canvas, 0, (batchIndex_ == 0) ? batch0 : batch1, nameColNormal, timeColNormal, highlightCol, cardBgNormal, cardBgNext);
  }

  canvas.present();
  return true;
}

bool PrayerRenderer::tryRenderCustomBackground(display::Framebuffer& canvas, const String& imagePath) {
  if (imagePath.length() == 0 || !media::FileImage::validPath(imagePath.c_str())) {
    return false;
  }

  static String s_lastPath = "";
  static uint32_t s_lastCheck = 0;
  static bool s_lastExists = false;
  const uint32_t now = millis();

  if (imagePath != s_lastPath || (now - s_lastCheck >= 5000U)) {
    s_lastPath = imagePath;
    s_lastCheck = now;
    const bool isSd = imagePath.startsWith("/sd");
    if (isSd) {
      s_lastExists = storage::sd().isMounted() && storage::sd().exists(imagePath.c_str());
    } else {
      s_lastExists = LittleFS.exists(imagePath.c_str());
    }
  }

  if (!s_lastExists) {
    return false;
  }

  fs::File file;
  const bool isSd = imagePath.startsWith("/sd");
  if (isSd) {
    file = storage::sd().open(imagePath.c_str(), "r");
  } else {
    file = LittleFS.open(imagePath.c_str(), "r");
  }
  if (!file || file.isDirectory()) return false;

  PrayerFileReader reader(file);
  media::FileImage img(allocPsramOrHeap, freePsramOrHeap);
  const char* error = nullptr;
  if (!img.load(imagePath.c_str(), reader, error)) {
    file.close();
    return false;
  }
  file.close();

  media::ImageRenderOptions options;
  options.width = canvas.width();
  options.height = canvas.height();
  options.mode = media::ScaleMode::Stretch;
  if (!media::renderImage(img.source(), canvas, options, error)) {
    return false;
  }
  return true;
}

void PrayerRenderer::renderAdzan(display::Framebuffer& canvas, const PrayerStatus& sched) {
  const int width = canvas.width();
  const int height = canvas.height();
  const uint32_t now = millis();

  constexpr uint16_t cWhite   = 0xFFFF;
  constexpr uint16_t cCyan    = 0x07FF;
  constexpr uint16_t cGold    = 0xFEA0;
  constexpr uint16_t cGreen   = 0x2FE7;
  constexpr uint16_t cRed     = 0xF800;

  const auto& pcfg = prayerSchedule().config();
  bool customBg = tryRenderCustomBackground(canvas, pcfg.adzanImage);

  if (!customBg) {
    canvas.clear(0x0000);
    // Built-in Vector Mosque & Muadzin Calling Adzan Scene on left side (x: 0..46, y: 0..63)
    // 1. Twilight sky gradient
    for (int y = 0; y < 64; ++y) {
      uint16_t skyCol = (y < 22) ? 0x0008 : (y < 44 ? 0x0810 : 0x0826);
      fillRect(canvas, 0, y, 46, 1, skyCol);
    }

    // 2. Stars & Crescent Moon
    if ((now / 400) % 2 == 0) {
      canvas.setPixel(4, 5, cWhite);
      canvas.setPixel(28, 6, cWhite);
    } else {
      canvas.setPixel(4, 6, 0x8410);
      canvas.setPixel(28, 5, 0x8410);
    }
    canvas.setPixel(16, 3, cWhite);
    canvas.setPixel(36, 10, cWhite);

    // Crescent Moon (x: 32..37, y: 3..9)
    fillRect(canvas, 33, 4, 3, 5, cGold);
    canvas.setPixel(32, 5, cGold);
    canvas.setPixel(32, 7, cGold);
    canvas.setPixel(36, 4, cGold);
    canvas.setPixel(36, 8, cGold);
    fillRect(canvas, 34, 5, 3, 3, 0x0008);

    // 3. Mosque Minaret Tower (x: 5..15, y: 14..63)
    fillRect(canvas, 10, 4, 1, 3, cGold);
    canvas.setPixel(10, 3, cGold);
    for (int r = 0; r < 7; ++r) {
      int rw = 1 + r * 2;
      int rx = 10 - r;
      fillRect(canvas, rx, 7 + r, rw, 1, 0x18E5);
    }
    // Balcony with railing
    fillRect(canvas, 2, 17, 17, 2, 0x2147);
    for (int bx = 3; bx <= 17; bx += 2) {
      canvas.setPixel(bx, 15, 0x39E9);
      canvas.setPixel(bx, 16, 0x39E9);
    }
    // Minaret Shaft
    fillRect(canvas, 6, 19, 9, 45, 0x10C4);
    fillRect(canvas, 5, 19, 1, 45, 0x18E5);
    fillRect(canvas, 9, 28, 3, 6, 0x0000);
    canvas.setPixel(10, 27, 0x0000);

    // 4. Muadzin Silhouette standing on the minaret balcony
    // Head & White Turban (y: 8..11, x: 10..12)
    fillRect(canvas, 10, 8, 3, 3, 0xFFFF);
    canvas.setPixel(11, 8, cGold);
    canvas.setPixel(12, 10, 0xFEA0); // Face
    // Arm raised to ear (Adzan calling gesture)
    fillRect(canvas, 13, 9, 2, 2, 0xFFFF);
    canvas.setPixel(14, 11, 0xFFFF);
    canvas.setPixel(9, 11, 0xFFFF);
    // White Robe (Thobe) Body
    fillRect(canvas, 9, 11, 5, 6, 0xDEFB);
    fillRect(canvas, 10, 11, 3, 6, 0xFFFF);

    // 5. Sound waves radiating from Muadzin
    uint16_t waveCol = ((now / 300) % 2 == 0) ? cGold : cCyan;
    canvas.setPixel(17, 9, waveCol);
    canvas.setPixel(18, 10, waveCol);
    canvas.setPixel(17, 11, waveCol);
    canvas.setPixel(21, 8, waveCol);
    canvas.setPixel(22, 10, waveCol);
    canvas.setPixel(21, 12, waveCol);
    if ((now / 300) % 3 >= 1) {
      canvas.setPixel(25, 7, waveCol);
      canvas.setPixel(26, 10, waveCol);
      canvas.setPixel(25, 13, waveCol);
    }

    // 6. Mosque Dome Silhouette on right of minaret
    fillRect(canvas, 20, 36, 25, 28, 0x0926);
    fillRect(canvas, 24, 33, 17, 3, 0x0926);
    fillRect(canvas, 28, 31, 9, 2, 0x0926);
    fillRect(canvas, 31, 29, 3, 2, 0x0926);
    canvas.setPixel(32, 27, cGold);
    canvas.setPixel(32, 28, cGold);
    canvas.setPixel(31, 26, cGold);
    canvas.setPixel(33, 26, cGold);
  }

  // Foreground Overlay Box for Adzan Text
  int cardX = customBg ? 10 : 47;
  int cardW = customBg ? 108 : 79;

  fillRect(canvas, cardX, 3, cardW, 58, 0x0842);
  drawRect(canvas, cardX, 3, cardW, 58, cGold);
  fillRect(canvas, cardX + 1, 4, cardW - 2, 1, cCyan);

  // Badge: [ WAKTU ADZAN ]
  int badgeW = 62;
  int badgeX = cardX + (cardW - badgeW) / 2;
  fillRect(canvas, badgeX, 6, badgeW, 9, 0x1904);
  drawRect(canvas, badgeX, 6, badgeW, 9, ((now / 500) % 2 == 0) ? cGold : cGreen);
  drawString3x5(canvas, badgeX + 5, 8, "WAKTU ADZAN", cWhite);

  // Line: "SAATNYA ADZAN"
  const char* pName = sched.activePrayerName.length() > 0 ? sched.activePrayerName.c_str() : "SHALAT";
  int pW = measureString5x7(pName);
  drawString5x7(canvas, cardX + 6, 18, "SAATNYA ADZAN", cCyan, 0, 0, 127, 63);

  // Big Prayer Name in Scale 2 (Gold!)
  int nameX = cardX + (cardW - (pW * 2)) / 2;
  if (nameX < cardX + 4) nameX = cardX + 4;
  drawString(canvas, nameX, 28, pName, cGold, 2, 1);

  // Countdown & Sisa Adzan
  int rem = std::max(0, sched.stateRemainingSeconds);
  int remM = rem / 60;
  int remS = rem % 60;
  char remBuf[16];
  snprintf(remBuf, sizeof remBuf, "SISA %02d:%02d", remM, remS);
  drawString3x5(canvas, cardX + 6, 45, remBuf, cWhite);

  // Progress Bar at bottom (y: 53..56)
  int barTotalW = cardW - 12;
  int barFillW = (sched.stateTotalSeconds > 0) ? (rem * barTotalW / sched.stateTotalSeconds) : 0;
  fillRect(canvas, cardX + 6, 53, barTotalW, 4, 0x18C3);
  if (barFillW > 0) {
    fillRect(canvas, cardX + 6, 53, barFillW, 4, cGreen);
  }
}

void PrayerRenderer::renderIqomah(display::Framebuffer& canvas, const PrayerStatus& sched) {
  const int width = canvas.width();
  const int height = canvas.height();
  const uint32_t now = millis();

  constexpr uint16_t cWhite   = 0xFFFF;
  constexpr uint16_t cCyan    = 0x07FF;
  constexpr uint16_t cGold    = 0xFEA0;
  constexpr uint16_t cGreen   = 0x2FE7;
  constexpr uint16_t cRed     = 0xF800;

  const auto& pcfg = prayerSchedule().config();
  bool customBg = tryRenderCustomBackground(canvas, pcfg.iqomahImage);

  if (!customBg) {
    canvas.clear(0x0000);
    // Built-in Islamic Mihrab Arch Frame
    drawRect(canvas, 0, 0, 128, 64, 0x12A4);
    // Left & Right Pillars
    fillRect(canvas, 2, 12, 3, 50, 0x18E5);
    fillRect(canvas, 123, 12, 3, 50, 0x18E5);
    // Pillar capitals
    fillRect(canvas, 1, 10, 5, 2, cGold);
    fillRect(canvas, 122, 10, 5, 2, cGold);
    // Pointed Moorish arch curve
    for (int ax = 6; ax <= 63; ++ax) {
      int ay = 10 - (ax - 6) * 8 / 57;
      if (ay < 2) ay = 2;
      canvas.setPixel(ax, ay, cGold);
      canvas.setPixel(127 - ax, ay, cGold);
    }
    // Hanging Mosque Lamp (Fanous) at center (x: 62..65, y: 2..10)
    canvas.setPixel(63, 3, cGold);
    canvas.setPixel(64, 3, cGold);
    canvas.setPixel(63, 4, cGold);
    canvas.setPixel(64, 4, cGold);
    fillRect(canvas, 62, 5, 4, 4, 0xFFE0);
    canvas.setPixel(63, 9, cGold);
    canvas.setPixel(64, 9, cGold);
  } else {
    // Shaded backdrop over custom image
    fillRect(canvas, 8, 3, 112, 58, 0x0822);
    drawRect(canvas, 8, 3, 112, 58, cGold);
  }

  // 1. Header (y: 4..13)
  const char* pName = sched.activePrayerName.length() > 0 ? sched.activePrayerName.c_str() : "SHALAT";
  char hdrBuf[32];
  snprintf(hdrBuf, sizeof hdrBuf, "IQOMAH %s", pName);
  int hdrW = measureString5x7(hdrBuf);
  int hdrX = (128 - hdrW) / 2;
  drawString5x7(canvas, hdrX, 4, hdrBuf, cGold, 0, 0, 127, 63);

  // Separator line at y: 14
  fillRect(canvas, 14, 14, 100, 1, 0x2248);

  // 2. Center Massive Countdown Timer (y: 18..38)
  int rem = std::max(0, sched.stateRemainingSeconds);
  int remM = rem / 60;
  int remS = rem % 60;

  uint16_t timeCol;
  if (rem > 60) {
    timeCol = cGreen;
  } else if (rem > 10) {
    timeCol = cGold;
  } else {
    // Urgency alert: Blinking vivid red & white!
    timeCol = ((now / 250) % 2 == 0) ? cRed : cWhite;
  }

  char timerBuf[12];
  snprintf(timerBuf, sizeof timerBuf, "%02d:%02d", remM, remS);
  int tX = (128 - 79) / 2; // = 24
  drawString(canvas, tX, 18, timerBuf, timeCol, 3, 1);

  // 3. Progress Bar (y: 42..45)
  constexpr int barX = 14;
  constexpr int barW = 100;
  fillRect(canvas, barX, 42, barW, 3, 0x18C3);
  if (sched.stateTotalSeconds > 0) {
    int fillW = (rem * barW) / sched.stateTotalSeconds;
    if (fillW > 0) {
      fillRect(canvas, barX, 42, fillW, 3, timeCol);
    }
  }

  // 4. Footer Message (y: 49..60): Alternating reminder every 3 seconds
  int msgCycle = (now / 3000) % 2;
  const char* msg = (msgCycle == 0) ? "LURUS & RAPATKAN SHAF" : "MATIKAN SUARA HP";
  uint16_t msgCol = (msgCycle == 0) ? cWhite : cCyan;
  int msgW = measureString5x7(msg);
  int msgX = (128 - msgW) / 2;
  drawString5x7(canvas, msgX, 50, msg, msgCol, 0, 0, 127, 63);
}

void PrayerRenderer::renderShalat(display::Framebuffer& canvas, const PrayerStatus& sched) {
  const int width = canvas.width();
  const int height = canvas.height();
  const uint32_t now = millis();

  const auto& pcfg = prayerSchedule().config();

  // If dim screen during shalat is enabled, check elapsed time
  int elapsed = sched.stateTotalSeconds - sched.stateRemainingSeconds;
  if (pcfg.dimScreenDuringShalat && elapsed >= 30) {
    // Screen is dimmed / off during shalat to prevent distraction
    canvas.clear(0x0000);
    // Keep a subtle dim indicator dot in the corner
    if ((now / 2000) % 2 == 0) {
      canvas.setPixel(1, 1, 0x0821);
    }
    return;
  }

  constexpr uint16_t cWhite   = 0xFFFF;
  constexpr uint16_t cCyan    = 0x07FF;
  constexpr uint16_t cGold    = 0xFEA0;
  constexpr uint16_t cGreen   = 0x2FE7;
  constexpr uint16_t cRed     = 0xF800;

  bool customBg = tryRenderCustomBackground(canvas, pcfg.shalatImage);

  if (!customBg) {
    canvas.clear(0x0000);
    drawRect(canvas, 0, 0, 128, 64, 0x1A45);
    // Solemn congregation silhouette at bottom
    for (int px = 8; px < 120; px += 14) {
      fillRect(canvas, px + 2, 54, 8, 8, 0x18E5);
      fillRect(canvas, px + 4, 50, 4, 4, 0x2167);
    }
  } else {
    // Shaded backdrop over custom image
    fillRect(canvas, 6, 2, 116, 60, 0x0822);
    drawRect(canvas, 6, 2, 116, 60, cGold);
  }

  // Header Badge: [ WAKTU SHALAT ]
  int badgeW = 76;
  int badgeX = (128 - badgeW) / 2;
  fillRect(canvas, badgeX, 4, badgeW, 9, 0x1904);
  drawRect(canvas, badgeX, 4, badgeW, 9, cGold);
  drawString3x5(canvas, badgeX + 6, 6, "SAATNYA SHALAT", cWhite);

  // Big Prayer Name: e.g. DZUHUR
  const char* pName = sched.activePrayerName.length() > 0 ? sched.activePrayerName.c_str() : "SHALAT";
  int pW = measureString5x7(pName);
  int nameX = (128 - (pW * 2)) / 2;
  if (nameX < 4) nameX = 4;
  drawString(canvas, nameX, 17, pName, cGold, 2, 1);

  // Divider
  fillRect(canvas, 16, 34, 96, 1, cCyan);

  // Reminder 1: LURUS & RAPATKAN SHAF
  const char* r1 = "LURUS & RAPATKAN SHAF";
  int r1W = measureString5x7(r1);
  int r1X = (128 - r1W) / 2;
  drawString5x7(canvas, r1X, 38, r1, cWhite, 0, 0, 127, 63);

  // Reminder 2: MOHON NONAKTIFKAN HP
  const char* r2 = "MOHON NONAKTIFKAN HP";
  int r2W = measureString5x7(r2);
  int r2X = (128 - r2W) / 2;
  uint16_t hpCol = ((now / 500) % 2 == 0) ? cRed : cGold;
  drawString5x7(canvas, r2X, 48, r2, hpCol, 0, 0, 127, 63);
}

}  // namespace prayer
