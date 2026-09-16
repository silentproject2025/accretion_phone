#include "text_input.h"
#include "theme.h"
#include <lvgl.h>
#include <cstring>

static void (*s_onDone)(const char*) = nullptr;
static lv_obj_t* s_ta;
bool g_textInputActive = false;

static void kb_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);

  if (code == LV_EVENT_READY) {
    if (s_onDone) {
      char buf[128];
      strncpy(buf, lv_textarea_get_text(s_ta), sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = 0;
      s_onDone(buf);
    }
  }
  // READY maupun CANCEL sama2 nutup overlay -- bedanya cuma READY manggil
  // onDone duluan, CANCEL enggak.
  g_textInputActive = false;
  lv_obj_del_async(overlay);
}

void chromeShowTextInput(const char* title, const char* initialText,
                          bool isPassword, void (*onDone)(const char* text)) {
  s_onDone = onDone;
  g_textInputActive = true;

  lv_obj_t* overlay = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_pos(overlay, 0, 0);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, c565(T().bg), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* titleLbl = lv_label_create(overlay);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_color(titleLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_14, 0);
  lv_obj_align(titleLbl, LV_ALIGN_TOP_MID, 0, 6);

  s_ta = lv_textarea_create(overlay);
  lv_textarea_set_one_line(s_ta, true);
  lv_textarea_set_password_mode(s_ta, isPassword);
  lv_textarea_set_text(s_ta, initialText ? initialText : "");
  lv_obj_set_size(s_ta, 300, 32);
  lv_obj_align(s_ta, LV_ALIGN_TOP_MID, 0, 26);

  lv_obj_t* kb = lv_keyboard_create(overlay);
  lv_obj_set_size(kb, 320, 140);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, s_ta);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, overlay);
  lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, overlay);

  lv_obj_add_state(s_ta, LV_STATE_FOCUSED);
}
