// sys_state.h
// Variabel status sistem SATU sumber kebenaran, dibaca semua screen.
// g_ntpSynced & g_wifiConnected sekarang REAL -- diupdate langsung oleh
// wifi_manager.cpp (bukan stub lagi). g_battPercent/g_battVoltage masih
// stub (ADC baterai belum di-port).
#pragma once

extern bool  g_ntpSynced;      // diupdate oleh wifi_manager.cpp (wifiConnectStep())
extern bool  g_wifiConnected;  // diupdate oleh wifi_manager.cpp (wifiConnectStep())
extern int   g_battPercent;    // TODO: isi dari pembacaan ADC baterai asli
extern float g_battVoltage;    // TODO: isi dari pembacaan ADC baterai asli (skrg default 3.7V)
extern bool  g_autoRotate;     // toggle di app Setting; logic rotasi-otomatisnya sendiri nyusul bareng MPU6050

// Dipanggil tombol "Refresh" di app Baterai. Sekarang no-op (belum ada
// ADC/voltage-divider yg di-port) -- pas kode battUpdate() asli (baca
// analogRead(BATT_ADC_PIN)) dipindah, isi fungsi ini dgn logic itu &
// update g_battVoltage/g_battPercent, tiap app yg makai otomatis ikut update.
void sysUpdateBattery();
