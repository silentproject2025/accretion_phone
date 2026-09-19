#include "app_labirin.h"
#include "screen_chrome.h"
#include "theme.h"
#include "mpu_sensor.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <Preferences.h>

#define MAZE_COLS 15
#define MAZE_ROWS 15
#define MAZE_TUNNEL_ROW 7
#define MAZE_MAX_GHOSTS 4
#define TILE 10
#define CANVAS_W (MAZE_COLS * TILE)
#define CANVAS_H (MAZE_ROWS * TILE)

static const char* kTemplate[MAZE_ROWS] = {
  "###############",
  "#......#......#",
  "#.###.#.#.###.#",
  "#.#...........#",
  "#.#.###.###.#.#",
  "#...#.....#...#",
  "###.#.###.#.###",
  "....#.....#....",
  "###.#.###.#.###",
  "#...#.....#...#",
  "#.#.###.###.#.#",
  "#.#...........#",
  "#.###.#.#.###.#",
  "#......#......#",
  "###############",
};

struct Ghost { int x, y, prevX, prevY, dirX, dirY; };
static Ghost s_ghosts[MAZE_MAX_GHOSTS];
static int  s_ghostCount;
static bool s_pellet[MAZE_ROWS][MAZE_COLS];
static int  s_pelletsLeft;
static int  s_px, s_py, s_prevX, s_prevY, s_dirX, s_dirY, s_qDirX, s_qDirY;
static int  s_lastTiltDirX = 0, s_lastTiltDirY = 0;
static int  s_level, s_lives, s_score, s_bestScore, s_maxLevelReached;
static int  s_playerTickMs, s_ghostTickMs;
static unsigned long s_playerLastTick, s_ghostLastTick, s_stateChangeMs, s_invulnUntil;
static bool s_invuln, s_showNewBest, s_progressLoaded;
static int  s_touchBaseX, s_touchBaseY;
static bool s_touchActive;
enum MazeState { MZ_PLAYING, MZ_LEVEL_CLEAR, MZ_GAME_OVER };
static MazeState s_state;

static lv_color_t* s_canvasBuf = nullptr;
static lv_obj_t* s_canvas;
static lv_obj_t* s_infoLbl;

static void loadProgress() {
  Preferences p; p.begin("maze", true);
  s_bestScore = p.getInt("best", 0);
  s_maxLevelReached = p.getInt("maxlvl", 1);
  p.end();
  s_progressLoaded = true;
}
static void saveProgress() {
  Preferences p; p.begin("maze", false);
  p.putInt("best", s_bestScore);
  p.putInt("maxlvl", s_maxLevelReached);
  p.end();
}
static uint16_t playerColor() {
  if (s_maxLevelReached >= 7) return 0xFFE0; // tier 3: emas
  if (s_maxLevelReached >= 4) return 0x07FF; // tier 2: cyan
  return 0xFDA0; // tier 1
}

static bool canStep(int x, int y, int dx, int dy, int& nx, int& ny) {
  nx = x + dx; ny = y + dy;
  if (ny == MAZE_TUNNEL_ROW) {
    if (nx < 0) nx = MAZE_COLS - 1;
    else if (nx >= MAZE_COLS) nx = 0;
  }
  if (nx < 0 || nx >= MAZE_COLS || ny < 0 || ny >= MAZE_ROWS) return false;
  if (kTemplate[ny][nx] == '#') return false;
  return true;
}
static void lerpPos(int prevX, int prevY, int curX, int curY, float frac, float& outX, float& outY) {
  int dx = curX - prevX, dy = curY - prevY;
  if (abs(dx) > 1) dx = 0; // abis wrap terowongan -- snap, jgn diinterpolasi
  if (abs(dy) > 1) dy = 0;
  outX = prevX + dx * frac; outY = prevY + dy * frac;
}

static void buildPellets() {
  s_pelletsLeft = 0;
  for (int y = 0; y < MAZE_ROWS; y++) for (int x = 0; x < MAZE_COLS; x++) {
    s_pellet[y][x] = (kTemplate[y][x] == '.');
    if (s_pellet[y][x]) s_pelletsLeft++;
  }
}
static void spawnGhosts(int count) {
  const int gx[4] = {1, 13, 1, 13}, gy[4] = {1, 1, 13, 13};
  s_ghostCount = count;
  for (int i = 0; i < count; i++) {
    s_ghosts[i].x = gx[i]; s_ghosts[i].y = gy[i];
    s_ghosts[i].prevX = gx[i]; s_ghosts[i].prevY = gy[i];
    s_ghosts[i].dirX = 0; s_ghosts[i].dirY = 0;
  }
}
static void setupLevel() {
  buildPellets();
  s_px = 7; s_py = 3; s_prevX = 7; s_prevY = 3;
  s_dirX = 0; s_dirY = 0; s_qDirX = 0; s_qDirY = 0;
  s_lastTiltDirX = 0; s_lastTiltDirY = 0;
  spawnGhosts(min(1 + (s_level - 1), MAZE_MAX_GHOSTS));
  s_playerTickMs = max(90, 220 - (s_level - 1) * 12);
  s_ghostTickMs = max(110, 260 - (s_level - 1) * 14);
  s_playerLastTick = millis(); s_ghostLastTick = millis();
  s_state = MZ_PLAYING;
  s_invuln = true; s_invulnUntil = millis() + 1200;
}
static void resetGame() {
  if (!s_progressLoaded) loadProgress();
  s_level = 1; s_lives = 3; s_score = 0; s_showNewBest = false;
  setupLevel();
}
static void nextLevel() {
  s_level++; s_score += 100;
  setupLevel();
}
static void loseLife() {
  s_lives--;
  if (s_lives <= 0) {
    bool newBest = s_score > s_bestScore;
    if (newBest) s_bestScore = s_score;
    bool newMaxLvl = s_level > s_maxLevelReached;
    if (newMaxLvl) s_maxLevelReached = s_level;
    s_showNewBest = newBest;
    if (newBest || newMaxLvl) saveProgress();
    s_state = MZ_GAME_OVER;
  } else {
    s_px = 7; s_py = 3; s_prevX = 7; s_prevY = 3;
    s_dirX = 0; s_dirY = 0; s_qDirX = 0; s_qDirY = 0;
    s_lastTiltDirX = 0; s_lastTiltDirY = 0;
    spawnGhosts(s_ghostCount);
    s_invuln = true; s_invulnUntil = millis() + 1200;
  }
}

static void playerTick() {
  if (s_qDirX != 0 || s_qDirY != 0) {
    int nx, ny;
    if (canStep(s_px, s_py, s_qDirX, s_qDirY, nx, ny)) { s_dirX = s_qDirX; s_dirY = s_qDirY; }
  }
  s_prevX = s_px; s_prevY = s_py;
  if (s_dirX != 0 || s_dirY != 0) {
    int nx, ny;
    if (canStep(s_px, s_py, s_dirX, s_dirY, nx, ny)) { s_px = nx; s_py = ny; }
    else { s_dirX = 0; s_dirY = 0; }
  }
  if (s_pellet[s_py][s_px]) {
    s_pellet[s_py][s_px] = false; s_pelletsLeft--; s_score += 10;
    if (s_pelletsLeft <= 0) { s_state = MZ_LEVEL_CLEAR; s_stateChangeMs = millis(); }
  }
}
static void ghostTick(Ghost& g) {
  const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
  int candDx[4], candDy[4], candCount = 0;
  for (int i = 0; i < 4; i++) {
    int dx = dirs[i][0], dy = dirs[i][1];
    if ((g.dirX != 0 || g.dirY != 0) && dx == -g.dirX && dy == -g.dirY) continue;
    int nx, ny;
    if (canStep(g.x, g.y, dx, dy, nx, ny)) { candDx[candCount] = dx; candDy[candCount] = dy; candCount++; }
  }
  if (candCount == 0) {
    int dx = -g.dirX, dy = -g.dirY, nx, ny;
    if ((dx != 0 || dy != 0) && canStep(g.x, g.y, dx, dy, nx, ny)) { candDx[0] = dx; candDy[0] = dy; candCount = 1; }
  }
  if (candCount == 0) return;
  int pickDx, pickDy;
  if (random(0, 100) < 15) {
    int pick = random(0, candCount); pickDx = candDx[pick]; pickDy = candDy[pick];
  } else {
    long bestDist = 2147483647L; pickDx = candDx[0]; pickDy = candDy[0];
    for (int i = 0; i < candCount; i++) {
      int nx, ny; canStep(g.x, g.y, candDx[i], candDy[i], nx, ny);
      long d = (long)abs(nx - s_px) + abs(ny - s_py);
      if (d < bestDist) { bestDist = d; pickDx = candDx[i]; pickDy = candDy[i]; }
    }
  }
  int nx, ny; canStep(g.x, g.y, pickDx, pickDy, nx, ny);
  g.prevX = g.x; g.prevY = g.y; g.x = nx; g.y = ny; g.dirX = pickDx; g.dirY = pickDy;
}

static void redraw() {
  lv_canvas_fill_bg(s_canvas, c565(T().bg), LV_OPA_COVER);

  lv_draw_rect_dsc_t wallDsc;
  lv_draw_rect_dsc_init(&wallDsc);
  wallDsc.bg_color = c565(T().surface2);
  wallDsc.bg_opa = LV_OPA_COVER;
  lv_draw_rect_dsc_t pelletDsc;
  lv_draw_rect_dsc_init(&pelletDsc);
  pelletDsc.bg_color = c565(T().accent);
  pelletDsc.bg_opa = LV_OPA_COVER;
  pelletDsc.radius = LV_RADIUS_CIRCLE;

  for (int y = 0; y < MAZE_ROWS; y++) for (int x = 0; x < MAZE_COLS; x++) {
    int wx = x * TILE, wy = y * TILE;
    if (kTemplate[y][x] == '#') lv_canvas_draw_rect(s_canvas, wx, wy, TILE, TILE, &wallDsc);
    else if (s_pellet[y][x]) lv_canvas_draw_rect(s_canvas, wx + TILE / 2 - 1, wy + TILE / 2 - 1, 2, 2, &pelletDsc);
  }

  const uint16_t ghostColors[4] = { T().danger, 0xFC9F, 0x07FF, 0xFBE0 };
  lv_draw_rect_dsc_t ghostDsc;
  lv_draw_rect_dsc_init(&ghostDsc);
  ghostDsc.bg_opa = LV_OPA_COVER;
  ghostDsc.radius = LV_RADIUS_CIRCLE;
  for (int i = 0; i < s_ghostCount; i++) {
    float frac = constrain((millis() - s_ghostLastTick) / (float)s_ghostTickMs, 0.0f, 1.0f);
    float lx, ly; lerpPos(s_ghosts[i].prevX, s_ghosts[i].prevY, s_ghosts[i].x, s_ghosts[i].y, frac, lx, ly);
    int gx = (int)((lx + 0.5f) * TILE), gy = (int)((ly + 0.5f) * TILE);
    int r = max(2, TILE / 2 - 1);
    ghostDsc.bg_color = c565(ghostColors[i % 4]);
    lv_canvas_draw_rect(s_canvas, gx - r, gy - r, r * 2, r * 2, &ghostDsc);
  }

  {
    float frac = constrain((millis() - s_playerLastTick) / (float)s_playerTickMs, 0.0f, 1.0f);
    float lx, ly; lerpPos(s_prevX, s_prevY, s_px, s_py, frac, lx, ly);
    int px = (int)((lx + 0.5f) * TILE), py = (int)((ly + 0.5f) * TILE);
    int r = max(2, TILE / 2 - 1);
    bool blink = s_invuln && ((millis() / 150) % 2 == 0);
    if (!blink) {
      lv_draw_rect_dsc_t pDsc;
      lv_draw_rect_dsc_init(&pDsc);
      pDsc.bg_color = c565(playerColor());
      pDsc.bg_opa = LV_OPA_COVER;
      pDsc.radius = LV_RADIUS_CIRCLE;
      lv_canvas_draw_rect(s_canvas, px - r, py - r, r * 2, r * 2, &pDsc);
    }
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "Lv%d  Nyawa:%d  Skor:%d  Terbaik:%d", s_level, s_lives, s_score, s_bestScore);
  lv_label_set_text(s_infoLbl, buf);
}

static lv_obj_t* showOverlay() {
  lv_obj_t* scr = lv_obj_get_parent(s_canvas);
  if (s_state == MZ_LEVEL_CLEAR) {
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 150, 48);
    lv_obj_center(box);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_bg_color(box, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, c565(T().good), 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* l = lv_label_create(box);
    char lb[24]; snprintf(lb, sizeof(lb), "Level %d Selesai!", s_level - 1 > 0 ? s_level - 1 : s_level);
    lv_label_set_text(l, lb);
    lv_obj_set_style_text_color(l, c565(T().good), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    return box;
  } else if (s_state == MZ_GAME_OVER) {
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 160, 76);
    lv_obj_center(box);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_bg_color(box, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, c565(T().danger), 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* t1 = lv_label_create(box);
    lv_label_set_text(t1, "GAME OVER");
    lv_obj_set_style_text_color(t1, c565(T().danger), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);
    lv_obj_t* t2 = lv_label_create(box);
    char lb[32]; snprintf(lb, sizeof(lb), "Skor: %d (Lv%d)", s_score, s_level);
    lv_label_set_text(t2, lb);
    lv_obj_set_style_text_color(t2, c565(T().text), 0);
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, 0);
    if (s_showNewBest) {
      lv_obj_t* t3 = lv_label_create(box);
      lv_label_set_text(t3, "Rekor baru!");
      lv_obj_set_style_text_color(t3, c565(T().good), 0);
      lv_obj_set_style_text_font(t3, &lv_font_montserrat_14, 0);
    }
    lv_obj_t* t4 = lv_label_create(box);
    lv_label_set_text(t4, "Ketuk layar utk main lagi");
    lv_obj_set_style_text_color(t4, c565(T().subtext), 0);
    lv_obj_set_style_text_font(t4, &lv_font_montserrat_14, 0);
    return box;
  }
  return nullptr;
}

static void gameTick(lv_timer_t* t) {
  if (s_state == MZ_PLAYING) {
    if (s_invuln && millis() > s_invulnUntil) s_invuln = false;

    // Tilt (kalau MPU6050 kedetek) -- edge-triggered, fix v70: cuma nembak
    // arah SEKALI pas baru lewat ambang (beda dr frame sblmnya), BUKAN
    // terus2an selama masih miring -- biar gak nimpa swipe user tiap frame.
    if (g_mpuReady) {
      const float ANG = 12.0f;
      int curTiltDirX = 0, curTiltDirY = 0;
      if (fabsf(g_smoothRoll) > ANG && fabsf(g_smoothRoll) >= fabsf(g_smoothPitch)) curTiltDirX = (g_smoothRoll > 0) ? 1 : -1;
      else if (fabsf(g_smoothPitch) > ANG) curTiltDirY = (g_smoothPitch > 0) ? 1 : -1;
      if ((curTiltDirX != 0 || curTiltDirY != 0) && (curTiltDirX != s_lastTiltDirX || curTiltDirY != s_lastTiltDirY)) {
        s_qDirX = curTiltDirX; s_qDirY = curTiltDirY;
      }
      s_lastTiltDirX = curTiltDirX; s_lastTiltDirY = curTiltDirY;
    }

    if (millis() - s_playerLastTick >= (unsigned long)s_playerTickMs) { playerTick(); s_playerLastTick = millis(); }
    if (s_state == MZ_PLAYING && millis() - s_ghostLastTick >= (unsigned long)s_ghostTickMs) {
      for (int i = 0; i < s_ghostCount; i++) ghostTick(s_ghosts[i]);
      s_ghostLastTick = millis();
    }
    if (s_state == MZ_PLAYING && !s_invuln) {
      for (int i = 0; i < s_ghostCount; i++) if (s_ghosts[i].x == s_px && s_ghosts[i].y == s_py) { loseLife(); break; }
    }
  } else if (s_state == MZ_LEVEL_CLEAR) {
    if (millis() - s_stateChangeMs > 1200) nextLevel();
  }

  redraw();

  // Overlay LEVEL_CLEAR/GAME_OVER dibangun sekali pas state BARU berubah
  // (bukan tiap frame) -- lastOverlay nge-track objeknya biar gak numpuk,
  // & kehapus lagi pas balik ke PLAYING (mis. restart).
  static lv_obj_t* lastOverlay = nullptr;
  if (s_state != MZ_PLAYING) {
    if (!lastOverlay) lastOverlay = showOverlay();
  } else if (lastOverlay) {
    lv_obj_del(lastOverlay);
    lastOverlay = nullptr;
  }
}

static int localCoord(lv_event_t* e, int* outX, int* outY) {
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return 0;
  lv_point_t p; lv_indev_get_point(indev, &p);
  lv_area_t area; lv_obj_get_coords(s_canvas, &area);
  *outX = p.x - area.x1; *outY = p.y - area.y1;
  return 1;
}
static void canvas_pressed_cb(lv_event_t* e) {
  if (s_state == MZ_GAME_OVER) { resetGame(); redraw(); return; }
  if (s_state != MZ_PLAYING) return;
  int x, y; if (!localCoord(e, &x, &y)) return;
  s_touchBaseX = x; s_touchBaseY = y; s_touchActive = true;
}
static void canvas_pressing_cb(lv_event_t* e) {
  if (s_state != MZ_PLAYING || !s_touchActive) return;
  int x, y; if (!localCoord(e, &x, &y)) return;
  int dx = x - s_touchBaseX, dy = y - s_touchBaseY;
  int adx = abs(dx), ady = abs(dy);
  if (max(adx, ady) > 16) {
    if (adx > ady) { s_qDirX = dx > 0 ? 1 : -1; s_qDirY = 0; }
    else { s_qDirY = dy > 0 ? 1 : -1; s_qDirX = 0; }
    s_touchBaseX = x; s_touchBaseY = y; // reset acuan -- bisa swipe berulang dlm 1 drag
  }
}

void labirinScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen(""); // judul dikosongkan, dipakai info line sendiri

  s_infoLbl = lv_label_create(scr);
  lv_obj_set_pos(s_infoLbl, 8, 26);
  lv_obj_set_style_text_color(s_infoLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_infoLbl, &lv_font_montserrat_14, 0);

  if (!s_canvasBuf) {
    // Perlu PSRAM (proyek ini udah di-build dgn PSRAM:opi).
    s_canvasBuf = (lv_color_t*)heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_W, CANVAS_H), MALLOC_CAP_SPIRAM);
  }
  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_canvasBuf, CANVAS_W, CANVAS_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(s_canvas, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_canvas, canvas_pressed_cb, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s_canvas, canvas_pressing_cb, LV_EVENT_PRESSING, NULL);

  s_touchActive = false;
  resetGame();
  redraw();
  lv_timer_t* timer = lv_timer_create(gameTick, 40, NULL); // ~25fps, sama spt render asli
  chromeBindTimerToScreen(scr, timer);
}
