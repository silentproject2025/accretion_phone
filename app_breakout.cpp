#include "app_breakout.h"
#include "screen_chrome.h"
#include "theme.h"
#include "mpu_sensor.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#define BRK_COLS 8
#define BRK_ROWS 4
#define PADW 44
#define PADH 8
#define BALLR 4
#define CANVAS_W 304
#define CANVAS_H 160
static const int BRICK_W = CANVAS_W / BRK_COLS;
static const int BRICK_H = 14;
static const int PAD_Y = CANVAS_H - 24;

static bool  s_bricks[BRK_ROWS][BRK_COLS];
static float s_padX;
static float s_ballX, s_ballY, s_velX, s_velY;
static int   s_score;
static bool  s_over, s_win, s_started;
static unsigned long s_lastTouchMs;

static lv_color_t* s_canvasBuf = nullptr;
static lv_obj_t* s_canvas;
static lv_obj_t* s_scoreLbl;
static lv_obj_t* s_hintLbl;
static lv_obj_t* s_overlay = nullptr;

static void resetGame() {
  for (int r = 0; r < BRK_ROWS; r++) for (int c = 0; c < BRK_COLS; c++) s_bricks[r][c] = true;
  s_padX = CANVAS_W / 2;
  s_ballX = CANVAS_W / 2; s_ballY = CANVAS_H - 40;
  s_velX = 0; s_velY = 0;
  s_score = 0; s_over = false; s_win = false; s_started = false;
  s_lastTouchMs = 0;
}

static void showEndOverlay(lv_obj_t* scr) {
  s_overlay = lv_obj_create(scr);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, 150, 56);
  lv_obj_center(s_overlay);
  lv_obj_set_style_radius(s_overlay, 10, 0);
  lv_obj_set_style_bg_color(s_overlay, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_overlay, 2, 0);
  lv_obj_set_style_border_color(s_overlay, c565(s_win ? T().good : T().danger), 0);
  lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* t1 = lv_label_create(s_overlay);
  lv_label_set_text(t1, s_win ? "MENANG!" : "GAME OVER");
  lv_obj_set_style_text_color(t1, c565(s_win ? T().good : T().danger), 0);
  lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);
  lv_obj_t* t2 = lv_label_create(s_overlay);
  lv_label_set_text(t2, "Ketuk layar utk ulang");
  lv_obj_set_style_text_color(t2, c565(T().subtext), 0);
  lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, 0);
}

static void redraw() {
  lv_canvas_fill_bg(s_canvas, c565(T().bg), LV_OPA_COVER);
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_COVER;
  for (int r = 0; r < BRK_ROWS; r++) {
    for (int c = 0; c < BRK_COLS; c++) {
      if (!s_bricks[r][c]) continue;
      dsc.bg_color = c565(r == 0 ? T().danger : r == 1 ? T().accent : r == 2 ? T().accent2 : T().good);
      lv_canvas_draw_rect(s_canvas, c * BRICK_W + 1, r * BRICK_H + 1, BRICK_W - 2, BRICK_H - 2, &dsc);
    }
  }
  lv_draw_rect_dsc_t padDsc;
  lv_draw_rect_dsc_init(&padDsc);
  padDsc.bg_color = c565(T().accent);
  padDsc.bg_opa = LV_OPA_COVER;
  padDsc.radius = 3;
  lv_canvas_draw_rect(s_canvas, (int)(s_padX - PADW / 2), PAD_Y, PADW, PADH, &padDsc);

  lv_draw_rect_dsc_t ballDsc;
  lv_draw_rect_dsc_init(&ballDsc);
  ballDsc.bg_color = c565(T().text);
  ballDsc.bg_opa = LV_OPA_COVER;
  ballDsc.radius = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(s_canvas, (int)s_ballX - BALLR, (int)s_ballY - BALLR, BALLR * 2, BALLR * 2, &ballDsc);

  char sb[24]; snprintf(sb, sizeof(sb), "Skor: %d", s_score);
  lv_label_set_text(s_scoreLbl, sb);
  if (s_hintLbl) {
    if (s_started) lv_obj_add_flag(s_hintLbl, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_hintLbl, LV_OBJ_FLAG_HIDDEN);
  }
}

static void gameTick(lv_timer_t* t) {
  if (!s_started || s_over || s_win) { redraw(); return; }

  // Kemiringan HP cuma bantu kalau gak ada sentuhan >150ms terakhir (fix
  // v20 file lama) -- drag jari SELALU prioritas, biar gak rebutan.
  if (g_mpuReady && millis() - s_lastTouchMs > 150) {
    float target = CANVAS_W / 2 + constrain(g_smoothRoll, -35.0f, 35.0f) * 3.2f;
    s_padX += (target - s_padX) * 0.25f;
  }
  s_padX = constrain(s_padX, PADW / 2.0f, CANVAS_W - PADW / 2.0f);

  s_ballX += s_velX; s_ballY += s_velY;
  if (s_ballX < BALLR || s_ballX > CANVAS_W - BALLR) s_velX = -s_velX;
  if (s_ballY < BALLR) s_velY = -s_velY;
  if (s_ballY > CANVAS_H + 20) { s_over = true; redraw(); showEndOverlay(lv_obj_get_parent(s_canvas)); return; }

  if (s_ballY + BALLR >= PAD_Y && s_ballY < PAD_Y + PADH &&
      s_ballX > s_padX - PADW / 2 && s_ballX < s_padX + PADW / 2 && s_velY > 0) {
    s_velY = -fabsf(s_velY);
    float hit = (s_ballX - s_padX) / (PADW / 2.0f);
    s_velX = hit * 3.2f;
  }

  for (int r = 0; r < BRK_ROWS; r++) {
    bool hitThisTick = false;
    for (int c = 0; c < BRK_COLS; c++) {
      if (!s_bricks[r][c]) continue;
      int bx = c * BRICK_W, by = r * BRICK_H;
      if (s_ballX > bx && s_ballX < bx + BRICK_W && s_ballY - BALLR < by + BRICK_H && s_ballY + BALLR > by) {
        s_bricks[r][c] = false;
        s_velY = -s_velY;
        s_score += 10;
        hitThisTick = true;
        break;
      }
    }
    if (hitThisTick) break;
  }

  bool anyLeft = false;
  for (int r = 0; r < BRK_ROWS; r++) for (int c = 0; c < BRK_COLS; c++) if (s_bricks[r][c]) anyLeft = true;
  if (!anyLeft) { s_win = true; redraw(); showEndOverlay(lv_obj_get_parent(s_canvas)); return; }

  redraw();
}

static int localX(lv_event_t* e) {
  lv_indev_t* indev = lv_indev_get_act();
  lv_point_t p; lv_indev_get_point(indev, &p);
  lv_area_t area; lv_obj_get_coords(s_canvas, &area);
  return p.x - area.x1;
}

static void canvas_pressed_cb(lv_event_t* e) {
  if (s_over || s_win) {
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = nullptr; }
    resetGame(); redraw(); return;
  }
  int lx = localX(e);
  if (!s_started) {
    s_started = true; s_velX = 1.6f; s_velY = -3.2f;
    s_padX = lx; s_lastTouchMs = millis();
    return;
  }
  s_padX = lx; s_lastTouchMs = millis();
}
static void canvas_pressing_cb(lv_event_t* e) {
  if (!s_started || s_over || s_win) return;
  s_padX = localX(e);
  s_lastTouchMs = millis();
}

void breakoutScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Breakout");
  s_overlay = nullptr;

  s_scoreLbl = lv_label_create(scr);
  lv_obj_set_pos(s_scoreLbl, 8, 26);
  lv_obj_set_style_text_color(s_scoreLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_scoreLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* dragHintLbl = lv_label_create(scr);
  lv_label_set_text(dragHintLbl, "(drag = geser)");
  lv_obj_set_style_text_color(dragHintLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(dragHintLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(dragHintLbl, 320 - 108, 26);

  if (!s_canvasBuf) {
    // Perlu PSRAM (proyek ini udah di-build dgn PSRAM:opi).
    s_canvasBuf = (lv_color_t*)heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_W, CANVAS_H), MALLOC_CAP_SPIRAM);
  }
  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_canvasBuf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_canvas, canvas_pressed_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s_canvas, canvas_pressing_cb, LV_EVENT_PRESSING, NULL);

  s_hintLbl = lv_label_create(scr);
  lv_label_set_text(s_hintLbl, "Ketuk layar utk mulai");
  lv_obj_set_style_text_color(s_hintLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_hintLbl, &lv_font_montserrat_14, 0);
  lv_obj_align(s_hintLbl, LV_ALIGN_CENTER, 0, 40);

  resetGame();
  redraw();
  lv_timer_t* timer = lv_timer_create(gameTick, 20, NULL);
  chromeBindTimerToScreen(scr, timer);
}
