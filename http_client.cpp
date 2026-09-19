#include "http_client.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

static volatile HttpState s_state = HTTP_IDLE;
static String s_url;
static String s_body;
static int s_statusCode = 0;

bool httpHeapReady(size_t minBlock) {
  size_t lb = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  for (int i = 0; i < 3 && lb < minBlock; i++) {
    delay(150);
    lb = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  }
  return lb >= minBlock;
}

static void httpTask(void* param) {
  String url = s_url; // salinan lokal punya task ini sendiri (aman dr race)
  String resultBody;
  int code = -1;

  // SENGAJA selalu pakai WiFiClientSecure (HTTPS), gak ada jalur HTTP
  // polos lagi -- semua API yg dipakai proyek ini (OpenTDB, NASA, dst)
  // HTTPS semua, jadi gak perlu WiFiClient biasa. Ini juga sekalian
  // ngindarin masalah nama kelas WiFiClient yg beda2 antar versi core
  // ESP32 (di sebagian versi baru, WiFiClient.h udah direstrukturisasi
  // jadi bagian dari library "Networking").
  WiFiClientSecure client;
  client.setInsecure(); // skip validasi sertifikat -- lihat catatan di http_client.h
  HTTPClient http;
  if (http.begin(client, url)) {
    code = http.GET();
    if (code > 0) resultBody = http.getString();
    http.end();
  }

  s_body = resultBody;
  s_statusCode = code;
  s_state = (code > 0 && code < 400) ? HTTP_DONE_OK : HTTP_DONE_ERR;
  vTaskDelete(NULL);
}

bool httpGetStart(const String& url) {
  if (s_state == HTTP_BUSY) return false;
  // Guard RAM DI SINI (di luar/sebelum task dibikin) -- lihat catatan
  // panjang di http_client.h soal kenapa ini WAJIB di titik ini, bukan
  // di dalam httpTask().
  if (!httpHeapReady()) return false;
  s_url = url;
  s_body = "";
  s_statusCode = 0;
  s_state = HTTP_BUSY;
  // Stack 12KB -- TLS handshake (HTTPS) lumayan boros stack, dikasih
  // lebih dari cukup drpd pas-pasan trus stack overflow diem2 crash.
  // Pinned ke core 1 (core yg sama dgn loop() Arduino) biar akses variabel
  // s_state/s_body dari sisi polling gak lintas-core.
  xTaskCreatePinnedToCore(httpTask, "httpGetTask", 12288, NULL, 1, NULL, 1);
  return true;
}

HttpState httpGetState() { return s_state; }
int httpGetStatusCode() { return s_statusCode; }
const String& httpGetBody() { return s_body; }
void httpGetReset() { s_state = HTTP_IDLE; s_body = ""; }
