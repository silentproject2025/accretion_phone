#include "app_flappy.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#define CANVAS_W 300
#define CANVAS_H 150
#define GRAVITY  0.35f
#define IMPULSE  -5.4f
#define GAP      64
#define PIPE_W   26
#define BIRD_X   46
#define BIRD_R   8
#define PIPE_COUNT 3

struct Pipe { int x, gapY; bool passed; };
static Pipe s_pipes[PIPE_COUNT];
static float s_birdY, s_vel;
static int s_score;
static bool s_over, s_started;

static lv_color_t* s_canvasBuf = nullptr;
static lv_obj_t* s_canvas;
static lv_obj_t* s_scoreLbl;
static lv_obj_t* s_hintLbl;
static lv_obj_t* s_overlay = nullptr;

static int randomGapY() {
  int margin = 12;
  int minY = GAP / 2 + margin;
  int maxY = CANVAS_H - GAP / 2 - margin;
  if (maxY < minY) maxY = minY;
  return random(minY, maxY + 1);
}

static void resetGame() {
  s_birdY = CANVAS_H / 2; s_vel = 0;
  s_score = 0; s_over = false; s_started = false;
  for (int i = 0; i < PIPE_COUNT; i++) {
    s_pipes[i].x = CANVAS_W + 60 + i * 130;
    s_pipes[i].gapY = randomGapY();
    s_pipes[i].passed = false;
  }
}

static void showGameOverOverlay(lv_obj_t* scr) {
  s_overlay = lv_obj_create(scr);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, 150, 56);
  lv_obj_center(s_overlay);
  lv_obj_set_style_radius(s_overlay, 10, 0);
  lv_obj_set_style_bg_color(s_overlay, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_overlay, 2, 0);
  lv_obj_set_style_border_color(s_overlay, c565(T().danger), 0);
  lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* t1 = lv_label_create(s_overlay);
  lv_label_set_text(t1, "GAME OVER");
  lv_obj_set_style_text_color(t1, c565(T().danger), 0);
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
  dsc.bg_color = c565(T().good);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = 4;
  for (int i = 0; i < PIPE_COUNT; i++) {
    int gy = s_pipes[i].gapY;
    int topH = max(0, gy - GAP / 2);
    int botY = gy + GAP / 2;
    int botH = max(0, CANVAS_H - botY);
    lv_canvas_draw_rect(s_canvas, s_pipes[i].x, 0, PIPE_W, topH, &dsc);
    lv_canvas_draw_rect(s_canvas, s_pipes[i].x, botY, PIPE_W, botH, &dsc);
  }
  lv_draw_rect_dsc_t birdDsc;
  lv_draw_rect_dsc_init(&birdDsc);
  birdDsc.bg_color = c565(T().accent);
  birdDsc.bg_opa = LV_OPA_COVER;
  birdDsc.radius = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(s_canvas, BIRD_X - BIRD_R, (int)s_birdY - BIRD_R, BIRD_R * 2, BIRD_R * 2, &birdDsc);

  char sb[8]; snprintf(sb, sizeof(sb), "%d", s_score);
  lv_label_set_text(s_scoreLbl, sb);
  if (s_hintLbl) {
    if (s_started) lv_obj_add_flag(s_hintLbl, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_hintLbl, LV_OBJ_FLAG_HIDDEN);
  }
}

static void gameTick(lv_timer_t* t) {
  if (!s_started || s_over) { redraw(); return; }
  s_vel += GRAVITY;
  s_birdY += s_vel;
  if (s_birdY > CANVAS_H - 6 || s_birdY < 6) {
    s_over = true; redraw(); showGameOverOverlay(lv_obj_get_parent(s_canvas)); return;
  }
  for (int i = 0; i < PIPE_COUNT; i++) {
    s_pipes[i].x -= 3;
    if (s_pipes[i].x < -PIPE_W) {
      s_pipes[i].x = CANVAS_W + 10;
      s_pipes[i].gapY = randomGapY();
      s_pipes[i].passed = false;
    }
    bool xOverlap = (BIRD_X + BIRD_R > s_pipes[i].x) && (BIRD_X - BIRD_R < s_pipes[i].x + PIPE_W);
    if (xOverlap) {
      if (s_birdY - BIRD_R < s_pipes[i].gapY - GAP / 2 || s_birdY + BIRD_R > s_pipes[i].gapY + GAP / 2) {
        s_over = true;
      }
    }
    if (!s_pipes[i].passed && s_pipes[i].x + PIPE_W < BIRD_X) { s_pipes[i].passed = true; s_score++; }
  }
  if (s_over) { redraw(); showGameOverOverlay(lv_obj_get_parent(s_canvas)); return; }
  redraw();
}

static void canvas_click_cb(lv_event_t* e) {
  if (s_over) {
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = nullptr; }
    resetGame(); redraw(); return;
  }
  s_started = true;
  s_vel = IMPULSE;
}

void flappyScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Flappy");
  s_overlay = nullptr;

  s_scoreLbl = lv_label_create(scr);
  lv_obj_align(s_scoreLbl, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_set_style_text_color(s_scoreLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(s_scoreLbl, &lv_font_montserrat_28, 0);

  if (!s_canvasBuf) {
    // Perlu PSRAM (proyek ini udah di-build dgn PSRAM:opi).
    s_canvasBuf = (lv_color_t*)heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_W, CANVAS_H), MALLOC_CAP_SPIRAM);
  }
  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_canvasBuf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_canvas, canvas_click_cb, LV_EVENT_CLICKED, NULL);

  s_hintLbl = lv_label_create(scr);
  lv_label_set_text(s_hintLbl, "Ketuk layar utk mulai");
  lv_obj_set_style_text_color(s_hintLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_hintLbl, &lv_font_montserrat_14, 0);
  lv_obj_align(s_hintLbl, LV_ALIGN_CENTER, 0, 30);

  resetGame();
  redraw();
  lv_timer_t* timer = lv_timer_create(gameTick, 30, NULL);
  chromeBindTimerToScreen(scr, timer);
}
