// app_calculator.h
// Port dari calcEnter()/drawCalc()/calcTouch()/calcApplyLabel() di file lama.
// Pakai widget lv_btnmatrix bawaan LVGL (bukan bikin 20 tombol manual) --
// jauh lebih ringkas & itu memang gunanya widget ini.
//
// Perbaikan kecil vs versi lama: di file lama, tombol "." (titik desimal)
// TIDAK PERNAH BISA DIPENCET -- rect-nya ketiban tombol "0" yg dilebarin
// (bug lama, area sentuhnya w=0 alias gak pernah ke-hit-test). Di versi
// LVGL ini titik desimal beneran jalan.
#pragma once

void calculatorScreenShow();
