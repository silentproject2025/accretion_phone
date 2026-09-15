#include "app_clock.h"
#include "screen_chrome.h"
#include "theme.h"
#include "sys_state.h"
#include <Arduino.h>
#include <time.h>

static lv_obj_t* s_bigTime;
static lv_obj_t* s_dateLbl;
static lv_obj_t* s_statusLbl;

static void clock_tick_cb(lv_timer_t* t) {
  struct tm ti;
  bool ok = g_ntpSynced && getLocalTime(&ti);
  if (ok) {
    char tb[10];
    snprintf(tb, sizeof(tb), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    lv_label_set_text(s_bigTime, tb);

    static const char* days[] = {"Min","Sen","Sel","Rab","Kam","Jum","Sab"};
    static const char* mons[] = {"Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"};
    char db[32];
    snprintf(db, sizeof(db), "%s, %02d %s %04d", days[ti.tm_wday], ti.tm_mday, mons[ti.tm_mon], ti.tm_year + 1900);
    lv_label_set_text(s_dateLbl, db);

    lv_label_set_text(s_statusLbl, "NTP Sync OK");
    lv_obj_set_style_text_color(s_statusLbl, c565(T().good), 0);
  } else {
    lv_label_set_text(s_bigTime, "--:--:--");
    lv_label_set_text(s_dateLbl, "");
    lv_label_set_text(s_statusLbl, "Tidak sync");
    lv_obj_set_style_text_color(s_statusLbl, c565(T().danger), 0);
  }
}

void clockScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Jam");

  s_bigTime = lv_label_create(scr);
  lv_obj_set_style_text_color(s_bigTime, c565(T().text), 0);
  lv_obj_set_style_text_font(s_bigTime, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(s_bigTime, 20, 50);

  s_dateLbl = lv_label_create(scr);
  lv_obj_set_style_text_color(s_dateLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_dateLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(s_dateLbl, 20, 90);

  // Garis pemisah -- sama spt drawFastHLine(20,114,SCR_W-40,T().accent) lama.
  lv_obj_t* divider = lv_obj_create(scr);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, 320 - 40, 1);
  lv_obj_set_pos(divider, 20, 108);
  lv_obj_set_style_bg_color(divider, c565(T().accent), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

  s_statusLbl = lv_label_create(scr);
  lv_obj_set_style_text_font(s_statusLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(s_statusLbl, 20, 116);

  lv_timer_t* timer = lv_timer_create(clock_tick_cb, 1000, NULL);
  clock_tick_cb(timer);
  chromeBindTimerToScreen(scr, timer);
}
