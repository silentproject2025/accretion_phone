#include "screen_chrome.h"
#include "status_bar.h"
#include "theme.h"
#include "nav.h"

static lv_obj_t* s_toast = nullptr;

static void toast_del_cb(lv_anim_t* a) {
  lv_obj_del((lv_obj_t*)a->var);
  if ((lv_obj_t*)a->var == s_toast) s_toast = nullptr;
}
// Wrapper opasitas generik -- lv_anim_exec_xcb_t cuma nembak 2 argumen
// (var,value), sedangkan lv_obj_set_style_opa() butuh 3 (obj,value,selector);
// cast langsung ke tipe callback anim itu gak aman (selector gak keisi bener).
static void toast_opa_anim_cb(void* obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

void chromeShowToast(const char* text) {
  if (s_toast) lv_obj_del(s_toast);
  lv_obj_t* scr = lv_scr_act();
  s_toast = lv_label_create(scr);
  lv_label_set_text(s_toast, text);
  lv_obj_set_style_bg_color(s_toast, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(s_toast, LV_OPA_90, 0);
  lv_obj_set_style_text_color(s_toast, c565(T().text), 0);
  lv_obj_set_style_pad_all(s_toast, 8, 0);
  lv_obj_set_style_radius(s_toast, 10, 0);
  lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -56);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_toast);
  lv_anim_set_exec_cb(&a, toast_opa_anim_cb);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&a, 400);
  lv_anim_set_delay(&a, 1200);
  lv_anim_set_ready_cb(&a, toast_del_cb);
  lv_anim_start(&a);
}

static void back_click_cb(lv_event_t* e) {
  navBack();
}

static void timer_autodel_cb(lv_event_t* e) {
  lv_timer_del((lv_timer_t*)lv_event_get_user_data(e));
}

void chromeBindTimerToScreen(lv_obj_t* scr, lv_timer_t* timer) {
  lv_obj_add_event_cb(scr, timer_autodel_cb, LV_EVENT_DELETE, timer);
}

lv_obj_t* chromeCreateAppScreen(const char* title) {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, c565(T().bg), 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_scr_load(scr);

  statusBarCreate(scr);

  lv_obj_t* titleLbl = lv_label_create(scr);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_color(titleLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(titleLbl, 8, 26); // persis di bawah status bar (STATUS_H=22)

  // Tombol back -- posisi/ukuran identik BACK_W=62,BACK_H=24,backX()=4,
  // backY()=SCR_H-BACK_H-3 di file lama (SCR_H=240 landscape).
  lv_obj_t* backBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(backBtn);
  lv_obj_set_size(backBtn, 62, 24);
  lv_obj_set_pos(backBtn, 4, 240 - 24 - 3);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_bg_color(backBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(backBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(backBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(backBtn, back_click_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t* backLbl = lv_label_create(backBtn);
  lv_label_set_text(backLbl, "< Back");
  lv_obj_set_style_text_color(backLbl, c565(T().accent), 0);
  lv_obj_center(backLbl);

  return scr;
}
