#include "app_hwmonitor.h"
#include "screen_chrome.h"
#include "theme.h"
#include "sys_state.h"
#include <Arduino.h>
#include <esp_freertos_hooks.h>
#include <esp_heap_caps.h>

#define HWMON_HIST_LEN 56 // sama panjang histori spt file lama

// =============================================
// Idle hook -- CARA HITUNG CPU LOAD PER CORE, disalin persis metodenya dari
// file lama: tiap kali task IDLE suatu core sempat kebagian giliran jalan
// (core lagi nganggur), hook ini nambah counter. Baseline "100% nganggur"
// dikalibrasi otomatis dari nilai TERTINGGI yg pernah terekam per jendela
// sampel (bukan tabel konstanta manual), jadi otomatis pas apapun revisi
// chip/IDF-nya.
// =============================================
static volatile uint32_t s_idleHits0 = 0, s_idleHits1 = 0;
static bool s_hooksRegistered = false;

static bool idleHook0() { s_idleHits0++; return false; }
static bool idleHook1() { s_idleHits1++; return false; }

void hwmonInitHooks() {
  if (s_hooksRegistered) return;
  esp_register_freertos_idle_hook_for_cpu(idleHook0, 0);
  esp_register_freertos_idle_hook_for_cpu(idleHook1, 1);
  s_hooksRegistered = true;
}

static uint32_t s_baseline0 = 1, s_baseline1 = 1;
static int s_usage0 = 0, s_usage1 = 0;
static int s_min0 = 100, s_max0 = 0, s_min1 = 100, s_max1 = 0;

static lv_obj_t* s_pctLbl0; static lv_obj_t* s_bar0; static lv_obj_t* s_chart0; static lv_chart_series_t* s_ser0; static lv_obj_t* s_mmLbl0;
static lv_obj_t* s_pctLbl1; static lv_obj_t* s_bar1; static lv_obj_t* s_chart1; static lv_chart_series_t* s_ser1; static lv_obj_t* s_mmLbl1;
static lv_obj_t* s_ramPctLbl; static lv_obj_t* s_ramBar; static lv_obj_t* s_ramKbLbl;
static lv_obj_t* s_blockValLbl; static lv_obj_t* s_blockWarnLbl;
static lv_obj_t* s_psramRow; static lv_obj_t* s_psramPctLbl; static lv_obj_t* s_psramBar; static lv_obj_t* s_psramKbLbl; static lv_obj_t* s_psramNoneLbl;
static lv_obj_t* s_infoLbl;

static uint16_t loadColor(int pct) {
  if (pct >= 85) return T().danger;
  if (pct >= 55) return T().accent;
  return T().good;
}

static void resetStats() {
  s_baseline0 = 1; s_baseline1 = 1;
  s_min0 = 100; s_max0 = 0; s_min1 = 100; s_max1 = 0;
  s_idleHits0 = 0; s_idleHits1 = 0;
  lv_chart_set_all_value(s_chart0, s_ser0, 0);
  lv_chart_set_all_value(s_chart1, s_ser1, 0);
}

static void reset_btn_cb(lv_event_t* e) {
  resetStats();
  chromeShowToast("Statistik min/maks direset");
}

// Satu blok "Core N": label+pct, bar tipis, grafik histori, teks min/maks.
static void buildCoreBlock(lv_obj_t* parent, const char* label,
                            lv_obj_t** pctLblOut, lv_obj_t** barOut,
                            lv_obj_t** chartOut, lv_chart_series_t** serOut,
                            lv_obj_t** mmLblOut) {
  lv_obj_t* block = lv_obj_create(parent);
  lv_obj_remove_style_all(block);
  lv_obj_set_size(block, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(block, 2, 0);

  lv_obj_t* head = lv_obj_create(block);
  lv_obj_remove_style_all(head);
  lv_obj_set_size(head, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* nameLbl = lv_label_create(head);
  lv_label_set_text(nameLbl, label);
  lv_obj_set_style_text_color(nameLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);

  *pctLblOut = lv_label_create(head);
  lv_obj_set_style_text_font(*pctLblOut, &lv_font_montserrat_14, 0);

  *barOut = lv_bar_create(block);
  lv_obj_set_size(*barOut, 300, 10);
  lv_bar_set_range(*barOut, 0, 100);
  lv_obj_set_style_bg_color(*barOut, c565(T().divider), LV_PART_MAIN);
  lv_obj_set_style_bg_color(*barOut, c565(T().good), LV_PART_INDICATOR);

  *chartOut = lv_chart_create(block);
  lv_obj_set_size(*chartOut, 300, 34);
  lv_chart_set_type(*chartOut, LV_CHART_TYPE_LINE);
  lv_chart_set_range(*chartOut, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_point_count(*chartOut, HWMON_HIST_LEN);
  lv_chart_set_update_mode(*chartOut, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_size(*chartOut, 0, LV_PART_INDICATOR); // sembunyikan titik marker
  lv_obj_set_style_bg_color(*chartOut, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(*chartOut, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(*chartOut, c565(T().divider), LV_PART_MAIN);
  *serOut = lv_chart_add_series(*chartOut, lv_color_make(0, 255, 0), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(*chartOut, *serOut, 0);

  *mmLblOut = lv_label_create(block);
  lv_obj_set_style_text_color(*mmLblOut, c565(T().subtext), 0);
  lv_obj_set_style_text_font(*mmLblOut, &lv_font_montserrat_14, 0);
}

static void poll_cb(lv_timer_t* t) {
  uint32_t c0 = s_idleHits0; s_idleHits0 = 0;
  uint32_t c1 = s_idleHits1; s_idleHits1 = 0;

  if (c0 > s_baseline0) s_baseline0 = c0;
  if (c1 > s_baseline1) s_baseline1 = c1;

  s_usage0 = constrain(100 - (int)((c0 * 100UL) / s_baseline0), 0, 100);
  s_usage1 = constrain(100 - (int)((c1 * 100UL) / s_baseline1), 0, 100);
  if (s_usage0 < s_min0) s_min0 = s_usage0;
  if (s_usage0 > s_max0) s_max0 = s_usage0;
  if (s_usage1 < s_min1) s_min1 = s_usage1;
  if (s_usage1 > s_max1) s_max1 = s_usage1;

  char pb[8];
  snprintf(pb, sizeof(pb), "%d%%", s_usage0);
  lv_label_set_text(s_pctLbl0, pb);
  lv_obj_set_style_text_color(s_pctLbl0, c565(loadColor(s_usage0)), 0);
  lv_bar_set_value(s_bar0, s_usage0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar0, c565(loadColor(s_usage0)), LV_PART_INDICATOR);
  lv_chart_set_next_value(s_chart0, s_ser0, s_usage0);
  char mm0[32]; snprintf(mm0, sizeof(mm0), "min %d%%  maks %d%%", s_min0, s_max0);
  lv_label_set_text(s_mmLbl0, mm0);

  snprintf(pb, sizeof(pb), "%d%%", s_usage1);
  lv_label_set_text(s_pctLbl1, pb);
  lv_obj_set_style_text_color(s_pctLbl1, c565(loadColor(s_usage1)), 0);
  lv_bar_set_value(s_bar1, s_usage1, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_bar1, c565(loadColor(s_usage1)), LV_PART_INDICATOR);
  lv_chart_set_next_value(s_chart1, s_ser1, s_usage1);
  char mm1[32]; snprintf(mm1, sizeof(mm1), "min %d%%  maks %d%%", s_min1, s_max1);
  lv_label_set_text(s_mmLbl1, mm1);

  size_t heapTotal = ESP.getHeapSize();
  size_t heapFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t heapLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  int ramPct = heapTotal > 0 ? (int)(100 - (heapFree * 100UL / heapTotal)) : 0;
  char rb[40];
  snprintf(rb, sizeof(rb), "%d%%", ramPct);
  lv_label_set_text(s_ramPctLbl, rb);
  lv_obj_set_style_text_color(s_ramPctLbl, c565(loadColor(ramPct)), 0);
  lv_bar_set_value(s_ramBar, ramPct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_ramBar, c565(loadColor(ramPct)), LV_PART_INDICATOR);
  snprintf(rb, sizeof(rb), "%u / %u KB", (unsigned)((heapTotal - heapFree) / 1024), (unsigned)(heapTotal / 1024));
  lv_label_set_text(s_ramKbLbl, rb);

  bool lbLow = heapLargest < 20000;
  char lb[24]; snprintf(lb, sizeof(lb), "%u KB", (unsigned)(heapLargest / 1024));
  lv_label_set_text(s_blockValLbl, lb);
  lv_obj_set_style_text_color(s_blockValLbl, c565(lbLow ? T().danger : T().good), 0);
  if (lbLow) lv_obj_clear_flag(s_blockWarnLbl, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s_blockWarnLbl, LV_OBJ_FLAG_HIDDEN);

  size_t psramTotal = ESP.getPsramSize();
  if (psramTotal > 0) {
    size_t psramFree = ESP.getFreePsram();
    int psPct = (int)(100 - (psramFree * 100UL / psramTotal));
    lv_obj_clear_flag(s_psramRow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_psramNoneLbl, LV_OBJ_FLAG_HIDDEN);
    char pp[8]; snprintf(pp, sizeof(pp), "%d%%", psPct);
    lv_label_set_text(s_psramPctLbl, pp);
    lv_obj_set_style_text_color(s_psramPctLbl, c565(loadColor(psPct)), 0);
    lv_bar_set_value(s_psramBar, psPct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_psramBar, c565(loadColor(psPct)), LV_PART_INDICATOR);
    char pk[40]; snprintf(pk, sizeof(pk), "%u / %u KB", (unsigned)((psramTotal - psramFree) / 1024), (unsigned)(psramTotal / 1024));
    lv_label_set_text(s_psramKbLbl, pk);
  } else {
    lv_obj_add_flag(s_psramRow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_psramNoneLbl, LV_OBJ_FLAG_HIDDEN);
  }

  unsigned long upS = millis() / 1000;
  int uh = upS / 3600, um = (upS % 3600) / 60, us = upS % 60;
  char infoBuf[96];
  snprintf(infoBuf, sizeof(infoBuf), "CPU %dMHz | Suhu %.1fC | Uptime %02d:%02d:%02d | Bat %d%%",
           ESP.getCpuFreqMHz(), temperatureRead(), uh, um, us, g_battPercent);
  lv_label_set_text(s_infoLbl, infoBuf);
}

void hwmonitorScreenShow() {
  hwmonInitHooks(); // jaga2 kalau belum dipanggil dari setup()
  s_min0 = 100; s_max0 = 0; s_min1 = 100; s_max1 = 0;
  s_idleHits0 = 0; s_idleHits1 = 0;

  lv_obj_t* scr = chromeCreateAppScreen("HWmonitor");

  // Tombol Reset Min/Max -- posisi mirip file lama (kanan atas).
  lv_obj_t* resetBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(resetBtn);
  lv_obj_set_size(resetBtn, 100, 18);
  lv_obj_set_pos(resetBtn, 320 - 108, 24);
  lv_obj_set_style_radius(resetBtn, 4, 0);
  lv_obj_set_style_bg_color(resetBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(resetBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(resetBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(resetBtn, reset_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* resetLbl = lv_label_create(resetBtn);
  lv_label_set_text(resetLbl, "Reset Min/Max");
  lv_obj_set_style_text_color(resetLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(resetLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(resetLbl);

  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 48);
  lv_obj_set_size(content, 320, 207 - 48);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 6, 0);
  lv_obj_set_style_pad_row(content, 8, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  buildCoreBlock(content, "Core 0", &s_pctLbl0, &s_bar0, &s_chart0, &s_ser0, &s_mmLbl0);
  buildCoreBlock(content, "Core 1", &s_pctLbl1, &s_bar1, &s_chart1, &s_ser1, &s_mmLbl1);

  // --- RAM Internal ---
  lv_obj_t* ramHead = lv_obj_create(content);
  lv_obj_remove_style_all(ramHead);
  lv_obj_set_size(ramHead, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ramHead, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ramHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* ramNameLbl = lv_label_create(ramHead);
  lv_label_set_text(ramNameLbl, "RAM Internal");
  lv_obj_set_style_text_color(ramNameLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(ramNameLbl, &lv_font_montserrat_14, 0);
  s_ramKbLbl = lv_label_create(ramHead);
  lv_obj_set_style_text_color(s_ramKbLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_ramKbLbl, &lv_font_montserrat_14, 0);

  s_ramPctLbl = lv_label_create(content);
  lv_obj_set_style_text_font(s_ramPctLbl, &lv_font_montserrat_14, 0);
  s_ramBar = lv_bar_create(content);
  lv_obj_set_size(s_ramBar, 300, 12);
  lv_bar_set_range(s_ramBar, 0, 100);
  lv_obj_set_style_bg_color(s_ramBar, c565(T().divider), LV_PART_MAIN);

  // --- Blok Terbesar ---
  lv_obj_t* blockHead = lv_obj_create(content);
  lv_obj_remove_style_all(blockHead);
  lv_obj_set_size(blockHead, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(blockHead, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(blockHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* blockNameLbl = lv_label_create(blockHead);
  lv_label_set_text(blockNameLbl, "Blok Terbesar");
  lv_obj_set_style_text_color(blockNameLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(blockNameLbl, &lv_font_montserrat_14, 0);
  s_blockValLbl = lv_label_create(blockHead);
  lv_obj_set_style_text_font(s_blockValLbl, &lv_font_montserrat_14, 0);

  s_blockWarnLbl = lv_label_create(content);
  lv_label_set_text(s_blockWarnLbl, "(<20KB -> app internet bs gagal HTTP -1)");
  lv_obj_set_style_text_color(s_blockWarnLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_blockWarnLbl, &lv_font_montserrat_14, 0);

  // --- PSRAM ---
  s_psramRow = lv_obj_create(content);
  lv_obj_remove_style_all(s_psramRow);
  lv_obj_set_size(s_psramRow, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(s_psramRow, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* psramHead = lv_obj_create(s_psramRow);
  lv_obj_remove_style_all(psramHead);
  lv_obj_set_size(psramHead, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(psramHead, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(psramHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* psramNameLbl = lv_label_create(psramHead);
  lv_label_set_text(psramNameLbl, "PSRAM");
  lv_obj_set_style_text_color(psramNameLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(psramNameLbl, &lv_font_montserrat_14, 0);
  s_psramKbLbl = lv_label_create(psramHead);
  lv_obj_set_style_text_color(s_psramKbLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_psramKbLbl, &lv_font_montserrat_14, 0);
  s_psramPctLbl = lv_label_create(s_psramRow);
  lv_obj_set_style_text_font(s_psramPctLbl, &lv_font_montserrat_14, 0);
  s_psramBar = lv_bar_create(s_psramRow);
  lv_obj_set_size(s_psramBar, 300, 12);
  lv_bar_set_range(s_psramBar, 0, 100);
  lv_obj_set_style_bg_color(s_psramBar, c565(T().divider), LV_PART_MAIN);

  s_psramNoneLbl = lv_label_create(content);
  lv_label_set_text(s_psramNoneLbl, "PSRAM: tidak terdeteksi");
  lv_obj_set_style_text_color(s_psramNoneLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_psramNoneLbl, &lv_font_montserrat_14, 0);
  lv_obj_add_flag(s_psramNoneLbl, LV_OBJ_FLAG_HIDDEN);

  // --- Divider + info chip ---
  lv_obj_t* divider = lv_obj_create(content);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, 300, 1);
  lv_obj_set_style_bg_color(divider, c565(T().divider), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

  s_infoLbl = lv_label_create(content);
  lv_label_set_long_mode(s_infoLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_infoLbl, 300);
  lv_obj_set_style_text_color(s_infoLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_infoLbl, &lv_font_montserrat_14, 0);

  poll_cb(NULL);
  lv_timer_t* timer = lv_timer_create(poll_cb, 500, NULL); // sama cadence file lama
  chromeBindTimerToScreen(scr, timer);
}
