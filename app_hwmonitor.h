// app_hwmonitor.h
// Port dari APP: HWMONITOR di file lama -- CPU load per-core (via idle
// hook FreeRTOS, dikalibrasi otomatis saat runtime, sama persis metodenya
// dgn file lama), RAM internal, blok memori kontigu terbesar, PSRAM, info
// chip (freq/suhu/uptime/baterai). Grafik histori pakai widget lv_chart
// bawaan LVGL (mode SHIFT) -- gantiin sparkline manual pixel-per-pixel yg
// lama, hasilnya sama (grafik geser ke kiri), tapi jauh lebih ringkas.
//
// Sengaja BELUM diikutkan di pass ini: tombol "Tes Speaker" (butuh
// I2SClass + pin AUDIO_I2S_* yg belum di-port -- app ini fokus ke
// monitoring dulu, speaker-test bisa nyusul kalau audio udah di-port).
#pragma once

// WAJIB dipanggil SEKALI di setup() (bukan tiap app dibuka) -- daftarin
// idle hook per core, murah & aman jalan terus walau app ini gak pernah
// dibuka user. Sama persis pola file lama.
void hwmonInitHooks();

void hwmonitorScreenShow();
