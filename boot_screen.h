// boot_screen.h
//
// CATATAN PENTING soal scope: ini BUKAN porting 1:1 dari runBootSequence()
// lama. Boot sequence lama itu simulasi partikel per-pixel yang nge-converge
// bentuk huruf "NYXOS" (BootTextPoint/BootParticle, dieksekusi manual tiap
// frame di atas LGFX_Sprite). Itu gaya "immediate-mode" murni -- ngoding
// ulang persis di LVGL (retained-mode, widget tree) itu proyek TERSENDIRI
// (jalannya lewat lv_canvas + digambar manual tiap frame, bukan lewat
// widget/animasi bawaan LVGL) dan sengaja TIDAK termasuk di fondasi ini.
//
// Versi di bawah ini ganti pendekatan: logo+tagline muncul dengan fade+scale
// pakai lv_anim bawaan LVGL (native, ringan, idiomatik di LVGL), tetap pakai
// palet warna & tagline yang sama ("Beyond the Event Horizon"). Kalau nanti
// kalian mau versi partikel yang identik, bilang aja -- itu bisa disusulkan
// sebagai modul terpisah (bootParticleCanvas) tanpa ganggu fondasi ini.
#pragma once

// Tampilkan boot screen, lalu panggil onDone() otomatis setelah animasi
// selesai (dipanggil sekali, dari dalam LVGL timer).
void bootScreenShow(void (*onDone)());
