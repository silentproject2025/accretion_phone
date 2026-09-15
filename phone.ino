// phone.ino (NyxOS -- porting ke LVGL)
//
// Cakupan file ini: init LVGL, boot screen, home screen (grid app + dock),
// subsistem WiFi (non-blocking, sama pola state-machine dgn file lama).
// Sensor gerak MPU6050, file explorer, JPEG/I2S, & sisa game BELUM
// dipindah -- itu jalan berikutnya. Lihat catatan scope di home_screen.h.
//
// Sebelum compile:
//  1. Install library "lvgl@8.3.11" & "LovyanGFX" lewat Library Manager.
//  2. Taruh lv_conf.h (ada di paket ini) di Arduino/libraries/lv_conf.h --
//     SEJAJAR dgn folder Arduino/libraries/lvgl/, BUKAN di dalam sketch.
//  3. Board: ESP32S3 Dev Module, PSRAM: OPI PSRAM (samakan dgn setting lama).
#include <lvgl.h>
#include "display_driver.h"
#include "boot_screen.h"
#include "home_screen.h"
#include "nav.h"
#include "app_hwmonitor.h"
#include "wifi_manager.h"
#include "storage_manager.h"

void setup() {
  Serial.begin(115200);

  displayHwInit();   // display.init() + rotasi + kalibrasi touch (lihat TODO di dalamnya)
  lv_init();
  lvglGlueInit();     // daftarkan flush_cb + touch indev ke LVGL
  hwmonInitHooks();   // idle hook per core -- murah, aman didaftarin dari awal
  storageInit();      // SD_MMC (+fallback FFat) -- dipakai Notepad dkk

  display.setBrightness(255); // nyalakan backlight setelah LVGL siap render

  navInit(homeScreenShow);  // daftarin Home sbg dasar stack navigasi
  bootScreenShow(navGoHome); // boot screen -> lompat ke Home lewat nav (biar screen boot ke-cleanup rapi)

  // WiFi: mulai connect non-blocking DI SINI (setelah backlight nyala),
  // sama persis alasannya dgn file lama -- biar gak ada jeda "layar nyala
  // tp gak bisa disentuh" pas nunggu konek. Progresnya dituntaskan sedikit
  // demi sedikit lewat wifiConnectStep() di loop().
  wifiLoadCreds();
  wifiConnect();
}

void loop() {
  lvglLoopTick();     // lv_tick_inc() + lv_timer_handler()
  wifiConnectStep();  // majuin konek WiFi & sync NTP dikit2, gak pernah nge-block
  delay(5);
}
