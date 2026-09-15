#include "wifi_manager.h"
#include "sys_state.h"
#include "screen_chrome.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

static const char* NTP_SERVER = "pool.ntp.org";
static const long  GMT_OFFSET = 7 * 3600; // WIB -- samain manual kalau HP kalian di zona lain
static const int   DST_OFFSET = 0;

char WIFI_SSID[64] = "";
char WIFI_PASSWORD[64] = "";
bool g_wifiConnecting = false;
bool g_airplaneMode = false;
bool g_wifiScanning = false;
ScannedWifi g_scannedWifis[MAX_SCANNED_WIFI];
int  g_scannedWifiNum = 0;

static bool s_ntpSyncing = false;
static bool s_wifiAnnounceResult = false;
static unsigned long s_wifiConnectStartMs = 0;
static unsigned long s_ntpSyncStartMs = 0;

void wifiLoadCreds() {
  Preferences p; p.begin("wifi", true);
  p.getString("ssid", "").toCharArray(WIFI_SSID, 64);
  p.getString("pass", "").toCharArray(WIFI_PASSWORD, 64);
  p.end();
}
void wifiSaveCreds() {
  Preferences p; p.begin("wifi", false);
  p.putString("ssid", WIFI_SSID);
  p.putString("pass", WIFI_PASSWORD);
  p.end();
}

void wifiConnect(bool announceResult) {
  if (g_airplaneMode || !strlen(WIFI_SSID)) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  g_wifiConnecting = true;
  s_wifiConnectStartMs = millis();
  s_wifiAnnounceResult = announceResult;
}

void wifiConnectStep() {
  if (g_wifiConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      g_wifiConnecting = false;
      g_wifiConnected = true;
      configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER); // async, cuma nyalain SNTP di background
      s_ntpSyncing = true; s_ntpSyncStartMs = millis();
      if (s_wifiAnnounceResult) chromeShowToast("WiFi Terhubung!");
    } else if (millis() - s_wifiConnectStartMs > 6000) {
      g_wifiConnecting = false;
      g_wifiConnected = false;
      if (s_wifiAnnounceResult) chromeShowToast("Gagal Connect");
    }
  }
  if (s_ntpSyncing) {
    struct tm tmChk;
    if (getLocalTime(&tmChk, 5)) { // timeout 5ms -- dipoll ulang, bukan nunggu lama sekali
      g_ntpSynced = true; s_ntpSyncing = false;
    } else if (millis() - s_ntpSyncStartMs > 5000) {
      s_ntpSyncing = false; // nyerah, g_ntpSynced tetap false -> jam nampilin "--:--"
    }
  }
}

void wifiDisconnect() {
  WiFi.disconnect(true);
  g_wifiConnected = false;
  g_wifiConnecting = false;
  s_ntpSyncing = false;
}

void wifiToggleAirplaneMode() {
  g_airplaneMode = !g_airplaneMode;
  if (g_airplaneMode) wifiDisconnect();
  else wifiConnect();
}

void wifiStartScan() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  g_wifiScanning = true;
  g_scannedWifiNum = 0;
}

void wifiCheckScanComplete() {
  if (!g_wifiScanning) return;
  int n = WiFi.scanComplete();
  if (n >= 0) {
    g_wifiScanning = false;
    g_scannedWifiNum = min(n, MAX_SCANNED_WIFI);
    for (int i = 0; i < g_scannedWifiNum; i++) {
      g_scannedWifis[i].ssid = WiFi.SSID(i);
      g_scannedWifis[i].rssi = WiFi.RSSI(i);
      g_scannedWifis[i].isEncrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
  }
}
