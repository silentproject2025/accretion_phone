// app_notepad.h
// Port dari APP: NOTEPAD file lama -- textarea multi-baris + keyboard
// bawaan LVGL (muncul otomatis pas textarea di-tap, sembunyi lagi pas
// selesai/dibatalkan -- persis spt behavior kbVisible lama).
//
// SIMPLIFIKASI sengaja vs file lama: catatan di-AUTOSAVE ke storage tiap
// ada perubahan (dicek tiap 1 detik), BUKAN manual-save + dialog konfirmasi
// "Simpan/Buang" saat keluar. Lebih sederhana & gak ada risiko kehilangan
// perubahan kalau lupa simpan -- tombol Back jadi bisa dipakai langsung
// tanpa perlu logic tahan-navigasi khusus.
#pragma once

void notepadScreenShow();
