// app_neopixel.h
// Port dari npxApp* di file lama -- kontrol LED RGB (GPIO 48 di board asli).
// UI + state (ON/OFF, warna, mode, kecerahan) full jalan; OUTPUT ke LED fisik
// masih TODO (butuh tambah library Adafruit_NeoPixel/FastLED ke proyek +
// port npxStep()/pin GPIO dari file lama -- lihat npxApplyColor() di .cpp
// ini). Sbg gantinya, ada swatch preview di layar biar tetep keliatan warna
// yg lagi "aktif" walau LED fisik blm nyala.
#pragma once

void neopixelScreenShow();
