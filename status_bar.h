// status_bar.h
// Status bar atas (waktu kiri, wifi+baterai kanan) -- versi reusable, dulu
// nempel di dalam home_screen.cpp, sekarang dipakai bareng oleh Home DAN
// semua layar app (persis spt drawStatusBar(s) yg dipanggil di HAMPIR
// SEMUA fungsi draw*() app di file lama). Nilai wifi/baterai/ntp diambil
// dari sys_state.h (lihat TODO di sana).
#pragma once
#include <lvgl.h>

// Bikin status bar sbg child dari `parent` (biasanya root screen obj).
// Nempel timer 1 detik yg OTOMATIS kehapus sendiri pas `parent` di-delete
// (lewat chromeBindTimerToScreen secara internal) -- jadi aman dipanggil
// di app screen manapun tanpa takut timer nyangkut/crash pas pindah layar.
void statusBarCreate(lv_obj_t* parent);
