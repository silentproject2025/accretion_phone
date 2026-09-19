// app_breakout.h
// Port dari GAME: BREAKOUT file lama -- FIX v20 diikutkan: drag jari SELALU
// jadi kendali utama paddle (bukan cuma kalau MPU6050 gak ada), kemiringan
// HP cuma bantu kalau gak ada sentuhan >150ms terakhir -- biar drag &
// auto-tilt gak saling rebutan bikin paddle gemetar.
#pragma once

void breakoutScreenShow();
