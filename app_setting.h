// app_setting.h
// Versi RINGKAS dari APP: SETTINGS file lama -- sengaja BELUM mengikutkan:
//  - Input SSID/Password + scan/connect WiFi (butuh WiFi.h + virtual
//    keyboard, dua-duanya belum di-port)
//  - Kalibrasi sensor gerak MPU6050 (sensornya sendiri belum di-port)
//  - Pilihan font UI (uiFontIdx/uiFontList -- custom font loader belum
//    di-port)
// Yang SUDAH jalan penuh di sini: slider Kecerahan (langsung ngatur
// backlight asli via display.setBrightness), pemilih Tema (4 tema,
// langsung kepakai begitu app lain dibuka lagi), toggle Auto-Rotate
// (state tersimpan, TODO logic rotasi otomatisnya nyusul bareng
// Orientasi3D/MPU6050), & tombol Kalibrasi Ulang touch (fungsional penuh,
// pakai displayRecalibrateTouch() yg udah ada).
#pragma once

void settingScreenShow();
