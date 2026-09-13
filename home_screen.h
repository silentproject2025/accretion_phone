// home_screen.h
//
// Scope fondasi ini: grid app (scroll), dock 4 shortcut, status bar,
// header sapaan+jam+tanggal -- SEMUA layout/warna/nama app disalin dari
// drawHome()/apps[]/initAppColors() di file lama.
//
// Ikon: dipakai LV_SYMBOL_* bawaan LVGL kalau ada yg cocok secara makna
// (mis. Setting->gear, Files->folder). Simbol bawaan LVGL cuma ~40 ikon
// UI generik (bukan icon-set buat app/game spesifik), jadi utk app yg
// gak ada padanannya (semua game, Astronomi, HWmonitor, dll) tetap pakai
// fallback huruf tunggal di lingkaran warna -- bukan bug, memang gak ada
// simbol LVGL yg representatif buat "Snake" atau "Inferno".
//
// Navigasi: field `onOpen` (nullable). Kalau ke-isi (app udah di-port),
// tap ikon manggil navPush(onOpen). Kalau masih nullptr, tap ikon cuma
// nampilin toast placeholder spt sebelumnya.
#pragma once
#include <cstdint>

struct AppDef {
  const char* name;
  char sym;                 // fallback huruf, dipakai kalau iconSymbol==nullptr
  const char* iconSymbol;   // LV_SYMBOL_xxx, nullptr = gak ada padanan yg cocok
  uint16_t color;           // RGB565, sama spt initAppColors() lama (tema Dark)
  void (*onOpen)();         // nullptr = belum di-port (toast placeholder)
};

#define APP_COUNT 26
extern AppDef apps[APP_COUNT];

// Bangun & tampilkan Home Screen (lv_scr_load).
void homeScreenShow();
