// app_2048.h
// Port dari GAME: 2048 di file lama -- logika papan/gabung/skor disalin
// PERSIS. Deteksi swipe pakai gesture bawaan LVGL (lv_indev_get_gesture_dir)
// gantiin tracking drag-delta manual yg lama -- lebih idiomatik & gak perlu
// state drag sendiri.
#pragma once

void game2048ScreenShow();
