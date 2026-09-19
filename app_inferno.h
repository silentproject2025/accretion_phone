// app_inferno.h
// Port dari GAME: INFERNO (v74-v77) file lama -- FPS raycaster gaya
// Wolfenstein (algoritma DDA, TANPA sqrt di loop kolom, sama persis
// pendekatannya). Peta 18x16, kontrol joystick-kiri (jalan, analog, jangkar
// tetap -- fix v75) + drag-look-kanan (noleh, basis direset tiap frame) +
// tilt-assist MPU6050 (gated timeout 150ms drag-look, BUKAN drag-joystick --
// biar bisa jalan+noleh-tilt BARENGAN, akal2in touchscreen resistif yg cuma
// baca 1 titik), tombol Tembak (hitscan lurus + toleransi kecil, cek
// z-buffer).
//
// SIMPLIFIKASI vs file lama (biar tetap jalan mulus di ESP32 -- Inferno itu
// fitur PALING berat di seluruh proyek ini, v74-v77 nambah banyak lapisan
// kosmetik di atas engine intinya):
//  - Musuh: badan+kepala sederhana (rounded-rect + lingkaran), BUKAN sosok
//    humanoid berlengan/berkaki v77 yg proporsional
//  - Skip: partikel abu/bara melayang, obor menyala di pilar, screen shake,
//    pantulan kilatan tembakan ke dinding
//  - Occlusion musuh vs dinding disederhanakan: cek 1 titik tengah sprite
//    thd z-buffer (bukan nyusutin visL/visR per kolom tepi)
// Yang TETAP PERSIS: algoritma DDA int-nya sendiri, rumus gerak/noleh/
// tembak, peta, tekstur prosedural dinding (mortar/rivet/hazard-stripe/
// organic-pulse), shading sisi + kabut jarak, FPS meter (v75).
#pragma once

void infernoScreenShow();
