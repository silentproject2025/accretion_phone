// nav.h
// Stack navigasi sederhana. Index 0 SELALU Home. navPush() buka layar app
// baru di atasnya, navBack() balik satu langkah, navGoHome() lompat
// langsung ke Home dari kedalaman manapun. Nama fungsi sengaja dibikin
// mirip navBack()/navGoHome() di file lama biar familiar.
//
// Kenapa perlu ini (bukan cuma manggil show()-fn langsung): stack ini yg
// inget "kalau Back, harus balik ke show()-fn yg mana". Penghapusan layar
// LAMA sendiri ditangani otomatis oleh lv_scr_load_anim(...,auto_del=true)
// di dalam chromeCreateAppScreen()/homeScreenShow() (lihat catatan di
// nav.cpp) -- BUKAN oleh nav.cpp ini lagi.
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
