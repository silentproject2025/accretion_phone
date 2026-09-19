#include "display_driver.h"
#include <Preferences.h>
#include <esp_heap_caps.h>

LGFX display;

// Ukuran layar landscape (sama seperti file lama: SCR_W=320, SCR_H=240)
static const int SCR_W = 320, SCR_H = 240;

// Buffer gambar LVGL -- FULL-SCREEN (bukan lagi cuma potongan 40 baris),
// dialokasikan dari PSRAM (project ini udah di-build dgn PSRAM:opi).
// Kenapa: buffer parsial yg lama bikin transisi/animasi yg nutup SELURUH
// layar (mis. fade antar app) kepotong jadi banyak strip flush kecil2 --
// keliatan "tersendat"/gak mulus. Dgn buffer sebesar layar penuh, satu
// frame animasi biasanya cuma butuh 1 kali panggilan flush -- jauh lebih
// mulus. (Single buffer, BUKAN double-buffer -- karena flush_cb kita masih
// blocking [writePixels, nunggu SPI kelar], double-buffer gak ngasih
// manfaat tanpa DMA async, cuma buang PSRAM 2x lipat percuma.)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, w, h);
  // lv_color_t pada LV_COLOR_DEPTH=16 layout-nya RGB565 -- sama persis
  // dengan yang diharapkan pushPixels(). Kalau warna kebalik, toggle
  // LV_COLOR_16_SWAP di lv_conf.h (lihat komentar di file itu).
  display.writePixels((lgfx::rgb565_t*)color_p, w * h);
  display.endWrite();

  lv_disp_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  int32_t x, y;
  bool touched = display.getTouch(&x, &y);
  if (touched) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Disalin dari loadOrRunCalibration() lama (namespace "touch_cal" di NVS
// dipertahankan sama persis, supaya kalibrasi HP kalian yang SUDAH ada
// dari firmware lama tetap kepakai -- tidak perlu kalibrasi ulang).
static void runCalibrationScreen() {
  display.setBrightness(255); // TODO: ganti ke variabel `brightness` tersimpan kalau sudah di-port
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE); display.setTextSize(2);
  display.setCursor(20, 100); display.println("Kalibrasi Touch");
  display.setTextSize(1);
  display.setCursor(20, 130); display.println("Sentuh tanda di setiap sudut");
  uint16_t d[8];
  display.calibrateTouch(d, TFT_WHITE, TFT_BLACK, 15);
  Preferences p; p.begin("touch_cal", false);
  p.putBytes("data", d, sizeof(d));
  p.putBool("done", true);
  p.end();
  display.fillScreen(TFT_BLACK);
}

static void loadOrRunCalibration() {
  Preferences p; p.begin("touch_cal", false);
  bool done = p.getBool("done", false);
  if (done) {
    uint16_t d[8];
    p.getBytes("data", d, sizeof(d));
    display.setTouchCalibrate(d);
    p.end();
  } else {
    p.end();
    runCalibrationScreen();
  }
}

void displayRecalibrateTouch() {
  Preferences p; p.begin("touch_cal", false);
  p.putBool("done", false);
  p.end();
  runCalibrationScreen();
  // Layar kalibrasi nge-freeze UI sementara (blocking, sama spt file lama)
  // -- caller (app Setting) yg tanggung jawab manggil navGoHome()/redraw
  // ulang stlh ini selesai, krn LVGL sempat "diam" selama proses ini jalan.
}

void displayHwInit() {
  display.init();
  display.setRotation(1);   // landscape, sama seperti default lama
  display.setBrightness(0); // biar gak "kedip" nyala penuh sebelum UI siap

  if (display.touch()) loadOrRunCalibration();
}

void lvglGlueInit() {
  buf1 = (lv_color_t*)heap_caps_malloc(SCR_W * SCR_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, SCR_W * SCR_H);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCR_W;
  disp_drv.ver_res = SCR_H;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch_read_cb;
  lv_indev_drv_register(&indev_drv);
}

void lvglLoopTick() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  lv_tick_inc(now - lastMs);
  lastMs = now;
  lv_timer_handler();
}
