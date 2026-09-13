#include "status_bar.h"
#include "screen_chrome.h"
#include "theme.h"
#include "sys_state.h"
#include <Arduino.h>
#include <time.h>

struct StatusBarLabels {
  lv_obj_t* time;
  lv_obj_t* wifi;
  lv_obj_t* batt;
};

static void status_tick_cb(lv_timer_t* t) {
  StatusBarLabels* L = (StatusBarLabels*)t->user_data;
  struct tm ti;
  bool ok = g_ntpSynced && getLocalTime(&ti);
  char tb[6];
  if (ok) snprintf(tb, sizeof(tb), "%02d:%02d", ti.tm_hour, ti.tm_min);
  else strcpy(tb, "--:--");
  lv_label_set_text(L->time, tb);

  lv_label_set_text(L->wifi, g_wifiConnected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
  char pb[6]; snprintf(pb, sizeof(pb), "%d%%", g_battPercent);
  lv_label_set_text(L->batt, pb);
}

static void status_labels_free_cb(lv_event_t* e) {
  delete (StatusBarLabels*)lv_event_get_user_data(e);
}

void statusBarCreate(lv_obj_t* parent) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, LV_PCT(100), 22); // STATUS_H=22 di file lama
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(bar, 8, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  StatusBarLabels* L = new StatusBarLabels();
  L->time = lv_label_create(bar);
  lv_obj_set_style_text_color(L->time, c565(T().text), 0);
  lv_obj_set_style_text_font(L->time, &lv_font_montserrat_14, 0);

  lv_obj_t* rightWrap = lv_obj_create(bar);
  lv_obj_remove_style_all(rightWrap);
  lv_obj_set_flex_flow(rightWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rightWrap, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(rightWrap, 6, 0);
  lv_obj_set_size(rightWrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_clear_flag(rightWrap, LV_OBJ_FLAG_SCROLLABLE);

  L->wifi = lv_label_create(rightWrap);
  lv_obj_set_style_text_color(L->wifi, c565(T().subtext), 0);
  L->batt = lv_label_create(rightWrap);
  lv_obj_set_style_text_color(L->batt, c565(T().subtext), 0);

  lv_timer_t* timer = lv_timer_create(status_tick_cb, 1000, L);
  status_tick_cb(timer);
  chromeBindTimerToScreen(parent, timer);

  // `L` sengaja di-`new` (bukan static) krn tiap statusBarCreate() dipanggil
  // ulang (tiap ganti layar) butuh instance baru -- dibersihkan bareng
  // timer-nya lewat event delete parent.
  lv_obj_add_event_cb(parent, status_labels_free_cb, LV_EVENT_DELETE, L);
}
