// mpu_sensor.h
// Port dari blok "SENSOR: MPU6050" di file lama -- akses register I2C
// LANGSUNG lewat Wire.h (bukan pakai library Adafruit_MPU6050 dkk), jadi
// GAK PERLU nambah library baru ke proyek. Wiring sama persis: SDA=GPIO15,
// SCL=GPIO7, alamat I2C=0x68.
//
// Diikutkan di pass ini: init, baca mentah+offset kalibrasi, kalibrasi
// (tersimpan permanen di NVS), smoothing roll/pitch, shake-to-home.
// BELUM diikutkan: auto-rotate SUNGGUHAN (flip layar portrait<->landscape)
// -- itu perubahan arsitektur lebih besar (seluruh layout LVGL project ini
// asumsi landscape 320x240 tetap), jadi toggle "Rotasi" di Setting utk
// SEKARANG cuma nyimpen preferensi on/off-nya doang, belum benar2 muter
// layar. Bisa disusulkan kalau dibutuhkan.
#pragma once
#include <Arduino.h>

extern bool  g_mpuReady;
extern float g_mpuAx, g_mpuAy, g_mpuAz;   // percepatan (g), sudah dikoreksi offset
extern float g_mpuGx, g_mpuGy, g_mpuGz;   // kec. sudut (dps), sudah dikoreksi offset
extern float g_mpuTempC;
extern float g_smoothRoll, g_smoothPitch; // sudut kemiringan halus (derajat)

// Panggil SEKALI di setup() -- true kalau sensor kedetek.
bool mpuInit();

// Panggil tiap loop() -- baca sensor, update smoothing roll/pitch, &
// jalankan shake-to-home. No-op otomatis kalau sensor gak kedetek.
void mpuUpdate();

// Kalibrasi BLOCKING (~1 detik, HP harus diam & rata) -- sama persis pola
// file lama. Hasil tersimpan permanen ke NVS.
void mpuCalibrate();

// true kalau shake-to-home lagi diaktifkan (dipakai app yg mau nonaktifin
// sementara, mis. Canvas/Update, sama spt file lama).
extern bool g_shakeEnabled;
