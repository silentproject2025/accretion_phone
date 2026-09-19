// theme.h
// Palet tema di-copy 1:1 dari file .ino lama (struct Theme + array themes[]),
// supaya tampilan NyxOS versi LVGL tetap konsisten dengan versi lama.
#pragma once
#include <lvgl.h>
#include <cstdint>

struct Theme {
  const char* name;
  uint16_t bg, surface, surface2, accent, accent2,
           text, subtext, divider, good, danger;
};

#define THEME_COUNT 4
extern Theme themes[THEME_COUNT];
extern int themeIdx;
Theme& T();

// LVGL v8 (LV_COLOR_DEPTH=16) internally masih pakai lv_color_t sbg struct;
// paling aman & portable across depth: rekonstruksi dari komponen 5-6-5.
static inline lv_color_t c565(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (c & 0x1F) * 255 / 31;
  return lv_color_make(r, g, b);
}
