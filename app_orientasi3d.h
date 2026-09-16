// app_orientasi3d.h
// Port dari APP: ORIENTASI 3D file lama -- kotak wireframe 3D yg berputar
// sesuai kemiringan HP (dari MPU6050), + angka roll/pitch/accel/gyro/suhu.
// Gambar wireframe-nya pakai lv_canvas (gambar manual per-frame, pas buat
// visual yg gak match widget bawaan LVGL manapun).
//
// SIMPLIFIKASI vs file lama: wajah "layar" HP (2 segitiga terisi warna)
// di-skip -- lv_canvas v8 gak punya fill-polygon/triangle bawaan, cuma
// line/rect/arc/text. Rangka wireframe 12 rusuk + 8 titik sudut tetap
// digambar penuh, jadi bentuk kotaknya tetap jelas kebaca cuma gak solid.
#pragma once

void orientasi3dScreenShow();
