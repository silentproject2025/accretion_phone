#include "app_snake.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#define CELL 12
#define COLS 25
#define ROWS 13
#define MAXLEN (COLS * ROWS)

struct Seg { int x, y; };
static Seg s_body[MAXLEN];
static int s_cols = COLS, s_rows = ROWS, s_len;
static int s_dirX, s_dirY;
static int s_foodX, s_foodY;
static int s_score, s_tickMs;
static bool s_over;

static lv_color_t* s_canvasBuf = nullptr; // PSRAM, sekali alokasi dipakai seumur app
static lv_obj_t* s_canvas;
static lv_obj_t* s_scoreLbl;
static lv_obj_t* s_overlay = nullptr;
static lv_timer_t* s_tickTimer;

static void spawnFood() {
  bool onBody;
  do {
    onBody = false;
    s_foodX = random(0, s_cols);
    s_foodY = random(0, s_rows);
    for (int i = 0; i < s_len; i++) if (s_body[i].x == s_foodX && s_body[i].y == s_foodY) { onBody = true; break; }
  } while (onBody);
}
static void resetGame() {
  s_len = 3;
  for (int i = 0; i < s_len; i++) { s_body[i].x = s_cols / 2 - i; s_body[i].y = s_rows / 2; }
  s_dirX = 1; s_dirY = 0;
  s_score = 0; s_over = false; s_tickMs = 150;
  spawnFood();
}

static void redraw() {
  lv_canvas_fill_bg(s_canvas, c565(T().bg), LV_OPA_COVER);

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.radius = 2;
  dsc.bg_opa = LV_OPA_COVER;
  for (int i = 0; i < s_len; i++) {
    dsc.bg_color = c565(i == 0 ? T().accent : T().good);
    lv_canvas_draw_rect(s_canvas, s_body[i].x * CELL, s_body[i].y * CELL, CELL - 1, CELL - 1, &dsc);
  }
  dsc.bg_color = c565(T().danger);
  dsc.radius = 4;
  lv_canvas_draw_rect(s_canvas, s_foodX * CELL, s_foodY * CELL, CELL - 1, CELL - 1, &dsc);

  char sb[24]; snprintf(sb, sizeof(sb), "Skor: %d", s_score);
  lv_label_set_text(s_scoreLbl, sb);
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

static void gameTick(lv_timer_t* t) {
  if (s_over) return;
  Seg head = { s_body[0].x + s_dirX, s_body[0].y + s_dirY };
  if (head.x < 0) head.x = s_cols - 1; else if (head.x >= s_cols) head.x = 0;
  if (head.y < 0) head.y = s_rows - 1; else if (head.y >= s_rows) head.y = 0;
  for (int i = 0; i < s_len; i++) if (s_body[i].x == head.x && s_body[i].y == head.y) {
    s_over = true; redraw(); showGameOverOverlay(lv_obj_get_parent(s_canvas)); return;
  }
  bool grow = (head.x == s_foodX && head.y == s_foodY);
  int newLen = grow ? min(s_len + 1, MAXLEN) : s_len;
  for (int i = newLen - 1; i > 0; i--) s_body[i] = s_body[i - 1];
  s_body[0] = head;
  s_len = newLen;
  if (grow) {
    s_score += 10;
    if (s_tickMs > 70) { s_tickMs -= 3; lv_timer_set_period(s_tickTimer, s_tickMs); }
    spawnFood();
  }
  redraw();
}

static void canvas_click_cb(lv_event_t* e) {
  if (s_over) {
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = nullptr; }
    resetGame(); redraw(); return;
  }
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p; lv_indev_get_point(indev, &p);
  lv_area_t area; lv_obj_get_coords(s_canvas, &area);
  int lx = p.x - area.x1, ly = p.y - area.y1;

  int hx = s_body[0].x * CELL + CELL / 2, hy = s_body[0].y * CELL + CELL / 2;
  int dx = lx - hx, dy = ly - hy;
  int nx = s_dirX, ny = s_dirY;
  if (abs(dx) > abs(dy)) { nx = dx > 0 ? 1 : -1; ny = 0; } else { ny = dy > 0 ? 1 : -1; nx = 0; }
  if (!(nx == -s_dirX && ny == -s_dirY)) { s_dirX = nx; s_dirY = ny; } // gak boleh balik 180 derajat langsung
}

void snakeScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("Snake");
  s_overlay = nullptr; // reset -- objek lama (kalau ada) udah ikut kehapus bareng layar sebelumnya

  s_scoreLbl = lv_label_create(scr);
  lv_obj_set_pos(s_scoreLbl, 320 - 90, 26);
  lv_obj_set_style_text_color(s_scoreLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_scoreLbl, &lv_font_montserrat_14, 0);

  if (!s_canvasBuf) {
    // Perlu PSRAM (proyek ini udah di-build dgn PSRAM:opi, lihat workflow) --
    // ~90KB gak akan muat di internal RAM.
    s_canvasBuf = (lv_color_t*)heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(COLS * CELL, ROWS * CELL), MALLOC_CAP_SPIRAM);
  }
  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_canvasBuf, COLS * CELL, ROWS * CELL, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_canvas, canvas_click_cb, LV_EVENT_CLICKED, NULL);

  resetGame();
  redraw();
  s_tickTimer = lv_timer_create(gameTick, s_tickMs, NULL);
  chromeBindTimerToScreen(scr, s_tickTimer);
}
