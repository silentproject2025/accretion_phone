// app_files.h
// Port dari APP: FILE EXPLORER file lama -- daftar file (SD Card, fallback
// FFat), buka (liat isi, teks), hapus. Web uploader (server HTTP upload
// lewat browser) SENGAJA belum diikutkan -- itu subsistem terpisah (HTTP
// server + form upload), di luar scope port file explorer dasar ini.
//
// Simplifikasi: daftar file pakai scroll bawaan LVGL (bukan lagi
// pagination "< Prev/Next >" manual), & isi file yg ditampilkan dibatasi
// ~3000 karakter pertama (dikasih catatan kalau lebih panjang) -- jaga2
// biar gak ada file gede bikin RAM label kepenuhan.
#pragma once

void filesScreenShow();
