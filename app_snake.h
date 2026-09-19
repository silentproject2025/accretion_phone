// app_snake.h
// Port dari GAME: SNAKE file lama -- grid gerak, wrap-around tepi layar
// (v20: keluar dari satu sisi nongol lagi dari sisi seberang, BUKAN game
// over), satu2nya sebab kalah = nabrak badan sendiri. Kontrol: ketuk layar
// ke arah tujuan, kepala belok ke situ (gak bisa balik 180 derajat
// langsung). Kecepatan nambah tiap makan, sama persis rumusnya.
#pragma once

void snakeScreenShow();
