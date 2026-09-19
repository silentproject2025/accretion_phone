// wifi_manager.h
// Port dari blok "WIFI & NTP & AUTO SCAN" di file lama -- state machine
// non-blocking yg SAMA PERSIS (budget waktu 6 detik konek, 5 detik NTP,
// dipoll tiap loop() lewat wifiConnectStep(), BUKAN while()+delay() yg
// nge-freeze UI). g_wifiConnected/g_ntpSynced (sys_state.h) diupdate
// otomatis dari sini -- semua screen yg udah baca dari situ (status bar,
// dll) langsung ikut kebawa tanpa perlu diubah.
//
// BELUM diikutkan: startWebServer()/AI memory/Gemini HTTP client (itu
// scope app AI Chat sendiri, bukan subsistem WiFi dasar).
#pragma once
#include <Arduino.h>

#define MAX_SCANNED_WIFI 8
struct ScannedWifi {
  String ssid;
  int rssi;
  bool isEncrypted;
};

extern char WIFI_SSID[64];
extern char WIFI_PASSWORD[64];
extern bool g_wifiConnecting;
extern bool g_airplaneMode;
extern bool g_wifiScanning;
extern ScannedWifi g_scannedWifis[MAX_SCANNED_WIFI];
extern int  g_scannedWifiNum;

void wifiLoadCreds();
void wifiSaveCreds(); // panggil stlh ganti WIFI_SSID/WIFI_PASSWORD manual
// announceResult=true -> kasih toast hasilnya (dipakai kalau dipicu manual
// dari tombol Connect, BUKAN auto-connect saat boot).
void wifiConnect(bool announceResult = false);
void wifiDisconnect();
void wifiToggleAirplaneMode();
void wifiStartScan();

// WAJIB dipanggil tiap loop() (di phone.ino) -- majuin proses konek/NTP
// sedikit demi sedikit tanpa pernah nge-block.
void wifiConnectStep();
// WAJIB dipanggil tiap loop() jg (murah) -- cek hasil scan async selesai.
void wifiCheckScanComplete();
