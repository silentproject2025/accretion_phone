#include "app_neopixel.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>

// =============================================
// Preset warna -- disalin persis dari npxPresets[] file lama.
// =============================================
struct NpxPreset { const char* name; uint8_t r, g, b; };
static const NpxPreset kPresets[8] = {
  {"Merah",255,0,0},{"Oranye",255,110,0},{"Kuning",255,220,0},{"Hijau",0,200,60},
  {"Cyan",0,200,200},{"Biru",30,80,255},{"Ungu",160,40,220},{"Putih",255,255,255}
};

static bool    s_on = false;
static uint8_t s_r = 255, s_g = 0, s_b = 0;
static uint8_t s_mode = 0;         // 0=warna solid, 1=rainbow
static uint8_t s_brightness = 180; // 0..255, sama default rentang file lama
static float   s_hue = 0;

static lv_obj_t* s_onOffBtn;
static lv_obj_t* s_onOffLbl;
static lv_obj_t* s_swatchObjs[8];
static lv_obj_t* s_rainbowBtn;
static lv_obj_t* s_rainbowLbl;
static lv_obj_t* s_preview;
static lv_obj_t* s_slider;

// =============================================
// TODO HARDWARE: di sini tempat manggil library LED fisik (mis.
// Adafruit_NeoPixel/FastLED di GPIO 48, sama spt file lama). Sekarang
// masih no-op (cuma log Serial) -- app ini fungsional penuh dari sisi
// UI/state, tinggal 2 fungsi ini yg perlu diisi pas library LED sudah
// ditambahin ke proyek (arduino-cli lib install "Adafruit NeoPixel" dkk).
// =============================================
static void npxApplyColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  Serial.printf("[Neopixel] TODO tulis ke LED fisik: r=%d g=%d b=%d bright=%d\n", r, g, b, brightness);
}
static void npxApplyOff() {
  Serial.println("[Neopixel] TODO matikan LED fisik");
}

static void updatePreview() {
  uint8_t dr = (uint8_t)((int)s_r * s_brightness / 255);
  uint8_t dg = (uint8_t)((int)s_g * s_brightness / 255);
  uint8_t db = (uint8_t)((int)s_b * s_brightness / 255);
  lv_obj_set_style_bg_color(s_preview, lv_color_make(dr, dg, db), 0);
  lv_obj_set_style_bg_opa(s_preview, s_on ? LV_OPA_COVER : LV_OPA_30, 0);
}

static void updateOnOffBtn() {
  lv_obj_set_style_bg_color(s_onOffBtn, c565(s_on ? T().good : T().surface2), 0);
  lv_label_set_text(s_onOffLbl, s_on ? "ON" : "OFF");
  lv_obj_set_style_text_color(s_onOffLbl, c565(s_on ? T().bg : T().subtext), 0);
}

static void updateSelectionHighlight() {
  for (int i = 0; i < 8; i++) {
    bool sel = s_on && s_mode == 0 && s_r == kPresets[i].r && s_g == kPresets[i].g && s_b == kPresets[i].b;
    lv_obj_set_style_border_width(s_swatchObjs[i], sel ? 3 : 1, 0);
    lv_obj_set_style_border_color(s_swatchObjs[i], c565(sel ? T().accent : T().divider), 0);
  }
  bool rbActive = s_on && s_mode == 1;
  lv_obj_set_style_bg_color(s_rainbowBtn, c565(rbActive ? T().accent : T().surface2), 0);
  lv_obj_set_style_text_color(s_rainbowLbl, c565(rbActive ? T().bg : T().text), 0);
}

static void applyAndRedraw() {
  uint8_t br = s_on ? s_brightness : 0;
  if (s_on) npxApplyColor(s_r, s_g, s_b, br); else npxApplyOff();
  updatePreview();
  updateOnOffBtn();
  updateSelectionHighlight();
}

static void onoff_click_cb(lv_event_t* e) {
  s_on = !s_on;
  applyAndRedraw();
}

static void swatch_click_cb(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  s_r = kPresets[idx].r; s_g = kPresets[idx].g; s_b = kPresets[idx].b;
  s_mode = 0; s_on = true;
  applyAndRedraw();
}

static void rainbow_click_cb(lv_event_t* e) {
  s_mode = 1; s_on = true;
  applyAndRedraw();
}

static void slider_event_cb(lv_event_t* e) {
  s_brightness = (uint8_t)lv_slider_get_value(s_slider);
  applyAndRedraw();
}

// Konversi HSV->RGB sederhana, dipakai mode Rainbow (h=0..360, s=v=1.0).
static void hsvToRgb(float h, uint8_t* r, uint8_t* g, uint8_t* b) {
  float c = 1.0f, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = 0;
  float rf, gf, bf;
  if      (h < 60)  { rf = c; gf = x; bf = 0; }
  else if (h < 120) { rf = x; gf = c; bf = 0; }
  else if (h < 180) { rf = 0; gf = c; bf = x; }
  else if (h < 240) { rf = 0; gf = x; bf = c; }
  else if (h < 300) { rf = x; gf = 0; bf = c; }
  else              { rf = c; gf = 0; bf = x; }
  *r = (uint8_t)((rf + m) * 255);
  *g = (uint8_t)((gf + m) * 255);
  *b = (uint8_t)((bf + m) * 255);
}

static void rainbow_tick_cb(lv_timer_t* t) {
  if (!(s_on && s_mode == 1)) return;
  s_hue += 6; if (s_hue >= 360) s_hue -= 360;
  hsvToRgb(s_hue, &s_r, &s_g, &s_b);
  npxApplyColor(s_r, s_g, s_b, s_brightness);
  updatePreview();
}

void neopixelScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Neopixel");

  // Tombol ON/OFF -- posisi identik npxHeaderBtnW()=70 di kanan atas.
  s_onOffBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(s_onOffBtn);
  lv_obj_set_size(s_onOffBtn, 70, 22);
  lv_obj_set_pos(s_onOffBtn, 320 - 8 - 70, 22);
  lv_obj_set_style_radius(s_onOffBtn, 6, 0);
  lv_obj_set_style_bg_opa(s_onOffBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_onOffBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_onOffBtn, onoff_click_cb, LV_EVENT_CLICKED, NULL);
  s_onOffLbl = lv_label_create(s_onOffBtn);
  lv_obj_center(s_onOffLbl);

  // Konten scrollable -- grid warna + rainbow + slider + preview.
  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 48);
  lv_obj_set_size(content, 320, 207 - 48);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 6, 0);
  lv_obj_set_style_pad_row(content, 10, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  // Grid 8 swatch (4 kolom, sama spt NPX_SW_COLS=4 file lama).
  lv_obj_t* grid = lv_obj_create(content);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(grid, 8, 0);

  for (int i = 0; i < 8; i++) {
    lv_obj_t* cell = lv_obj_create(grid);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, 64, 58);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* sw = lv_obj_create(cell);
    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, 36, 36);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, lv_color_make(kPresets[i].r, kPresets[i].g, kPresets[i].b), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_set_style_border_color(sw, c565(T().divider), 0);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sw, swatch_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    s_swatchObjs[i] = sw;

    lv_obj_t* lbl = lv_label_create(cell);
    lv_label_set_text(lbl, kPresets[i].name);
    lv_obj_set_style_text_color(lbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  }

  // Tombol Rainbow
  s_rainbowBtn = lv_obj_create(content);
  lv_obj_remove_style_all(s_rainbowBtn);
  lv_obj_set_size(s_rainbowBtn, 300, 32);
  lv_obj_set_style_radius(s_rainbowBtn, 8, 0);
  lv_obj_set_style_bg_opa(s_rainbowBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(s_rainbowBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_rainbowBtn, rainbow_click_cb, LV_EVENT_CLICKED, NULL);
  s_rainbowLbl = lv_label_create(s_rainbowBtn);
  lv_label_set_text(s_rainbowLbl, "Rainbow");
  lv_obj_center(s_rainbowLbl);

  // Preview warna aktif (tambahan baru -- di file lama gak ada, krn LED
  // fisiknya sendiri yg jadi "preview". Berguna dites walau LED blm disambung).
  lv_obj_t* previewRow = lv_obj_create(content);
  lv_obj_remove_style_all(previewRow);
  lv_obj_set_size(previewRow, 300, 40);
  lv_obj_set_flex_flow(previewRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(previewRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(previewRow, 10, 0);

  s_preview = lv_obj_create(previewRow);
  lv_obj_remove_style_all(s_preview);
  lv_obj_set_size(s_preview, 32, 32);
  lv_obj_set_style_radius(s_preview, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(s_preview, 1, 0);
  lv_obj_set_style_border_color(s_preview, c565(T().divider), 0);

  lv_obj_t* previewLbl = lv_label_create(previewRow);
  lv_label_set_text(previewLbl, "Preview (LED fisik: TODO)");
  lv_obj_set_style_text_color(previewLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(previewLbl, &lv_font_montserrat_14, 0);

  // Slider kecerahan
  lv_obj_t* brightLbl = lv_label_create(content);
  lv_label_set_text(brightLbl, "Kecerahan");
  lv_obj_set_style_text_color(brightLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(brightLbl, &lv_font_montserrat_14, 0);

  s_slider = lv_slider_create(content);
  lv_obj_set_size(s_slider, 300, 12);
  lv_slider_set_range(s_slider, 0, 255);
  lv_slider_set_value(s_slider, s_brightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_slider, c565(T().divider), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_slider, c565(T().accent), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s_slider, c565(T().accent), LV_PART_KNOB);
  lv_obj_add_event_cb(s_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  applyAndRedraw();

  lv_timer_t* timer = lv_timer_create(rainbow_tick_cb, 40, NULL);
  chromeBindTimerToScreen(scr, timer);
}
