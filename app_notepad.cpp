#include "app_notepad.h"
#include "screen_chrome.h"
#include "theme.h"
#include "storage_manager.h"
#include <Arduino.h>

#define NOTE_FILE "/notepad.txt"

static lv_obj_t* s_ta;
static lv_obj_t* s_kb;
static bool s_dirty = false;

static void ta_focus_cb(lv_event_t* e) {
  lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(s_ta, 312, 44); // ciutkan biar muat di atas keyboard
}
static void ta_value_changed_cb(lv_event_t* e) {
  s_dirty = true;
}
static void kb_close_cb(lv_event_t* e) {
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(s_ta, 312, 207 - 48); // balikin full lagi
  lv_obj_clear_state(s_ta, LV_STATE_FOCUSED);
}
static void autosave_tick_cb(lv_timer_t* t) {
  if (!s_dirty) return;
  s_dirty = false;
  storageWriteText(NOTE_FILE, String(lv_textarea_get_text(s_ta)));
}
static void delete_btn_cb(lv_event_t* e) {
  lv_textarea_set_text(s_ta, "");
  storageWriteText(NOTE_FILE, "");
  s_dirty = false;
  chromeShowToast("Dihapus");
}

void notepadScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Notepad");

  // Tombol Hapus -- posisi/warna identik file lama (kanan atas, merah).
  lv_obj_t* delBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(delBtn);
  lv_obj_set_size(delBtn, 62, 18);
  lv_obj_set_pos(delBtn, 320 - 70, 24);
  lv_obj_set_style_radius(delBtn, 4, 0);
  lv_obj_set_style_bg_color(delBtn, c565(T().danger), 0);
  lv_obj_set_style_bg_opa(delBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(delBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(delBtn, delete_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* delLbl = lv_label_create(delBtn);
  lv_label_set_text(delLbl, "Hapus");
  lv_obj_set_style_text_color(delLbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(delLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(delLbl);

  s_ta = lv_textarea_create(scr);
  lv_obj_set_pos(s_ta, 4, 48);
  lv_obj_set_size(s_ta, 312, 207 - 48);
  lv_obj_set_style_bg_color(s_ta, c565(T().surface), 0);
  lv_obj_set_style_text_color(s_ta, c565(T().text), 0);
  lv_textarea_set_text(s_ta, storageReadText(NOTE_FILE).c_str());
  lv_obj_add_event_cb(s_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
  lv_obj_add_event_cb(s_ta, ta_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

  s_kb = lv_keyboard_create(scr);
  lv_obj_set_size(s_kb, 320, 140);
  lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_kb, s_ta);
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_kb, kb_close_cb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb, kb_close_cb, LV_EVENT_CANCEL, NULL);

  s_dirty = false;
  lv_timer_t* timer = lv_timer_create(autosave_tick_cb, 1000, NULL);
  chromeBindTimerToScreen(scr, timer);

  if (!g_sdReady && !g_ffatReady) {
    chromeShowToast("Storage tidak terdeteksi, catatan tidak tersimpan");
  }
}
