// http_client.h
// Subsistem HTTP GET generik, NON-BLOCKING -- dipakai app manapun yg perlu
// ambil data dari internet (Trivia, app NASA, Update OTA cek versi, AI
// Chat). Pola sama persis semangatnya dgn doGeminiHttpRequest()/
// geminiTaskFunc() di file lama: request jalan di FreeRTOS task TERPISAH
// (HTTPClient.GET() itu sendiri blocking), app cuma perlu POLL hasilnya
// tiap ~200-300ms lewat timer yg udah biasa dipakai tiap app screen --
// LVGL/UI thread gak pernah ketahan nunggu jaringan.
//
// Cuma bisa 1 request brjalan dlm satu waktu (cukup krn cuma 1 layar app
// yg aktif dlm satu waktu jg). HTTPS pakai WiFiClientSecure::setInsecure()
// (skip validasi sertifikat) -- pola umum di proyek hobi ESP32, bukan
// paling aman scr kriptografis tapi cukup buat baca API publik spt ini.
//
// GUARD RAM (PENTING, pelajaran dari file lama v53->v61->v63): bikin
// FreeRTOS task baru (spt yg dipakai buat request async ini) LANGSUNG
// motong largest-free-block RAM internal sebesar stack task itu (12KB di
// sini) SEBELUM baris pertama task-nya sempat jalan. Kalau baru DI DALAM
// task itu kita cek "RAM cukup gak", potongan 12KB itu GAK BAKAL lepas
// sampai task-nya sendiri selesai -- chicken-and-egg, gak akan pernah lolos
// kalau RAM emang lagi pas-pasan. FIX-nya: httpGetStart() di sini cek+
// tunggu RAM DULU, DI LUAR/SEBELUM task dibikin -- kalau msh kurang stlh
// retry singkat, BATALKAN (return false), JANGAN tetap bikin task-nya.
#pragma once
#include <Arduino.h>

enum HttpState { HTTP_IDLE, HTTP_BUSY, HTTP_DONE_OK, HTTP_DONE_ERR };

// Cek (& kasih kesempatan RAM pulih sebentar) apakah aman bikin task HTTP
// baru sekarang. Dipanggil OTOMATIS di dalam httpGetStart(), tapi juga bisa
// dipanggil duluan sendiri kalau app kalian mau kasih pesan spesifik
// "RAM lagi padat" SEBELUM nyoba mulai fetch (spt file lama).
bool httpHeapReady(size_t minBlock = 20000);

// Mulai GET request async. Return false kalau ada request lain yg masih
// jalan (httpGetState()==HTTP_BUSY), ATAU RAM internal gak cukup buat bikin
// task baru dgn aman (lihat httpHeapReady() di atas) -- cek httpGetState()
// abis ini: masih HTTP_IDLE = gagal mulai (salah satu dari dua alasan tsb).
bool httpGetStart(const String& url);

// Panggil dari timer app kalian tiap beberapa ratus ms buat cek progress.
HttpState httpGetState();

// Cuma valid kalau httpGetState()==HTTP_DONE_OK/HTTP_DONE_ERR.
int httpGetStatusCode();       // kode HTTP (200, 404, dst), -1 kalau gagal konek sama sekali
const String& httpGetBody();   // isi respons (body) kalau HTTP_DONE_OK

// WAJIB dipanggil stlh selesai baca hasil (OK ataupun ERR) sblm mulai
// request baru -- balikin state ke IDLE & bebasin slot.
void httpGetReset();
