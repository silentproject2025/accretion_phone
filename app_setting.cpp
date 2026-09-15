#include "app_setting.h"
#include "screen_chrome.h"
#include "theme.h"
#include "sys_state.h"
#include "display_driver.h"
#include "wifi_manager.h"
#include "text_input.h"
#include <Arduino.h>

static lv_obj_t* s_rotateBtn;
static lv_obj_t* s_rotateLbl;

// Rebuild diri sendiri (bukan lewat navPush, biar gak numpuk stack) --
// dipakai stlh ganti tema/kalibrasi/status WiFi berubah, krn widget yg
// SUDAH ke-render gak otomatis ganti sendiri; cara paling gampang & aman:
// bikin ulang layar ini dari nol (murah, cuma beberapa obj) & buang yg lama.
static void rebuildSelf() {
  lv_obj_t* prev = lv_scr_act();
  settingScreenShow();
  lv_obj_del_async(prev);
}

static void brightness_slider_cb(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  int v = lv_slider_get_value(slider);
  display.setBrightness((uint8_t)v);
}

static void theme_btn_cb(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  themeIdx = idx;
  rebuildSelf();
}

static void rotate_toggle_cb(lv_event_t* e) {
  g_autoRotate = !g_autoRotate;
  lv_obj_set_style_bg_color(s_rotateBtn, c565(g_autoRotate ? T().good : T().surface2), 0);
  lv_label_set_text(s_rotateLbl, g_autoRotate ? "Rotasi: ON" : "Rotasi: OFF");
  lv_obj_set_style_text_color(s_rotateLbl, c565(g_autoRotate ? T().bg : T().subtext), 0);
}

static void recalibrate_btn_cb(lv_event_t* e) {
  displayRecalibrateTouch(); // blocking, gambar langsung ke panel (bukan lewat LVGL)
  rebuildSelf();             // paksa LVGL redraw penuh stlh layar kalibrasi selesai
  chromeShowToast("Kalibrasi touch selesai");
}

// =============================================
// WiFi
// =============================================
static void onPasswordEntered(const char* pass) {
  strncpy(WIFI_PASSWORD, pass, 63); WIFI_PASSWORD[63] = 0;
  wifiSaveCreds();
  wifiConnect(true);
  rebuildSelf();
}
static void onSsidEntered(const char* ssid) {
  strncpy(WIFI_SSID, ssid, 63); WIFI_SSID[63] = 0;
  chromeShowTextInput("Password WiFi", "", true, onPasswordEntered);
}
static void manual_ssid_btn_cb(lv_event_t* e) {
  chromeShowTextInput("SSID WiFi", WIFI_SSID, false, onSsidEntered);
}
static void scan_btn_cb(lv_event_t* e) {
  wifiStartScan();
  rebuildSelf(); // biar "Memindai..." langsung kelihatan; hasil nyusul via timer poll
}
static void scanned_item_cb(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  strncpy(WIFI_SSID, g_scannedWifis[idx].ssid.c_str(), 63); WIFI_SSID[63] = 0;
  if (g_scannedWifis[idx].isEncrypted) {
    chromeShowTextInput("Password WiFi", "", true, onPasswordEntered);
  } else {
    WIFI_PASSWORD[0] = 0;
    wifiSaveCreds();
    wifiConnect(true);
    rebuildSelf();
  }
}
static void connect_toggle_cb(lv_event_t* e) {
  if (g_wifiConnected || g_wifiConnecting) wifiDisconnect();
  else wifiConnect(true);
  rebuildSelf();
}
static void airplane_toggle_cb(lv_event_t* e) {
  wifiToggleAirplaneMode();
  rebuildSelf();
}

// Poll ringan tiap 500ms selagi layar Setting kebuka -- refresh status kalau
// ada perubahan (konek berhasil/gagal, scan selesai). DIJAGA supaya gak
// nge-rebuild selagi overlay input teks (SSID/password) lagi kebuka --
// kalau dipaksa rebuild, overlay-nya ikut kehapus krn dia child layar ini.
static bool s_lastConnected, s_lastConnecting, s_lastScanning;
static void poll_cb(lv_timer_t* t) {
  wifiCheckScanComplete();
  if (g_textInputActive) return;
  if (g_wifiConnected != s_lastConnected || g_wifiConnecting != s_lastConnecting || g_wifiScanning != s_lastScanning) {
    rebuildSelf();
  }
}

static const char* wifiStatusText() {
  if (g_airplaneMode) return "Mode Pesawat aktif";
  if (g_wifiConnected) return "Terhubung";
  if (g_wifiConnecting) return "Menyambung...";
  return "Terputus";
}
static uint16_t wifiStatusColor() {
  if (g_wifiConnected) return T().good;
  if (g_wifiConnecting) return T().accent;
  return T().subtext;
}

void settingScreenShow() {
  s_lastConnected = g_wifiConnected; s_lastConnecting = g_wifiConnecting; s_lastScanning = g_wifiScanning;

  lv_obj_t* scr = chromeCreateAppScreen("Pengaturan");

  // Toggle Auto-Rotate -- posisi kanan atas.
  s_rotateBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(s_rotateBtn);
  lv_obj_set_size(s_rotateBtn, 90, 20);
  lv_obj_set_pos(s_rotateBtn, 320 - 98, 24);
  lv_obj_set_style_radius(s_rotateBtn, 5, 0);
  lv_obj_set_style_bg_color(s_rotateBtn, c565(g_autoRotate ? T().good : T().surface2), 0);
  lv_obj_set_style_bg_opa(s_rotateBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_rotateBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_rotateBtn, rotate_toggle_cb, LV_EVENT_CLICKED, NULL);
  s_rotateLbl = lv_label_create(s_rotateBtn);
  lv_label_set_text(s_rotateLbl, g_autoRotate ? "Rotasi: ON" : "Rotasi: OFF");
  lv_obj_set_style_text_color(s_rotateLbl, c565(g_autoRotate ? T().bg : T().subtext), 0);
  lv_obj_set_style_text_font(s_rotateLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(s_rotateLbl);

  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 48);
  lv_obj_set_size(content, 320, 207 - 48);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 8, 0);
  lv_obj_set_style_pad_row(content, 12, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  // --- WiFi ---
  lv_obj_t* wifiHead = lv_obj_create(content);
  lv_obj_remove_style_all(wifiHead);
  lv_obj_set_size(wifiHead, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(wifiHead, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(wifiHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* wifiTitleLbl = lv_label_create(wifiHead);
  lv_label_set_text(wifiTitleLbl, "WiFi");
  lv_obj_set_style_text_color(wifiTitleLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(wifiTitleLbl, &lv_font_montserrat_14, 0);
  lv_obj_t* wifiStatusLbl = lv_label_create(wifiHead);
  lv_label_set_text(wifiStatusLbl, wifiStatusText());
  lv_obj_set_style_text_color(wifiStatusLbl, c565(wifiStatusColor()), 0);
  lv_obj_set_style_text_font(wifiStatusLbl, &lv_font_montserrat_14, 0);

  char ssidBuf[80];
  snprintf(ssidBuf, sizeof(ssidBuf), "SSID: %s", strlen(WIFI_SSID) ? WIFI_SSID : "(belum diatur)");
  lv_obj_t* ssidLbl = lv_label_create(content);
  lv_label_set_text(ssidLbl, ssidBuf);
  lv_obj_set_style_text_color(ssidLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(ssidLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* wifiBtnRow = lv_obj_create(content);
  lv_obj_remove_style_all(wifiBtnRow);
  lv_obj_set_size(wifiBtnRow, 300, 28);
  lv_obj_set_flex_flow(wifiBtnRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(wifiBtnRow, 6, 0);

  struct { const char* label; lv_event_cb_t cb; uint16_t bg; } wbtns[4] = {
    { "Pindai",  scan_btn_cb,      T().surface2 },
    { "Manual",  manual_ssid_btn_cb, T().surface2 },
    { (g_wifiConnected || g_wifiConnecting) ? "Putuskan" : "Sambung", connect_toggle_cb, T().accent },
    { g_airplaneMode ? "Pesawat:ON" : "Pesawat:OFF", airplane_toggle_cb, g_airplaneMode ? T().danger : T().surface2 },
  };
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_obj_create(wifiBtnRow);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 26);
    lv_obj_set_style_radius(b, 5, 0);
    lv_obj_set_style_bg_color(b, c565(wbtns[i].bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, wbtns[i].cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, wbtns[i].label);
    lv_obj_set_style_text_color(l, c565(wbtns[i].bg == T().accent ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  if (g_wifiScanning) {
    lv_obj_t* scanningLbl = lv_label_create(content);
    lv_label_set_text(scanningLbl, "Memindai jaringan...");
    lv_obj_set_style_text_color(scanningLbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(scanningLbl, &lv_font_montserrat_14, 0);
  } else if (g_scannedWifiNum > 0) {
    lv_obj_t* listWrap = lv_obj_create(content);
    lv_obj_remove_style_all(listWrap);
    lv_obj_set_size(listWrap, 300, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(listWrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(listWrap, 4, 0);
    for (int i = 0; i < g_scannedWifiNum; i++) {
      lv_obj_t* item = lv_obj_create(listWrap);
      lv_obj_remove_style_all(item);
      lv_obj_set_size(item, 300, 26);
      lv_obj_set_style_radius(item, 5, 0);
      lv_obj_set_style_bg_color(item, c565(T().surface), 0);
      lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
      lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_hor(item, 10, 0);
      lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(item, scanned_item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

      lv_obj_t* nameLbl = lv_label_create(item);
      lv_label_set_text(nameLbl, g_scannedWifis[i].ssid.c_str());
      lv_obj_set_style_text_color(nameLbl, c565(T().text), 0);
      lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);

      lv_obj_t* rssiLbl = lv_label_create(item);
      char rb[16]; snprintf(rb, sizeof(rb), "%s %ddBm", g_scannedWifis[i].isEncrypted ? LV_SYMBOL_CLOSE : LV_SYMBOL_WIFI, g_scannedWifis[i].rssi);
      lv_label_set_text(rssiLbl, rb);
      lv_obj_set_style_text_color(rssiLbl, c565(T().subtext), 0);
      lv_obj_set_style_text_font(rssiLbl, &lv_font_montserrat_14, 0);
    }
  }

  lv_obj_t* wifiDivider = lv_obj_create(content);
  lv_obj_remove_style_all(wifiDivider);
  lv_obj_set_size(wifiDivider, 300, 1);
  lv_obj_set_style_bg_color(wifiDivider, c565(T().divider), 0);
  lv_obj_set_style_bg_opa(wifiDivider, LV_OPA_COVER, 0);

  // --- Kecerahan ---
  lv_obj_t* briLbl = lv_label_create(content);
  lv_label_set_text(briLbl, "Kecerahan");
  lv_obj_set_style_text_color(briLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(briLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* briSlider = lv_slider_create(content);
  lv_obj_set_size(briSlider, 300, 12);
  lv_slider_set_range(briSlider, 10, 255); // min 10 biar gak sengaja bikin layar gelap total tanpa sadar
  lv_slider_set_value(briSlider, 255, LV_ANIM_OFF); // TODO: baca dari variabel `brightness` tersimpan kalau sudah di-port
  lv_obj_set_style_bg_color(briSlider, c565(T().divider), LV_PART_MAIN);
  lv_obj_set_style_bg_color(briSlider, c565(T().accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(briSlider, c565(T().accent), LV_PART_KNOB);
  lv_obj_add_event_cb(briSlider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // --- Tema ---
  lv_obj_t* themeLbl = lv_label_create(content);
  lv_label_set_text(themeLbl, "Tema");
  lv_obj_set_style_text_color(themeLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(themeLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* themeRow = lv_obj_create(content);
  lv_obj_remove_style_all(themeRow);
  lv_obj_set_size(themeRow, 300, 28);
  lv_obj_set_flex_flow(themeRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(themeRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(themeRow, 4, 0);

  for (int i = 0; i < THEME_COUNT; i++) {
    lv_obj_t* btn = lv_obj_create(themeRow);
    lv_obj_remove_style_all(btn);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 26);
    lv_obj_set_style_radius(btn, 5, 0);
    lv_obj_set_style_bg_color(btn, c565(i == themeIdx ? T().accent : T().surface2), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, theme_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, themes[i].name);
    lv_obj_set_style_text_color(lbl, c565(i == themeIdx ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);
  }

  // --- Kalibrasi Ulang ---
  lv_obj_t* calBtn = lv_obj_create(content);
  lv_obj_remove_style_all(calBtn);
  lv_obj_set_size(calBtn, 300, 30);
  lv_obj_set_style_radius(calBtn, 6, 0);
  lv_obj_set_style_bg_color(calBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(calBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(calBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(calBtn, recalibrate_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* calLbl = lv_label_create(calBtn);
  lv_label_set_text(calLbl, "Kalibrasi Ulang Touch");
  lv_obj_set_style_text_color(calLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(calLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(calLbl);

  // --- Catatan bagian yg belum di-port ---
  lv_obj_t* note = lv_label_create(content);
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(note, 300);
  lv_label_set_text(note,
    "Pilihan Font & kalibrasi sensor gerak MPU6050 belum tersedia di versi "
    "LVGL ini -- menyusul stlh sensor gerak di-port.");
  lv_obj_set_style_text_color(note, c565(T().subtext), 0);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);

  lv_timer_t* timer = lv_timer_create(poll_cb, 500, NULL);
  chromeBindTimerToScreen(scr, timer);
}
