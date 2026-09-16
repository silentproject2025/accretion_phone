#include "app_orientasi3d.h"
#include "screen_chrome.h"
#include "theme.h"
#include "mpu_sensor.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

#define CANVAS_W 280
#define CANVAS_H 100

struct Vec3 { float x, y, z; };

static lv_color_t* s_canvasBuf = nullptr; // dialokasikan sekali dari PSRAM, dipakai seumur hidup app ini
static lv_obj_t* s_canvas;
static lv_obj_t* s_roll1Lbl; static lv_obj_t* s_accelLbl; static lv_obj_t* s_gyroLbl; static lv_obj_t* s_tempLbl;

static Vec3 rot3(Vec3 v, float rxRad, float ryRad) {
  float y1 = v.y * cosf(rxRad) - v.z * sinf(rxRad);
  float z1 = v.y * sinf(rxRad) + v.z * cosf(rxRad);
  float x1 = v.x;
  float x2 =  x1 * cosf(ryRad) + z1 * sinf(ryRad);
  float z2 = -x1 * sinf(ryRad) + z1 * cosf(ryRad);
  return { x2, y1, z2 };
}

// Gambar ulang kotak wireframe ke canvas -- dipanggil tiap tick timer.
static void redrawBox() {
  lv_canvas_fill_bg(s_canvas, c565(T().bg), LV_OPA_COVER);

  float rx = g_smoothPitch * (PI / 180.0f);
  float ry = g_smoothRoll  * (PI / 180.0f);
  float w = 46, h = 78, d = 8; // proporsi disesuaikan biar pas di canvas 280x100
  Vec3 local[8] = {
    {-w,-h,-d},{w,-h,-d},{w,h,-d},{-w,h,-d},
    {-w,-h, d},{w,-h, d},{w,h, d},{-w,h, d}
  };
  float px[8], py[8];
  int cx = CANVAS_W / 2, cy = CANVAS_H / 2;
  for (int i = 0; i < 8; i++) {
    Vec3 r = rot3(local[i], rx, ry);
    float persp = 240.0f / (240.0f + r.z);
    px[i] = cx + r.x * persp;
    py[i] = cy - r.y * persp;
  }

  static const int edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
  };
  lv_draw_line_dsc_t lineDsc;
  lv_draw_line_dsc_init(&lineDsc);
  lineDsc.color = c565(T().accent);
  lineDsc.width = 2;
  lineDsc.round_end = 1; lineDsc.round_start = 1;
  for (int i = 0; i < 12; i++) {
    lv_point_t pts[2] = {
      { (lv_coord_t)px[edges[i][0]], (lv_coord_t)py[edges[i][0]] },
      { (lv_coord_t)px[edges[i][1]], (lv_coord_t)py[edges[i][1]] }
    };
    lv_canvas_draw_line(s_canvas, pts, 2, &lineDsc);
  }

  lv_draw_rect_dsc_t dotDsc;
  lv_draw_rect_dsc_init(&dotDsc);
  dotDsc.bg_color = c565(T().accent2);
  dotDsc.bg_opa = LV_OPA_COVER;
  dotDsc.radius = LV_RADIUS_CIRCLE;
  for (int i = 0; i < 8; i++) {
    lv_canvas_draw_rect(s_canvas, (int)px[i] - 3, (int)py[i] - 3, 6, 6, &dotDsc);
  }
}

static void poll_cb(lv_timer_t* t) {
  if (!g_mpuReady) return;
  redrawBox();
  char buf[64];
  snprintf(buf, sizeof(buf), "Roll: %+.1f  Pitch: %+.1f", g_smoothRoll, g_smoothPitch);
  lv_label_set_text(s_roll1Lbl, buf);
  snprintf(buf, sizeof(buf), "Accel(g): X%.2f Y%.2f Z%.2f", g_mpuAx, g_mpuAy, g_mpuAz);
  lv_label_set_text(s_accelLbl, buf);
  snprintf(buf, sizeof(buf), "Gyro(dps): X%.0f Y%.0f Z%.0f", g_mpuGx, g_mpuGy, g_mpuGz);
  lv_label_set_text(s_gyroLbl, buf);
  snprintf(buf, sizeof(buf), "Suhu MPU: %.1fC  Chip: %.1fC", g_mpuTempC, temperatureRead());
  lv_label_set_text(s_tempLbl, buf);
}

static void retry_btn_cb(lv_event_t* e) {
  mpuInit();
  chromeShowToast(g_mpuReady ? "MPU6050 terhubung!" : "Masih gagal, cek wiring");
  lv_obj_t* prev = lv_scr_act();
  orientasi3dScreenShow();
  lv_obj_del_async(prev);
}

void orientasi3dScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Orientasi 3D");

  if (!g_mpuReady) {
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 16, 48);
    lv_obj_set_size(box, 320 - 32, 207 - 48 - 8);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_bg_color(box, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_set_style_pad_row(box, 4, 0);

    lv_obj_t* t1 = lv_label_create(box);
    lv_label_set_text(t1, "MPU6050 tidak terdeteksi");
    lv_obj_set_style_text_color(t1, c565(T().danger), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);

    static const char* wiring[4] = { "Cek wiring:", "SDA -> GPIO 15", "SCL -> GPIO 7", "VCC -> 3V3, GND -> GND" };
    for (int i = 0; i < 4; i++) {
      lv_obj_t* wl = lv_label_create(box);
      lv_label_set_text(wl, wiring[i]);
      lv_obj_set_style_text_color(wl, c565(T().subtext), 0);
      lv_obj_set_style_text_font(wl, &lv_font_montserrat_14, 0);
    }

    lv_obj_t* retryBtn = lv_obj_create(box);
    lv_obj_remove_style_all(retryBtn);
    lv_obj_set_size(retryBtn, 140, 28);
    lv_obj_set_style_radius(retryBtn, 6, 0);
    lv_obj_set_style_bg_color(retryBtn, c565(T().accent), 0);
    lv_obj_set_style_bg_opa(retryBtn, LV_OPA_COVER, 0);
    lv_obj_add_flag(retryBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(retryBtn, retry_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* retryLbl = lv_label_create(retryBtn);
    lv_label_set_text(retryLbl, "Coba Lagi");
    lv_obj_set_style_text_color(retryLbl, c565(T().bg), 0);
    lv_obj_center(retryLbl);
    return;
  }

  if (!s_canvasBuf) {
    // Perlu PSRAM (proyek ini udah di-build dgn PSRAM:opi).
    s_canvasBuf = (lv_color_t*)heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_W, CANVAS_H), MALLOC_CAP_SPIRAM);
  }
  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_canvasBuf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 46);

  int ty = 46 + CANVAS_H + 6;
  s_roll1Lbl = lv_label_create(scr);
  lv_obj_set_pos(s_roll1Lbl, 14, ty);
  lv_obj_set_style_text_color(s_roll1Lbl, c565(T().text), 0);
  lv_obj_set_style_text_font(s_roll1Lbl, &lv_font_montserrat_14, 0);

  s_accelLbl = lv_label_create(scr);
  lv_obj_set_pos(s_accelLbl, 14, ty + 16);
  lv_obj_set_style_text_color(s_accelLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_accelLbl, &lv_font_montserrat_14, 0);

  s_gyroLbl = lv_label_create(scr);
  lv_obj_set_pos(s_gyroLbl, 14, ty + 30);
  lv_obj_set_style_text_color(s_gyroLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_gyroLbl, &lv_font_montserrat_14, 0);

  s_tempLbl = lv_label_create(scr);
  lv_obj_set_pos(s_tempLbl, 14, ty + 44);
  lv_obj_set_style_text_color(s_tempLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_tempLbl, &lv_font_montserrat_14, 0);

  poll_cb(NULL);
  lv_timer_t* timer = lv_timer_create(poll_cb, 60, NULL); // ~16fps, cukup halus buat wireframe kecil ini
  chromeBindTimerToScreen(scr, timer);
}
