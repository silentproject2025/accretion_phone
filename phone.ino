// phone.ino (NyxOS -- fondasi porting ke LVGL)
//
// FONDASI porting NyxOS dari custom-draw LovyanGFX ke LVGL v8.3.x.
// Cakupan file ini: init LVGL, boot screen, home screen (grid app + dock).
// Logic non-UI (WiFi, sensor MPU6050, file explorer, AI chat, semua 26
// app/game) BELUM dipindah -- itu jalan berikutnya, satu per satu, di atas
// fondasi ini. Lihat catatan scope di home_screen.h.
//
// Sebelum compile:
//  1. Install library "lvgl" (v8.3.x) & "LovyanGFX" lewat Library Manager.
//  2. Taruh lv_conf.h (ada di paket ini) di Arduino/libraries/lv_conf.h --
//     SEJAJAR dgn folder Arduino/libraries/lvgl/, BUKAN di dalam sketch.
//  3. Board: ESP32S3 Dev Module, PSRAM: OPI PSRAM (samakan dgn setting lama).
#include <lvgl.h>
#include "display_driver.h"
#include "boot_screen.h"
#include "home_screen.h"

void setup() {
  Serial.begin(115200);

  displayHwInit();   // display.init() + rotasi + kalibrasi touch (lihat TODO di dalamnya)
  lv_init();
  lvglGlueInit();     // daftarkan flush_cb + touch indev ke LVGL

  display.setBrightness(255); // nyalakan backlight setelah LVGL siap render

  bootScreenShow(homeScreenShow); // boot screen -> otomatis lanjut ke Home
}

void loop() {
  lvglLoopTick(); // lv_tick_inc() + lv_timer_handler()
  delay(5);
}
