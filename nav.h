// nav.h
// Stack navigasi sederhana. Index 0 SELALU Home. navPush() buka layar app
// baru di atasnya, navBack() balik satu langkah, navGoHome() lompat
// langsung ke Home dari kedalaman manapun. Nama fungsi sengaja dibikin
// mirip navBack()/navGoHome() di file lama biar familiar.
//
// Kenapa perlu ini (bukan cuma lv_scr_load manual di tiap app): supaya
// screen LAMA otomatis di-delete stlh screen BARU tampil (lv_obj_del_async)
// -- kalau lupa dihapus manual, tiap kali pindah layar bakal numpuk objek
// & timer nyangkut di memori sampai akhirnya crash/out-of-memory.
#pragma once

typedef void (*ScreenShowFn)();

// Panggil SEKALI di setup(), daftarin fungsi show() punya Home.
void navInit(ScreenShowFn homeShowFn);

// Buka layar app baru (dipanggil dari tap ikon app di Home).
void navPush(ScreenShowFn showFn);

// Balik satu langkah (dipanggil dari tombol "< Back" tiap app screen).
void navBack();

// Lompat langsung ke Home, reset stack (dipakai boot screen & tombol Home).
void navGoHome();
