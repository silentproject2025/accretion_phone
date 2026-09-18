#include "app_calculator.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>

// Indeks tombol di dalam lv_btnmatrix (dihitung dari 0, "\n" tidak dihitung):
//  0:C  1:+/-  2:%   3:/
//  4:7  5:8    6:9   7:x
//  8:4  9:5   10:6  11:-
// 12:1 13:2   14:3  15:+
// 16:0 17:.   18:=  (lebar "=" di-set 2x biar center-align rapi)
static const char* calc_map[] = {
  "C", "+/-", "%", "/", "\n",
  "7", "8", "9", "x", "\n",
  "4", "5", "6", "-", "\n",
  "1", "2", "3", "+", "\n",
  "0", ".", "=", ""
};

static String s_input = "0";
static float  s_a = 0;
static char   s_op = 0;
static bool   s_isNew = true;

static lv_obj_t* s_display;

static void updateDisplay() {
  lv_label_set_text(s_display, s_input.c_str());
}

static void applyLabel(const char* l) {
  if (!strcmp(l, "C")) {
    s_input = "0"; s_a = 0; s_op = 0; s_isNew = true;
  } else if (!strcmp(l, "+/-")) {
    s_input = String(s_input.toFloat() * -1);
  } else if (!strcmp(l, "%")) {
    s_input = String(s_input.toFloat() / 100);
  } else if (!strcmp(l, "=")) {
    float b = s_input.toFloat(), res = 0;
    if (s_op == '+') res = s_a + b;
    else if (s_op == '-') res = s_a - b;
    else if (s_op == 'x') res = s_a * b;
    else if (s_op == '/') res = b ? s_a / b : 0;
    s_input = (res == (int)res) ? String((int)res) : String(res, 4);
    s_op = 0; s_isNew = true;
  } else if (!strcmp(l, "+") || !strcmp(l, "-") || !strcmp(l, "x") || !strcmp(l, "/")) {
    s_a = s_input.toFloat(); s_op = l[0]; s_isNew = true;
  } else {
    if (s_isNew) { s_input = ""; s_isNew = false; }
    if (!strcmp(l, ".") && s_input.indexOf('.') >= 0) return; // gak dobel titik
    if (s_input == "0" && strcmp(l, ".") != 0) s_input = "";
    s_input += l;
  }
  if (s_input.length() > 12) s_input = s_input.substring(0, 12);
  updateDisplay();
}

static void btnmatrix_event_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  uint32_t id = lv_btnmatrix_get_selected_btn(obj);
  const char* txt = lv_btnmatrix_get_btn_text(obj, id);
  if (txt) applyLabel(txt);
}

void calculatorScreenShow() {
  // Reset tiap kali app dibuka ulang, sama spt state fresh di file lama
  // stlh calcEnter()/calcApplyLabel("C") -- biar konsisten tiap masuk app.
  s_input = "0"; s_a = 0; s_op = 0; s_isNew = true;

  lv_obj_t* scr = chromeCreateAppScreen("Kalkulator");

  // Kotak display -- sama posisi spt fillRoundRect(4,STATUS_H+8,...) lama.
  lv_obj_t* dispBox = lv_obj_create(scr);
  lv_obj_remove_style_all(dispBox);
  lv_obj_set_pos(dispBox, 4, 30);
  lv_obj_set_size(dispBox, 320 - 8, 26);
  lv_obj_set_style_radius(dispBox, 4, 0);
  lv_obj_set_style_bg_color(dispBox, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(dispBox, LV_OPA_COVER, 0);

  s_display = lv_label_create(dispBox);
  lv_obj_set_style_text_color(s_display, c565(T().text), 0);
  lv_obj_set_style_text_font(s_display, &lv_font_montserrat_20, 0);
  lv_obj_align(s_display, LV_ALIGN_RIGHT_MID, -8, 0);
  updateDisplay();

  // Grid tombol -- top=STATUS_H+34(=56), bottom=backY()-6(=207), sama file lama.
  lv_obj_t* btnm = lv_btnmatrix_create(scr);
  lv_btnmatrix_set_map(btnm, calc_map);
  lv_btnmatrix_set_btn_width(btnm, 18, 2); // "=" selebar 2 kolom
  lv_obj_set_pos(btnm, 4, 60);
  lv_obj_set_size(btnm, 320 - 8, 207 - 60);
  lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btnm, 0, 0);
  lv_obj_set_style_pad_all(btnm, 0, 0);
  lv_obj_set_style_pad_row(btnm, 4, 0);
  lv_obj_set_style_pad_column(btnm, 4, 0);

  lv_obj_set_style_radius(btnm, 6, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(btnm, c565(T().surface), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(btnm, c565(T().text), LV_PART_ITEMS);
  lv_obj_set_style_text_font(btnm, &lv_font_montserrat_20, LV_PART_ITEMS);

  // Tombol operator (/,x,-,+) & "=" dikasih warna beda lewat control-flag
  // custom LVGL (LV_BTNMATRIX_CTRL_CUSTOM_1/2 + LV_STATE_USER_1/2) --
  // pola resmi LVGL utk styling per-tombol beda di dalam satu btnmatrix.
  static lv_style_t styleOp, styleEq;
  lv_style_init(&styleOp);
  lv_style_set_bg_color(&styleOp, c565(T().accent));
  lv_style_set_text_color(&styleOp, c565(T().bg));
  lv_obj_add_style(btnm, &styleOp, LV_PART_ITEMS | LV_STATE_USER_1);

  lv_style_init(&styleEq);
  lv_style_set_bg_color(&styleEq, c565(T().accent2));
  lv_style_set_text_color(&styleEq, c565(T().bg));
  lv_obj_add_style(btnm, &styleEq, LV_PART_ITEMS | LV_STATE_USER_2);

  int opIdx[4] = {3, 7, 11, 15}; // "/","x","-","+"
  for (int i = 0; i < 4; i++) lv_btnmatrix_set_btn_ctrl(btnm, opIdx[i], LV_BTNMATRIX_CTRL_CUSTOM_1);
  lv_btnmatrix_set_btn_ctrl(btnm, 18, LV_BTNMATRIX_CTRL_CUSTOM_2); // "="

  lv_obj_add_event_cb(btnm, btnmatrix_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
