// app_labirin.h
// Port dari GAME: LABIRIN (v56-v70) file lama -- kejar-kejaran ala Pac-Man.
// Kontrol GABUNGAN: swipe jari ATAU miringkan HP (MPU6050, kalau kedetek),
// termasuk fix v70 (tilt cuma "nembak" arah SEKALI pas baru lewat ambang,
// bukan terus2an selama masih miring -- biar gak nimpa swipe user).
// Skor terbaik & level tertinggi tersimpan permanen (NVS), warna karakter
// "kebuka" makin bagus makin tinggi level tertinggi yg PERNAH dicapai.
#pragma once

void labirinScreenShow();
