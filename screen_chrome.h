// screen_chrome.h
// "Kerangka" yg dipakai ULANG oleh SETIAP layar app -- persis spt gimana
// file lama manggil drawStatusBar(s)+judul+drawBack(s) di HAMPIR SEMUA
// fungsi draw*() app. Di sini cukup panggil chromeCreateAppScreen("Judul")
// sekali di awal show()-nya app kalian, taruh konten app sbg child dari
// obj yg dikembalikan.
#pragma once
#include <lvgl.h>

// Bikin layar app baru (lv_scr_load otomatis), lengkap dgn:
//  - background sesuai tema aktif
//  - status bar atas (waktu/wifi/baterai)
//  - judul app (posisi & warna sama spt file lama: accent, di bawah statusbar)
//  - tombol "< Back" pojok kiri-bawah (posisi identik BACK_W/BACK_H lama),
//    nge-trigger navBack() otomatis
// Return: obj layar root -- taruh konten app kalian sbg child dari ini.
lv_obj_t* chromeCreateAppScreen(const char* title);

// Ikat lv_timer ke siklus hidup sebuah layar: timer OTOMATIS terhapus pas
// `scr` di-delete oleh nav.cpp (lv_obj_del_async). WAJIB dipakai kalau
// app kalian bikin lv_timer_create() sendiri di dalam show()-nya -- kalau
// lupa, timer bakal terus nembak ke objek yg udah dihapus = crash.
void chromeBindTimerToScreen(lv_obj_t* scr, lv_timer_t* timer);
