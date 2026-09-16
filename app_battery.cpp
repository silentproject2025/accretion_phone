#include "app_battery.h"
#include "screen_chrome.h"
#include "theme.h"
#include "sys_state.h"
#include <Arduino.h>

#define BATTERY_CAPACITY_MAH 2300 // sama spt file lama

// Catatan konsolidasi vs file lama: baris "Kapasitas baterai" & "Estimasi
// sisa daya" digabung jadi SATU baris di sini ("2300 mAh (~N mAh tersisa)")
// -- bukan ngilangin info, cuma dipadatkan biar muat rapi di layar 240px
// landscape (rownya dulu 5 baris + catatan panjang, kepepet kalo dipisah
// semua + gak di-scroll). Seluruh konten bisa di-scroll kalau ada yg
// kepotong di layar HP kalian.

static lv_obj_t* s_fillBar;
static lv_obj_t* s_pctLabel;
static lv_obj_t* s_voltRow;
static lv_obj_t* s_capRow;
static lv_obj_t* s_statusRow;

static uint16_t battColorFor(int pct) {
  if (pct <= 15) return T().danger;
  if (pct <= 35) return T().accent;
  return T().good;
}

static const char* battStatusText(int pct) {
  if (pct <= 5)  return "Kritis, segera cas!";
  if (pct <= 15) return "Lemah, segera cas";
  if (pct <= 35) return "Cukup";
  return "Baik";
}

static void refreshBatteryUI() {
  int pct = g_battPercent;
  uint16_t col = battColorFor(pct);

  int innerW = 80; // iconW(90)-innerPad(5)*2
  int fillW = constrain((innerW * pct) / 100, 0, innerW);
  lv_obj_set_width(s_fillBar, fillW > 0 ? fillW : 1);
  lv_obj_set_style_bg_opa(s_fillBar, fillW > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(s_fillBar, c565(col), 0);

  char pctBuf[8]; snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  lv_label_set_text(s_pctLabel, pctBuf);
  lv_obj_set_style_text_color(s_pctLabel, c565(col), 0);

  char vb[24]; snprintf(vb, sizeof(vb), "%.2f V", g_battVoltage);
  lv_label_set_text(s_voltRow, vb);

  int remainMah = (int)(BATTERY_CAPACITY_MAH * pct / 100.0f);
  char cb[40]; snprintf(cb, sizeof(cb), "%d mAh (~%d mAh tersisa)", BATTERY_CAPACITY_MAH, remainMah);
  lv_label_set_text(s_capRow, cb);

  lv_label_set_text(s_statusRow, battStatusText(pct));
  lv_obj_set_style_text_color(s_statusRow, c565(col), 0);
}

static void refresh_btn_cb(lv_event_t* e) {
  sysUpdateBattery(); // TODO: masih no-op sampai ADC asli di-port
  refreshBatteryUI();
  chromeShowToast("Data baterai diperbarui");
}

// Baris "label ... value" full-width -- dipakai 3x (voltase/kapasitas/status).
static lv_obj_t* buildDetailRow(lv_obj_t* parent, const char* label, lv_obj_t** valueLblOut) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* l = lv_label_create(row);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_color(l, c565(T().subtext), 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);

  lv_obj_t* v = lv_label_create(row);
  lv_obj_set_style_text_color(v, c565(T().text), 0);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
  *valueLblOut = v;
  return row;
}

void batteryScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Baterai");

  // Tombol Refresh -- posisi identik file lama (kanan atas, dekat judul).
  lv_obj_t* refreshBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(refreshBtn);
  lv_obj_set_size(refreshBtn, 64, 18);
  lv_obj_set_pos(refreshBtn, 320 - 70, 24);
  lv_obj_set_style_radius(refreshBtn, 4, 0);
  lv_obj_set_style_bg_color(refreshBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(refreshBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(refreshBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(refreshBtn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* refreshLbl = lv_label_create(refreshBtn);
  lv_label_set_text(refreshLbl, "Refresh");
  lv_obj_set_style_text_color(refreshLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(refreshLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(refreshLbl);

  // Konten bisa di-scroll -- biar aman kalau catatan di bawah kepanjangan
  // di layar 240px landscape.
  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 44);
  lv_obj_set_size(content, 320, 207 - 44);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 6, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  // --- Ikon baterai besar ---
  lv_obj_t* iconWrap = lv_obj_create(content);
  lv_obj_remove_style_all(iconWrap);
  lv_obj_set_size(iconWrap, 100, 44);

  lv_obj_t* outline = lv_obj_create(iconWrap);
  lv_obj_remove_style_all(outline);
  lv_obj_set_pos(outline, 0, 0);
  lv_obj_set_size(outline, 90, 44);
  lv_obj_set_style_radius(outline, 6, 0);
  lv_obj_set_style_border_width(outline, 2, 0);
  lv_obj_set_style_border_color(outline, c565(T().text), 0);
  lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);

  lv_obj_t* nub = lv_obj_create(iconWrap);
  lv_obj_remove_style_all(nub);
  lv_obj_set_pos(nub, 90, 44 / 2 - 7);
  lv_obj_set_size(nub, 7, 14);
  lv_obj_set_style_radius(nub, 3, 0);
  lv_obj_set_style_bg_color(nub, c565(T().text), 0);
  lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);

  s_fillBar = lv_obj_create(outline);
  lv_obj_remove_style_all(s_fillBar);
  lv_obj_set_pos(s_fillBar, 5, 5);
  lv_obj_set_height(s_fillBar, 34);
  lv_obj_set_style_radius(s_fillBar, 3, 0);

  // --- Persen besar ---
  s_pctLabel = lv_label_create(content);
  lv_obj_set_style_text_font(s_pctLabel, &lv_font_montserrat_28, 0);

  // --- Detail: voltase, kapasitas+sisa, status ---
  lv_obj_t* rows = lv_obj_create(content);
  lv_obj_remove_style_all(rows);
  lv_obj_set_size(rows, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(rows, 4, 0);

  buildDetailRow(rows, "Tegangan saat ini:", &s_voltRow);
  buildDetailRow(rows, "Kapasitas baterai:", &s_capRow);
  buildDetailRow(rows, "Status:", &s_statusRow);

  // Garis pemisah + catatan (wrap) -- persis isi teks di file lama.
  lv_obj_t* divider = lv_obj_create(content);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, 300, 1);
  lv_obj_set_style_bg_color(divider, c565(T().divider), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

  lv_obj_t* note = lv_label_create(content);
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(note, 300);
  lv_label_set_text(note,
    "Catatan: estimasi dihitung dari pembacaan tegangan (voltage divider), "
    "bukan fuel-gauge IC, jadi persen & sisa mAh di atas perkiraan kasar, "
    "bukan angka presisi.");
  lv_obj_set_style_text_color(note, c565(T().subtext), 0);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);

  refreshBatteryUI();
}
