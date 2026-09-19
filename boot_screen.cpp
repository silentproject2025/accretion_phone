#include "boot_screen.h"
#include "theme.h"
#include <lvgl.h>

// Palet boot -- sama persis dgn BOOT_ACCENT/BOOT_GLOW/BOOT_BG_* di file lama.
static uint16_t BOOT_ACCENT = 0xFFFF;
static uint16_t BOOT_GLOW   = 0x39C7;
static uint16_t BOOT_BG_TOP = 0x0841;
static uint16_t BOOT_BG_BOT = 0x0000;

static void (*s_onDone)() = nullptr;
static lv_obj_t* s_dots[3];

static void dot_pulse_anim_cb(void* obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}
// Wrapper opasitas generik -- lv_obj_set_style_opa() aslinya butuh 3 argumen
// (obj, value, selector), sedangkan lv_anim_exec_xcb_t cuma nembak 2
// (var, value). Cast langsung ke tipe callback anim itu UB/gak aman krn
// argumen ke-3 (selector) gak pernah keisi dgn benar -- makanya dibungkus
// fungsi kecil di sini, sama seperti dot_pulse_anim_cb di atas.
static void opa_anim_cb(void* obj, int32_t v) {
  lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}

static void finish_timer_cb(lv_timer_t* t) {
  lv_timer_del(t);
  if (s_onDone) s_onDone();
}

void bootScreenShow(void (*onDone)()) {
  s_onDone = onDone;

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, c565(BOOT_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color(scr, c565(BOOT_BG_BOT), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  // Fade dari layar default LVGL (kosong) -- auto_del=true otomatis
  // hapus layar default itu, aman krn objeknya emang gak dipakai lagi.
  lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);

  // Halo lembut di belakang logo -- didekati pakai shadow blur (LVGL gak
  // punya radial-gradient asli), cukup mirip efek glow di versi lama.
  lv_obj_t* halo = lv_obj_create(scr);
  lv_obj_set_size(halo, 10, 10);
  lv_obj_center(halo);
  lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(halo, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(halo, 0, 0);
  lv_obj_set_style_shadow_width(halo, 80, 0);
  lv_obj_set_style_shadow_spread(halo, 30, 0);
  lv_obj_set_style_shadow_color(halo, c565(BOOT_GLOW), 0);
  lv_obj_set_style_shadow_opa(halo, LV_OPA_60, 0);

  // Wordmark "NYXOS"
  lv_obj_t* logo = lv_label_create(scr);
  lv_label_set_text(logo, "NYXOS");
  lv_obj_set_style_text_color(logo, c565(BOOT_ACCENT), 0);
  lv_obj_set_style_text_font(logo, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_letter_space(logo, 6, 0);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, -6);
  lv_obj_set_style_opa(logo, LV_OPA_TRANSP, 0);

  // Tagline
  lv_obj_t* tag = lv_label_create(scr);
  lv_label_set_text(tag, "Beyond the Event Horizon");
  lv_obj_set_style_text_color(tag, c565(BOOT_GLOW), 0);
  lv_obj_set_style_text_font(tag, &lv_font_montserrat_14, 0);
  lv_obj_align(tag, LV_ALIGN_CENTER, 0, 26);
  lv_obj_set_style_opa(tag, LV_OPA_TRANSP, 0);

  // Fade+scale-in logo
  lv_anim_t a1;
  lv_anim_init(&a1);
  lv_anim_set_var(&a1, logo);
  lv_anim_set_exec_cb(&a1, opa_anim_cb);
  lv_anim_set_values(&a1, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_time(&a1, 700);
  lv_anim_set_delay(&a1, 150);
  lv_anim_start(&a1);

  // Fade-in tagline (nyusul stlh logo)
  lv_anim_t a2 = a1;
  lv_anim_set_var(&a2, tag);
  lv_anim_set_delay(&a2, 550);
  lv_anim_set_time(&a2, 500);
  lv_anim_start(&a2);

  // Loader: 3 titik kecil, berdenyut bergantian (spirit sama dgn "loader
  // titik berdenyut" di stage 2 boot lama).
  for (int i = 0; i < 3; i++) {
    lv_obj_t* d = lv_obj_create(scr);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 6, 6);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, c565(BOOT_ACCENT), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_align(d, LV_ALIGN_CENTER, -14 + i * 14, 60);
    s_dots[i] = d;

    lv_anim_t ad;
    lv_anim_init(&ad);
    lv_anim_set_var(&ad, d);
    lv_anim_set_exec_cb(&ad, dot_pulse_anim_cb);
    lv_anim_set_values(&ad, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&ad, 400);
    lv_anim_set_playback_time(&ad, 400);
    lv_anim_set_repeat_count(&ad, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_delay(&ad, i * 200);
    lv_anim_start(&ad);
  }

  // Total durasi boot ~2.2s, senada dengan versi lama, lalu pindah ke Home.
  lv_timer_create(finish_timer_cb, 2200, NULL);
}
