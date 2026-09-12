/*
 * lv_conf.h — konfigurasi minimal LVGL v8.3.x untuk NyxOS di ESP32-S3.
 *
 * PENTING soal LOKASI FILE INI (ini gotcha paling umum di integrasi
 * LVGL+Arduino): file ini TIDAK ditaruh di dalam folder sketch.
 * Taruh persis di:
 *   Arduino/libraries/lv_conf.h
 * yaitu SEJAJAR (satu level) dengan folder:
 *   Arduino/libraries/lvgl/
 * Kalau ditaruh di tempat lain, LVGL akan build pakai default config-nya
 * sendiri dan sebagian besar setting di bawah ini tidak akan kepakai.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH     16
// Kalau nanti warna kebalik-balik (merah<->biru) pas tampil di layar,
// coba toggle baris di bawah ini ke 1 -- itu gotcha umum kedua soal
// endianness RGB565 antara buffer LVGL & panel ILI9341.
#define LV_COLOR_16_SWAP   0

#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (48U * 1024U)   // heap internal utk objek LVGL

#define LV_DISP_DEF_REFR_PERIOD  30        // ms
#define LV_INDEV_DEF_READ_PERIOD 30        // ms

#define LV_TICK_CUSTOM     0   // kita panggil lv_tick_inc() manual di loop()

#define LV_USE_FLEX        1
#define LV_USE_GRID        1

#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_28  1
#define LV_THEME_DEFAULT_INIT       lv_theme_default_init
#define LV_THEME_DEFAULT_DARK       1

#define LV_USE_LOG         1
#define LV_LOG_LEVEL       LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF      1

// Widget yang dipakai fondasi ini (hemat flash: matikan yang tak perlu)
#define LV_USE_LABEL       1
#define LV_USE_BTN         1
#define LV_USE_IMG         1
#define LV_USE_BAR         1
#define LV_USE_ANIMIMG     0

#endif // LV_CONF_H
