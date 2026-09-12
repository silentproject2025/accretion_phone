// home_screen.h
//
// Scope fondasi ini: grid app (scroll), dock 4 shortcut, status bar,
// header sapaan+jam+tanggal -- SEMUA layout/warna/nama app disalin dari
// drawHome()/apps[]/initAppColors() di file lama.
//
// YANG BELUM ikut di fondasi ini (menyusul di tahap "app per app"):
//  - Ikon vektor custom per app (drawAppIcon() asli gambar garis vektor
//    per app, bukan cuma huruf). Di sini dipakai fallback huruf tunggal
//    (field `sym`) di atas lingkaran warna -- sudah cukup buat navigasi,
//    tinggal diperhalus belakangan.
//  - 26 layar app itu sendiri (Jam, Kalkulator, Snake, dst) -- tap ikon
//    sekarang cuma nampilin toast + print ke Serial sbg placeholder.
//  - Wallpaper custom, status WiFi/baterai asli (masih variabel dummy,
//    lihat TODO di home_screen.cpp) -- tinggal disambungkan ke kode
//    WiFi/battery/NTP kalian yg lama.
#pragma once
#include <cstdint>

struct AppDef {
  const char* name;
  char sym;
  uint16_t color; // RGB565, sama spt initAppColors() lama (tema Dark)
};

#define APP_COUNT 26
extern AppDef apps[APP_COUNT];

// Bangun & tampilkan Home Screen (lv_scr_load).
void homeScreenShow();
