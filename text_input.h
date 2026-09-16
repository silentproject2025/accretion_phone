// text_input.h
// Overlay input teks pakai widget BAWAAN LVGL (lv_textarea + lv_keyboard) --
// gantiin sistem virtual keyboard custom yg lama (kbVisible/kbTarget/dll).
// Reusable dipakai app manapun yg butuh input teks (SSID/password WiFi di
// Setting, nanti Notepad/AI Chat/dll).
#pragma once

// Tampilkan overlay full-screen di atas layar AKTIF saat ini: judul +
// textarea (terisi initialText) + keyboard di bawah. Kalau user tekan
// tombol OK/checkmark di keyboard, onDone dipanggil dgn teks final lalu
// overlay ditutup. Kalau user Cancel, overlay ditutup TANPA manggil onDone.
// isPassword=true -> teks disensor titik-titik (dipakai utk field password).
void chromeShowTextInput(const char* title, const char* initialText,
                          bool isPassword, void (*onDone)(const char* text));

// True selama overlay ini terbuka -- dicek layar lain (mis. Setting) yg
// punya timer auto-refresh sendiri, biar timer itu TIDAK rebuild diri
// (yg bakal ikut ngehapus overlay ini krn dia child dari layar itu) selagi
// user masih ngetik.
extern bool g_textInputActive;
