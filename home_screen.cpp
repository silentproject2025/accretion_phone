#include "home_screen.h"
#include "theme.h"
#include "status_bar.h"
#include "screen_chrome.h"
#include "sys_state.h"
#include "nav.h"
#include "app_clock.h"
#include "app_calculator.h"
#include "app_battery.h"
#include "app_neopixel.h"
#include "app_hwmonitor.h"
#include "app_setting.h"
#include "app_tictactoe.h"
#include "app_2048.h"
#include "app_notepad.h"
#include "app_orientasi3d.h"
#include "app_snake.h"
#include "app_flappy.h"
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>

// =============================================
// Data app -- disalin dari apps[]/initAppColors() lama (nilai warna tema
// "Dark", themeIdx=0). Kalau kalian ganti tema aktif, panggil ulang fungsi
// yang meng-update style lingkaran ikon (belum ada di fondasi ini -- untuk
// sekarang warna app fixed mengikuti tema Dark).
// Catatan: index Mic Level (dulu index 22) di file ASLI gak pernah kebagian
// warna sama sekali (bug lama -- lingkarannya jadi hitam), di sini sudah
// dikasih warna supaya tidak keulang.
//
// iconSymbol: LV_SYMBOL_xxx bawaan LVGL, dipasang cuma di app yg makna
// ikonnya beneran cocok (gear=Setting, folder=Files, dst). Sisanya nullptr
// -> tetap fallback huruf `sym`, krn LVGL gak punya ikon generik yg pas
// buat "Snake", "Inferno", "Astronomi", dsb.
//
// onOpen: nullptr = belum di-port (tap = toast placeholder). Sejauh ini
// yg udah di-port: Jam, Kalkulator, Orientasi3D, Setting, Notepad, Baterai,
// Neopixel, HWmonitor, Snake, Flappy, 2048, TicTacToe.
// =============================================
AppDef apps[APP_COUNT] = {
  { "Jam",         'J', nullptr,               0xFD40, clockScreenShow },
  { "Kalkulator",  '+', LV_SYMBOL_PLUS,        0x04FF, calculatorScreenShow },
  { "Orientasi3D", '3', LV_SYMBOL_GPS,         0x07FF, orientasi3dScreenShow },
  { "Setting",     '@', LV_SYMBOL_SETTINGS,    0xF81F, settingScreenShow },
  { "Notepad",     'N', LV_SYMBOL_EDIT,        0xFFE0, notepadScreenShow },
  { "Canvas",      'C', LV_SYMBOL_IMAGE,       0x07E0, nullptr },
  { "AI Chat",     'A', nullptr,               0xFD40, nullptr },
  { "Files",       'F', LV_SYMBOL_DIRECTORY,   0x3ADF, nullptr },
  { "MJPEG",       'M', LV_SYMBOL_VIDEO,       0xFBE0, nullptr },
  { "Update",      'U', LV_SYMBOL_DOWNLOAD,    0xF800, nullptr },
  { "Baterai",     'B', LV_SYMBOL_CHARGE,      0x07E0, batteryScreenShow },
  { "Snake",       'S', nullptr,               0x1FF9, snakeScreenShow },
  { "Flappy",      'V', nullptr,               0xFC9F, flappyScreenShow },
  { "2048",        '2', nullptr,               0xFFD2, game2048ScreenShow },
  { "TicTacToe",   'X', nullptr,               0x861F, tictactoeScreenShow },
  { "Breakout",    'K', nullptr,               0xFB40, nullptr },
  { "Trivia",      'Q', nullptr,               0xFDA0, nullptr },
  { "Astronomi",   '*', nullptr,               0x3A5F, nullptr },
  { "NEO Asteroid",'O', nullptr,               0xFBC0, nullptr },
  { "Bumi EPIC",   'E', nullptr,               0x1F9F, nullptr },
  { "Galeri NASA", 'G', LV_SYMBOL_IMAGE,       0x781F, nullptr },
  { "Neopixel",    'P', LV_SYMBOL_TINT,        0xF81F, neopixelScreenShow },
  { "Mic Level",   'L', LV_SYMBOL_VOLUME_MAX,  0x9FF3, nullptr },
  { "HWmonitor",   'H', nullptr,               0x07FF, hwmonitorScreenShow },
  { "Labirin",     'Z', nullptr,               0x4A1F, nullptr },
  { "Inferno",     'R', nullptr,               0xC2E2, nullptr },
};

static lv_obj_t* s_greetLabel;
static lv_obj_t* s_clockLabel;
static lv_obj_t* s_dateLabel;

static const char* homeGreeting(int hour) {
  if (hour < 5)  return "Malam";
  if (hour < 11) return "Pagi";
  if (hour < 15) return "Siang";
  if (hour < 19) return "Sore";
  return "Malam";
}

static void app_icon_click_cb(lv_event_t* e) {
  AppDef* app = (AppDef*)lv_event_get_user_data(e);
  if (app->onOpen) {
    navPush(app->onOpen);
    return;
  }
  Serial.printf("[Home] Tap app: %s (belum di-port ke LVGL)\n", app->name);
  char buf[48];
  snprintf(buf, sizeof(buf), "%s (segera hadir)", app->name);
  chromeShowToast(buf);
}

// Isi glyph di dalam lingkaran ikon: pakai iconSymbol (font simbol LVGL)
// kalau ada, kalau nullptr fallback ke huruf tunggal `sym`.
static void setIconGlyph(lv_obj_t* circle, AppDef* app, const lv_font_t* font) {
  lv_obj_t* g = lv_label_create(circle);
  if (app->iconSymbol) {
    lv_label_set_text(g, app->iconSymbol);
  } else {
    char buf[2] = { app->sym, 0 };
    lv_label_set_text(g, buf);
  }
  lv_obj_set_style_text_font(g, font, 0);
  lv_obj_set_style_text_color(g, c565(T().bg), 0);
  lv_obj_center(g);
}

// Satu "kartu" app: lingkaran warna + simbol/huruf + label nama di bawah.
static void buildAppCard(lv_obj_t* parent, AppDef* app) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, 72, 78);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* circle = lv_obj_create(card);
  lv_obj_remove_style_all(circle);
  lv_obj_set_size(circle, 40, 40);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle, c565(app->color), 0);
  lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(circle, 2, 0);
  lv_obj_set_style_border_color(circle, c565(T().divider), 0);
  lv_obj_add_flag(circle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(circle, app_icon_click_cb, LV_EVENT_CLICKED, app);
  setIconGlyph(circle, app, &lv_font_montserrat_20);

  lv_obj_t* name = lv_label_create(card);
  lv_label_set_text(name, app->name);
  lv_obj_set_style_text_color(name, c565(T().text), 0);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
}

static void header_tick_cb(lv_timer_t* t) {
  struct tm ti;
  bool ok = g_ntpSynced && getLocalTime(&ti);
  char tb[6];
  if (ok) snprintf(tb, sizeof(tb), "%02d:%02d", ti.tm_hour, ti.tm_min);
  else strcpy(tb, "--:--");
  lv_label_set_text(s_clockLabel, tb);
  lv_label_set_text(s_greetLabel, ok ? homeGreeting(ti.tm_hour) : "Halo");

  if (ok) {
    static const char* days[] = {"Min","Sen","Sel","Rab","Kam","Jum","Sab"};
    static const char* mons[] = {"Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"};
    char db[24];
    snprintf(db, sizeof(db), "%s, %d %s", days[ti.tm_wday], ti.tm_mday, mons[ti.tm_mon]);
    lv_label_set_text(s_dateLabel, db);
  } else {
    lv_label_set_text(s_dateLabel, "");
  }
}

void homeScreenShow() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, c565(T().bg), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_scr_load(scr);

  statusBarCreate(scr);

  // ---- Header: sapaan + jam besar + tanggal ----
  lv_obj_t* header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, LV_PCT(100), 62);
  lv_obj_set_pos(header, 0, 22);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  s_greetLabel = lv_label_create(header);
  lv_obj_set_style_text_color(s_greetLabel, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_greetLabel, &lv_font_montserrat_14, 0);

  s_clockLabel = lv_label_create(header);
  lv_obj_set_style_text_color(s_clockLabel, c565(T().text), 0);
  lv_obj_set_style_text_font(s_clockLabel, &lv_font_montserrat_28, 0);

  s_dateLabel = lv_label_create(header);
  lv_obj_set_style_text_color(s_dateLabel, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_dateLabel, &lv_font_montserrat_14, 0);

  // ---- Grid app (scrollable) ----
  lv_obj_t* grid = lv_obj_create(scr);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, LV_PCT(100), 240 - 22 - 62 - 46);
  lv_obj_set_pos(grid, 0, 22 + 62);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(grid, 4, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_AUTO);

  for (int i = 0; i < APP_COUNT; i++) {
    buildAppCard(grid, &apps[i]);
  }

  // ---- Dock (4 shortcut: Jam, Notepad, AI Chat, Files -- sama spt lama) ----
  lv_obj_t* dock = lv_obj_create(scr);
  lv_obj_remove_style_all(dock);
  lv_obj_set_size(dock, LV_PCT(100) - 12, 38);
  lv_obj_align(dock, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(dock, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(dock, LV_OPA_80, 0);
  lv_obj_set_style_radius(dock, 18, 0);
  lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(dock, LV_OBJ_FLAG_SCROLLABLE);

  int dockIdx[4] = {0, 4, 6, 7}; // Jam, Notepad, AI Chat, Files
  for (int i = 0; i < 4; i++) {
    AppDef* app = &apps[dockIdx[i]];
    lv_obj_t* circle = lv_obj_create(dock);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 26, 26);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, c565(app->color), 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_add_flag(circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(circle, app_icon_click_cb, LV_EVENT_CLICKED, app);
    setIconGlyph(circle, app, &lv_font_montserrat_14);
  }

  // Update sapaan/jam-besar/tanggal tiap 1 detik. Diikat ke siklus hidup
  // `scr` biar timer lama otomatis kehapus tiap kali homeScreenShow()
  // dipanggil ulang (mis. abis navBack() dari app screen) -- tanpa ini,
  // timer versi lama bakal numpuk & bisa crash krn nembak objek yg udah
  // dihapus.
  lv_timer_t* timer = lv_timer_create(header_tick_cb, 1000, NULL);
  header_tick_cb(timer);
  chromeBindTimerToScreen(scr, timer);
}
