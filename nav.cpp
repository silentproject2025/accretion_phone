#include "nav.h"
#include <lvgl.h>

#define NAV_MAX 8
static ScreenShowFn s_stack[NAV_MAX];
static int s_depth = 0;

void navInit(ScreenShowFn homeShowFn) {
  s_stack[0] = homeShowFn;
  s_depth = 0;
}

void navPush(ScreenShowFn showFn) {
  lv_obj_t* prev = lv_scr_act();
  if (s_depth < NAV_MAX - 1) s_stack[++s_depth] = showFn;
  showFn(); // fungsi ini yg manggil lv_scr_load(scrBaru) di dalamnya
  if (prev) lv_obj_del_async(prev);
}

void navBack() {
  lv_obj_t* prev = lv_scr_act();
  if (s_depth > 0) s_depth--;
  s_stack[s_depth]();
  if (prev) lv_obj_del_async(prev);
}

void navGoHome() {
  lv_obj_t* prev = lv_scr_act();
  s_depth = 0;
  s_stack[0]();
  if (prev) lv_obj_del_async(prev);
}
