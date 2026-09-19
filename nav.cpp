#include "nav.h"
#include <lvgl.h>

#define NAV_MAX 8
static ScreenShowFn s_stack[NAV_MAX];
static int s_depth = 0;

void navInit(ScreenShowFn homeShowFn) {
  s_stack[0] = homeShowFn;
  s_depth = 0;
}

// CATATAN: gak ada lagi lv_obj_del_async(prev) manual di sini. Setiap
// show()-fn (lewat chromeCreateAppScreen()/homeScreenShow()) motong layar
// pakai lv_scr_load_anim(..., auto_del=true) -- LVGL SENDIRI yg hapus
// layar lama stlh animasi fade-nya kelar. Kalau di sini JUGA dihapus
// manual, itu dobel-hapus = use-after-free/crash.
void navPush(ScreenShowFn showFn) {
  if (s_depth < NAV_MAX - 1) s_stack[++s_depth] = showFn;
  showFn();
}

void navBack() {
  if (s_depth > 0) s_depth--;
  s_stack[s_depth]();
}

void navGoHome() {
  s_depth = 0;
  s_stack[0]();
}
