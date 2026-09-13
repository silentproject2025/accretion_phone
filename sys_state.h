// sys_state.h
// Variabel status sistem yg BELUM di-port (WiFi/NTP/battery ASLI masih di
// file lama, belum dipindah). SEMUA screen baca dari sini -- satu sumber
// kebenaran, jadi pas kalian port kode WiFi/NTP/battery asli nanti, cukup
// update dari SATU tempat (atau ganti implementasi di sys_state.cpp jadi
// baca sensor/WiFi beneran), gak perlu ubah tiap app satu-satu.
#pragma once

extern bool g_ntpSynced;      // TODO: set true stlh sinkron NTP asli berhasil
extern bool g_wifiConnected;  // TODO: set dari WiFi.status()==WL_CONNECTED
extern int  g_battPercent;    // TODO: isi dari pembacaan ADC baterai asli
