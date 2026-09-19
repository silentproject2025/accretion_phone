#include "storage_manager.h"
#include <SD_MMC.h>
#include <FFat.h>

// Pin SD_MMC -- sama persis dgn file lama.
#define SD_PIN_CLK 39
#define SD_PIN_CMD 38
#define SD_PIN_D0  40

bool g_sdReady = false;
bool g_ffatReady = false;

void storageInit() {
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);
  g_sdReady = SD_MMC.begin("/sdcard", true); // true = mode 1-bit, sama spt file lama

  // FFat SENGAJA cuma fallback (bukan utama) -- prioritas tetap SD Card
  // kalau ada, sama spt strategi wallpaper di file lama. `true` = auto-format
  // kalau partisi FFat belum pernah dipakai (aman, sekali doang di boot
  // pertama). Kalau Partition Scheme yg dipilih gak nyisain ruang FFat,
  // ini balikin false & fallback-nya otomatis gak aktif -- gak bikin crash.
  g_ffatReady = FFat.begin(true);
}

String storageReadText(const char* path) {
  if (g_sdReady) {
    File f = SD_MMC.open(path, FILE_READ);
    if (f) { String s = f.readString(); f.close(); return s; }
  }
  if (g_ffatReady) {
    File f = FFat.open(path, FILE_READ);
    if (f) { String s = f.readString(); f.close(); return s; }
  }
  return "";
}

bool storageWriteText(const char* path, const String& content) {
  if (g_sdReady) {
    File f = SD_MMC.open(path, FILE_WRITE);
    if (f) { f.print(content); f.close(); return true; }
  }
  if (g_ffatReady) {
    File f = FFat.open(path, FILE_WRITE);
    if (f) { f.print(content); f.close(); return true; }
  }
  return false;
}

int storageListFiles(StorageFileEntry out[], int maxN) {
  int n = 0;
  File root;
  if (g_sdReady) root = SD_MMC.open("/");
  else if (g_ffatReady) root = FFat.open("/");
  else return 0;
  if (!root) return 0;

  File f = root.openNextFile();
  while (f && n < maxN) {
    if (!f.isDirectory()) {
      String name = String(f.name());
      if (!name.startsWith("/")) name = "/" + name;
      out[n].name = name;
      out[n].size = f.size();
      n++;
    }
    f = root.openNextFile();
  }
  return n;
}

bool storageDeleteFile(const char* path) {
  if (g_sdReady) return SD_MMC.remove(path);
  if (g_ffatReady) return FFat.remove(path);
  return false;
}
