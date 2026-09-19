// storage_manager.h
// Port dari initSD()/initFfat() di file lama -- SD_MMC (kartu SD fisik)
// sbg penyimpanan utama, FFat (partisi flash internal) sbg fallback kalau
// SD Card gak kedetek. Dipakai Notepad (& nanti Files/Canvas/AI memory).
#pragma once
#include <Arduino.h>

extern bool g_sdReady;
extern bool g_ffatReady;

// Panggil SEKALI di setup().
void storageInit();

// Helper baca/tulis file teks kecil (notes, dll) -- otomatis pilih SD kalau
// ada, kalau enggak & FFat tersedia baca/tulis dari situ sbg fallback.
// Return "" kalau file gak ada / gagal baca / storage gak tersedia.
String storageReadText(const char* path);
// Return true kalau berhasil ditulis.
bool storageWriteText(const char* path, const String& content);

// ---- Dipakai app Files ----
#define STORAGE_MAX_FILES 40
struct StorageFileEntry { String name; uint32_t size; };
// Scan folder root (SD kalau ada, kalau enggak FFat) -- return jumlah file
// (bukan folder) yg ketemu, diisi ke out[].
int storageListFiles(StorageFileEntry out[], int maxN);
// Return true kalau berhasil dihapus.
bool storageDeleteFile(const char* path);
