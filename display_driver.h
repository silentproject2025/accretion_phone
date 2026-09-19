// display_driver.h
//
// Class LGFX di bawah ini di-COPY PERSIS dari file .ino lama (pin, bus SPI,
// panel ILI9341, backlight, touch XPT2046) -- TIDAK ada yang diubah, supaya
// kalibrasi & wiring yang sudah kalian benerin lama (bug lock screen swipe,
// dll) tetap kepakai apa adanya. LVGL cuma "numpang" di atas driver ini
// lewat callback flush (gambar) & callback indev (baca sentuhan) --
// dia sama sekali tidak butuh driver low-level sendiri.
#pragma once
#include <LovyanGFX.hpp>
#include <lvgl.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;
public:
  LGFX(void) {
    { auto cfg = _bus_instance.config();
      cfg.spi_host=SPI2_HOST; cfg.spi_mode=0;
      cfg.freq_write=40000000; cfg.freq_read=16000000;
      cfg.spi_3wire=false; cfg.use_lock=true;
      cfg.dma_channel=SPI_DMA_CH_AUTO;
      cfg.pin_sclk=12; cfg.pin_mosi=11;
      cfg.pin_miso=13; cfg.pin_dc=2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance); }
    { auto cfg = _panel_instance.config();
      cfg.pin_cs=10; cfg.pin_rst=14; cfg.pin_busy=-1;
      cfg.memory_width=240; cfg.memory_height=320;
      cfg.panel_width=240;  cfg.panel_height=320;
      cfg.offset_x=0; cfg.offset_y=0; cfg.offset_rotation=0;
      cfg.dummy_read_pixel=8; cfg.dummy_read_bits=1;
      cfg.readable=true; cfg.invert=false;
      cfg.rgb_order=false; cfg.dlen_16bit=false;
      cfg.bus_shared=false;
      _panel_instance.config(cfg); }
    { auto cfg = _light_instance.config();
      cfg.pin_bl=21; cfg.invert=false;
      cfg.freq=44100; cfg.pwm_channel=7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance); }
    { auto cfg = _touch_instance.config();
      cfg.pin_int=-1; cfg.bus_shared=false;
      cfg.offset_rotation=0; cfg.spi_host=SPI3_HOST;
      cfg.freq=2000000;
      cfg.pin_sclk=6; cfg.pin_mosi=5;
      cfg.pin_miso=4; cfg.pin_cs=9;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance); }
    setPanel(&_panel_instance);
  }
};

extern LGFX display;

// Init urutan: display.init() + rotasi landscape + kalibrasi touch (baca
// data tersimpan di NVS lewat loadOrRunCalibration(), sama persis pola
// file lama -- lihat display_driver.cpp).
void displayHwInit();

// Hapus data kalibrasi tersimpan & jalankan ulang calibrateTouch() dari
// awal (blocking, freeze UI sebentar). Dipanggil tombol "Kalibrasi Ulang"
// di app Setting.
void displayRecalibrateTouch();

// Daftarkan display + touch ke LVGL. Panggil SETELAH displayHwInit()
// dan SETELAH lv_init().
void lvglGlueInit();

// Panggil tiap iterasi loop(): lv_tick_inc() + lv_timer_handler().
void lvglLoopTick();
