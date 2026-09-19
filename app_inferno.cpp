#include "app_inferno.h"
#include "screen_chrome.h"
#include "theme.h"
#include "mpu_sensor.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include <math.h>

#define MAP_W 18
#define MAP_H 16
#define MAX_ENEMIES 6
#define RAYS 140
#define COL_W 2
#define CANVAS_W (RAYS * COL_W)
#define CANVAS_H 140
#define JOY_RADIUS 46

static const char* kMap[MAP_H] = {
  "322222222222222224",
  "3................4",
  "3................4",
  "3................4",
  "3...44......22...4",
  "3...44......22...4",
  "3................4",
  "3.1.....33.....1.4",
  "3.1.....33.....1.4",
  "3................4",
  "3...22......44...4",
  "3...22......44...4",
  "3................4",
  "3................4",
  "3................4",
  "311111111111111114",
};
#define WALL_METAL    0x5B2D
#define WALL_BRICK    0x71C4
#define WALL_HAZARD_A 0xC2E2
#define WALL_HAZARD_B 0x20C2
#define WALL_FLESH    0x90A3
#define CEIL_FAR      0x0821
#define CEIL_NEAR     0x28A3
#define FLOOR_NEAR    0x59C4
#define FLOOR_FAR     0x2061
#define ENEMY_BODY    0x70C3
#define ENEMY_DARK    0x3841
#define GUN_METAL     0x39C7
#define GUN_DARK      0x18C3
#define FOG_COLOR     0x3000

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t alpha) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int rr = ar + ((br - ar) * alpha) / 255;
  int rg = ag + ((bg - ag) * alpha) / 255;
  int rb = ab + ((bb - ab) * alpha) / 255;
  return (rr << 11) | (rg << 5) | rb;
}

struct Enemy { float x, y; bool alive; int hp; unsigned long hitFlashUntil; };
static Enemy s_enemies[MAX_ENEMIES];
static int s_enemyCount;
static float s_posX, s_posY, s_dirX, s_dirY, s_planeX, s_planeY;
static float s_zbuf[RAYS];
static int s_level, s_lives, s_score, s_bestScore, s_maxLevelReached;
static float s_health, s_maxHealth;
static bool s_showNewBest, s_progressLoaded;
static unsigned long s_lastShotMs, s_fireCooldownMs = 380, s_muzzleFlashUntil;
static float s_gunKick;
static unsigned long s_enemyLastTick, s_stateChangeMs;
static bool s_invuln; static unsigned long s_invulnUntil;
static unsigned long s_hitFlashMs;
static unsigned long s_fpsWindowStartMs; static int s_fpsFrameCount; static float s_fpsDisplay;

static bool s_isJoy = false, s_joyActive = false, s_lookActive = false;
static int s_joyAnchorX, s_joyAnchorY, s_joyCurX, s_joyCurY;
static int s_lookLastX, s_lookLastY;
static unsigned long s_lastLookTouchMs = 0;

enum InfState { INF_PLAYING, INF_LEVEL_CLEAR, INF_GAME_OVER };
static InfState s_state;

static lv_color_t* s_canvasBuf = nullptr;
static lv_obj_t* s_canvas;
static lv_obj_t* s_healthBar;
static lv_obj_t* s_hudLbl;
static lv_obj_t* s_fpsLbl;
static lv_obj_t* s_overlay = nullptr;

static void loadProgress() {
  Preferences p; p.begin("inferno", true);
  s_bestScore = p.getInt("best", 0);
  s_maxLevelReached = p.getInt("maxlvl", 1);
  p.end();
  s_progressLoaded = true;
}
static void saveProgress() {
  Preferences p; p.begin("inferno", false);
  p.putInt("best", s_bestScore);
  p.putInt("maxlvl", s_maxLevelReached);
  p.end();
}
static uint16_t weaponColor() {
  if (s_maxLevelReached >= 7) return 0x07FF;
  if (s_maxLevelReached >= 4) return 0xFDA0;
  return GUN_METAL;
}

static char tileAt(int x, int y) {
  if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return '1';
  return kMap[y][x];
}
static bool isWallF(float x, float y) { return tileAt((int)x, (int)y) != '.'; }
static bool canMoveTo(float x, float y) {
  const float pad = 0.2f;
  if (isWallF(x - pad, y) || isWallF(x + pad, y) || isWallF(x, y - pad) || isWallF(x, y + pad)) return false;
  return true;
}
static void tryMove(float dx, float dy) {
  if (canMoveTo(s_posX + dx, s_posY)) s_posX += dx;
  if (canMoveTo(s_posX, s_posY + dy)) s_posY += dy;
}
static void rotatePlayer(float ang) {
  float ndx = s_dirX * cosf(ang) - s_dirY * sinf(ang);
  float ndy = s_dirX * sinf(ang) + s_dirY * cosf(ang);
  float dlen = sqrtf(ndx * ndx + ndy * ndy);
  if (dlen > 0.0001f) { ndx /= dlen; ndy /= dlen; }
  s_dirX = ndx; s_dirY = ndy;
  s_planeX = s_dirY * 0.66f;
  s_planeY = -s_dirX * 0.66f;
}

static void spawnEnemies(int count) {
  const float ex[6] = {2.5f,15.5f,2.5f,15.5f,9.5f,9.5f};
  const float ey[6] = {2.5f,2.5f,13.5f,13.5f,2.5f,13.5f};
  s_enemyCount = count;
  for (int i = 0; i < count; i++) {
    s_enemies[i].x = ex[i]; s_enemies[i].y = ey[i];
    s_enemies[i].alive = true;
    s_enemies[i].hp = 2 + (s_level - 1) / 3;
    s_enemies[i].hitFlashUntil = 0;
  }
}
static bool allEnemiesDead() {
  for (int i = 0; i < s_enemyCount; i++) if (s_enemies[i].alive) return false;
  return true;
}
static void setupLevel() {
  s_posX = 9.5f; s_posY = 2.5f; s_dirX = 0; s_dirY = 1; s_planeX = 0.66f; s_planeY = 0;
  spawnEnemies(min(2 + (s_level - 1), MAX_ENEMIES));
  s_health = s_maxHealth;
  s_state = INF_PLAYING;
  s_invuln = true; s_invulnUntil = millis() + 1000;
  s_enemyLastTick = millis();
}
static void resetGame() {
  if (!s_progressLoaded) loadProgress();
  s_level = 1; s_lives = 3; s_score = 0; s_maxHealth = 100; s_showNewBest = false;
  for (int i = 0; i < RAYS; i++) s_zbuf[i] = 1000.0f;
  setupLevel();
}
static void nextLevel() { s_level++; s_score += 150; setupLevel(); }
static void loseLife() {
  s_lives--;
  if (s_lives <= 0) {
    bool newBest = s_score > s_bestScore;
    if (newBest) s_bestScore = s_score;
    bool newMaxLvl = s_level > s_maxLevelReached;
    if (newMaxLvl) s_maxLevelReached = s_level;
    s_showNewBest = newBest;
    if (newBest || newMaxLvl) saveProgress();
    s_state = INF_GAME_OVER;
  } else {
    s_posX = 9.5f; s_posY = 2.5f; s_dirX = 0; s_dirY = 1; s_planeX = 0.66f; s_planeY = 0;
    s_health = s_maxHealth;
    spawnEnemies(s_enemyCount);
    s_invuln = true; s_invulnUntil = millis() + 1200;
  }
}
static void enemyTick(Enemy& e) {
  if (!e.alive) return;
  float dx = s_posX - e.x, dy = s_posY - e.y;
  float dist = sqrtf(dx * dx + dy * dy);
  if (dist > 0.05f) {
    float speed = 0.045f + (s_level - 1) * 0.004f;
    float mx = (dx / dist) * speed, my = (dy / dist) * speed;
    if (!isWallF(e.x + mx, e.y)) e.x += mx;
    if (!isWallF(e.x, e.y + my)) e.y += my;
  }
  if (dist < 0.55f && !s_invuln && s_state == INF_PLAYING) {
    s_health -= 12 + (s_level - 1) * 1.5f;
    s_invuln = true; s_invulnUntil = millis() + 900;
    s_hitFlashMs = millis();
    if (s_health <= 0) { s_health = 0; loseLife(); }
  }
}
static void fire() {
  if (s_state != INF_PLAYING) return;
  if (millis() - s_lastShotMs < s_fireCooldownMs) return;
  s_lastShotMs = millis();
  s_muzzleFlashUntil = millis() + 90;
  s_gunKick = 1.0f;
  float invDet = 1.0f / (s_planeX * s_dirY - s_dirX * s_planeY);
  int bestI = -1; float bestDist = 1e9f;
  for (int i = 0; i < s_enemyCount; i++) {
    if (!s_enemies[i].alive) continue;
    float ex = s_enemies[i].x - s_posX, ey = s_enemies[i].y - s_posY;
    float dist = sqrtf(ex * ex + ey * ey);
    if (dist < 0.05f || dist >= bestDist) continue;
    float transX = invDet * (s_dirY * ex - s_dirX * ey);
    float transY = invDet * (-s_planeY * ex + s_planeX * ey);
    if (transY <= 0.15f) continue;
    float screenX = (CANVAS_W / 2.0f) * (1.0f + transX / transY);
    if (fabsf(screenX - CANVAS_W / 2.0f) > CANVAS_W * 0.08f) continue;
    int ray = constrain((int)(screenX / COL_W), 0, RAYS - 1);
    if (transY >= s_zbuf[ray]) continue;
    bestDist = dist; bestI = i;
  }
  if (bestI >= 0) {
    s_enemies[bestI].hp--;
    s_enemies[bestI].hitFlashUntil = millis() + 150;
    if (s_enemies[bestI].hp <= 0) {
      s_enemies[bestI].alive = false;
      s_score += 50;
      if (allEnemiesDead()) { s_state = INF_LEVEL_CLEAR; s_stateChangeMs = millis(); }
    }
  }
}

static void showEndOverlay(lv_obj_t* scr) {
  bool over = (s_state == INF_GAME_OVER);
  s_overlay = lv_obj_create(scr);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, over ? 170 : 150, over ? 76 : 48);
  lv_obj_center(s_overlay);
  lv_obj_set_style_radius(s_overlay, 10, 0);
  lv_obj_set_style_bg_color(s_overlay, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_overlay, 2, 0);
  lv_obj_set_style_border_color(s_overlay, c565(over ? T().danger : T().good), 0);
  lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (over) {
    lv_obj_t* t1 = lv_label_create(s_overlay);
    lv_label_set_text(t1, "KAU TEWAS");
    lv_obj_set_style_text_color(t1, c565(T().danger), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);
    lv_obj_t* t2 = lv_label_create(s_overlay);
    char lb[32]; snprintf(lb, sizeof(lb), "Skor: %d (Lv%d)", s_score, s_level);
    lv_label_set_text(t2, lb);
    lv_obj_set_style_text_color(t2, c565(T().text), 0);
    lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, 0);
    if (s_showNewBest) {
      lv_obj_t* t3 = lv_label_create(s_overlay);
      lv_label_set_text(t3, "Rekor baru!");
      lv_obj_set_style_text_color(t3, c565(T().good), 0);
      lv_obj_set_style_text_font(t3, &lv_font_montserrat_14, 0);
    }
    lv_obj_t* t4 = lv_label_create(s_overlay);
    lv_label_set_text(t4, "Ketuk layar utk main lagi");
    lv_obj_set_style_text_color(t4, c565(T().subtext), 0);
    lv_obj_set_style_text_font(t4, &lv_font_montserrat_14, 0);
  } else {
    lv_obj_t* t1 = lv_label_create(s_overlay);
    char lb[24]; snprintf(lb, sizeof(lb), "Zona Bersih! Lv%d", s_level);
    lv_label_set_text(t1, lb);
    lv_obj_set_style_text_color(t1, c565(T().good), 0);
    lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);
  }
}

static void redraw() {
  unsigned long now = millis();
  if (s_fpsWindowStartMs == 0) s_fpsWindowStartMs = now;
  s_fpsFrameCount++;
  if (now - s_fpsWindowStartMs >= 500) {
    s_fpsDisplay = s_fpsFrameCount * 1000.0f / (float)(now - s_fpsWindowStartMs);
    s_fpsFrameCount = 0; s_fpsWindowStartMs = now;
  }

  int vpHalf = CANVAS_H / 2;
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_COVER;

  const int BANDS = 8;
  for (int i = 0; i < BANDS; i++) {
    int y0 = (vpHalf * i) / BANDS, y1 = (vpHalf * (i + 1)) / BANDS;
    uint8_t a = (uint8_t)(i * 255 / (BANDS - 1));
    dsc.bg_color = c565(blend565(CEIL_FAR, CEIL_NEAR, a));
    lv_canvas_draw_rect(s_canvas, 0, y0, CANVAS_W, max(1, y1 - y0), &dsc);
  }
  for (int i = 0; i < BANDS; i++) {
    int y0 = vpHalf + (vpHalf * i) / BANDS, y1 = vpHalf + (vpHalf * (i + 1)) / BANDS;
    uint8_t a = (uint8_t)(i * 255 / (BANDS - 1));
    dsc.bg_color = c565(blend565(FLOOR_NEAR, FLOOR_FAR, a));
    lv_canvas_draw_rect(s_canvas, 0, y0, CANVAS_W, max(1, y1 - y0), &dsc);
  }

  // ---- Dinding: DDA, 1 sinar tiap COL_W piksel ----
  for (int ray = 0; ray < RAYS; ray++) {
    int x = ray * COL_W;
    float cameraX = 2.0f * x / (float)CANVAS_W - 1.0f;
    float rayDirX = s_dirX + s_planeX * cameraX;
    float rayDirY = s_dirY + s_planeY * cameraX;
    int mapX = (int)s_posX, mapY = (int)s_posY;
    float deltaDistX = (rayDirX == 0) ? 1e30f : fabsf(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30f : fabsf(1.0f / rayDirY);
    int stepX, stepY; float sideDistX, sideDistY;
    if (rayDirX < 0) { stepX = -1; sideDistX = (s_posX - mapX) * deltaDistX; } else { stepX = 1; sideDistX = (mapX + 1.0f - s_posX) * deltaDistX; }
    if (rayDirY < 0) { stepY = -1; sideDistY = (s_posY - mapY) * deltaDistY; } else { stepY = 1; sideDistY = (mapY + 1.0f - s_posY) * deltaDistY; }
    int side = 0; char hitTile = '1';
    for (int guard = 0; guard < 64; guard++) {
      if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
      else { sideDistY += deltaDistY; mapY += stepY; side = 1; }
      hitTile = tileAt(mapX, mapY);
      if (hitTile != '.') break;
    }
    float perpDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
    if (perpDist < 0.05f) perpDist = 0.05f;
    s_zbuf[ray] = perpDist;

    int lineH = (int)(CANVAS_H / perpDist);
    int drawStart = vpHalf - lineH / 2, drawEnd = drawStart + lineH;
    if (drawStart < 0) drawStart = 0;
    if (drawEnd > CANVAS_H) drawEnd = CANVAS_H;

    float hitX = (side == 0) ? (s_posY + perpDist * rayDirY) : (s_posX + perpDist * rayDirX);
    hitX -= floorf(hitX);

    uint16_t baseCol;
    switch (hitTile) {
      case '2': { bool mortarV = fmodf(hitX * 5.0f, 1.0f) < 0.07f; baseCol = mortarV ? blend565(WALL_BRICK, 0x0000, 120) : WALL_BRICK; break; }
      case '3': { baseCol = (fmodf(hitX * 6.0f, 2.0f) < 1.0f) ? WALL_HAZARD_A : WALL_HAZARD_B; break; }
      case '4': { float pulse = 0.5f + 0.5f * sinf(millis() * 0.004f + mapX * 0.7f + mapY * 0.5f); baseCol = blend565(WALL_FLESH, 0xF800, (uint8_t)(pulse * 45.0f)); break; }
      default: {
        float pf = fmodf(hitX * 3.0f, 1.0f);
        if (pf < 0.06f) baseCol = blend565(WALL_METAL, 0x0000, 130);
        else if (pf > 0.46f && pf < 0.54f) baseCol = blend565(WALL_METAL, 0xFFFF, 60);
        else baseCol = WALL_METAL;
        break;
      }
    }
    if (side == 1) baseCol = blend565(baseCol, 0x0000, 55);

    if (millis() < s_muzzleFlashUntil) {
      float flashT = (float)(s_muzzleFlashUntil - millis()) / 90.0f;
      if (flashT > 1.0f) flashT = 1.0f;
      float wglow = constrain(1.0f - perpDist / 3.0f, 0.0f, 1.0f) * flashT;
      if (wglow > 0.0f) baseCol = blend565(baseCol, 0xFFFF, (uint8_t)(wglow * 90.0f));
    }

    uint8_t fog = (uint8_t)constrain(perpDist * 22.0f, 0.0f, 205.0f);
    uint16_t finalCol = blend565(baseCol, FOG_COLOR, fog);

    if (drawEnd > drawStart) {
      dsc.bg_color = c565(finalCol);
      lv_canvas_draw_rect(s_canvas, x, drawStart, COL_W, drawEnd - drawStart, &dsc);
    }
  }

  // ---- Musuh (billboard, terjauh->terdekat) ----
  int order[MAX_ENEMIES], oc = 0;
  for (int i = 0; i < s_enemyCount; i++) if (s_enemies[i].alive) order[oc++] = i;
  for (int a = 0; a < oc; a++) {
    int maxI = a; float maxD = -1;
    for (int b = a; b < oc; b++) {
      float ex = s_enemies[order[b]].x - s_posX, ey = s_enemies[order[b]].y - s_posY;
      float d = ex * ex + ey * ey;
      if (d > maxD) { maxD = d; maxI = b; }
    }
    int t = order[a]; order[a] = order[maxI]; order[maxI] = t;
  }
  float invDet = 1.0f / (s_planeX * s_dirY - s_dirX * s_planeY);
  for (int k = 0; k < oc; k++) {
    Enemy& e = s_enemies[order[k]];
    float ex = e.x - s_posX, ey = e.y - s_posY;
    float transX = invDet * (s_dirY * ex - s_dirX * ey);
    float transY = invDet * (-s_planeY * ex + s_planeX * ey);
    if (transY <= 0.1f) continue;
    int scrX = (int)((CANVAS_W / 2.0f) * (1.0f + transX / transY));
    int spH = abs((int)(CANVAS_H / transY));
    if (scrX < -spH / 2 || scrX > CANVAS_W + spH / 2) continue;
    int rayIdx = constrain(scrX / COL_W, 0, RAYS - 1);
    if (transY >= s_zbuf[rayIdx]) continue; // ketutup dinding -- skip (occlusion disederhanakan, cek 1 titik tengah)

    bool flash = millis() < e.hitFlashUntil;
    uint16_t bodyC = flash ? 0xFFFF : ENEMY_BODY;
    uint16_t darkC = flash ? 0xFCE0 : ENEMY_DARK;
    int bw = spH / 3, bh = spH / 2, headR = max(2, spH / 8);
    dsc.radius = 3;
    dsc.bg_color = c565(bodyC);
    lv_canvas_draw_rect(s_canvas, scrX - bw / 2, vpHalf, bw, bh, &dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(s_canvas, scrX - headR, vpHalf - headR * 2, headR * 2, headR * 2, &dsc);
    dsc.bg_color = c565(darkC);
    int eyeR = max(1, headR / 3);
    lv_canvas_draw_rect(s_canvas, scrX - headR / 2 - eyeR, vpHalf - headR * 2 - eyeR / 2, eyeR * 2, eyeR * 2, &dsc);
    lv_canvas_draw_rect(s_canvas, scrX + headR / 2 - eyeR, vpHalf - headR * 2 - eyeR / 2, eyeR * 2, eyeR * 2, &dsc);
    dsc.radius = 0;
  }

  // ---- Crosshair ----
  dsc.bg_color = c565(0xFFFF); dsc.radius = 0;
  lv_canvas_draw_rect(s_canvas, CANVAS_W / 2 - 6, vpHalf - 1, 13, 2, &dsc);
  lv_canvas_draw_rect(s_canvas, CANVAS_W / 2 - 1, vpHalf - 6, 2, 13, &dsc);

  // ---- Senjata (viewmodel) ----
  int gunY = CANVAS_H - (int)(s_gunKick * 8);
  dsc.bg_color = c565(GUN_METAL); dsc.radius = 4;
  lv_canvas_draw_rect(s_canvas, CANVAS_W / 2 - 14, gunY - 26, 28, 30, &dsc);
  dsc.bg_color = c565(GUN_DARK); dsc.radius = 3;
  lv_canvas_draw_rect(s_canvas, CANVAS_W / 2 - 5, gunY - 44, 10, 22, &dsc);
  dsc.bg_color = c565(weaponColor()); dsc.radius = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(s_canvas, CANVAS_W / 2 - 4, gunY - 48, 8, 8, &dsc);

  // ---- Tombol tembak (lingkaran kanan-bawah, posisi tetap) ----
  dsc.bg_color = c565(T().danger); dsc.radius = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(s_canvas, CANVAS_W - 26 - 20, CANVAS_H - 26 - 20, 40, 40, &dsc);

  // ---- Vignette merah singkat pas abis kena hit ----
  if (s_hitFlashMs != 0) {
    unsigned long sinceHit = millis() - s_hitFlashMs;
    if (sinceHit < 300) {
      uint8_t a = (uint8_t)(90 * (1.0f - (float)sinceHit / 300.0f));
      dsc.bg_color = c565(0xF800); dsc.radius = 0; dsc.bg_opa = a;
      lv_canvas_draw_rect(s_canvas, 0, 0, CANVAS_W, 10, &dsc);
      lv_canvas_draw_rect(s_canvas, 0, CANVAS_H - 10, CANVAS_W, 10, &dsc);
      dsc.bg_opa = LV_OPA_COVER;
    }
  }

  // ---- HUD teks ----
  char hb[64];
  snprintf(hb, sizeof(hb), "Lv%d  Nyawa:%d  Skor:%d  Terbaik:%d", s_level, s_lives, s_score, s_bestScore);
  lv_label_set_text(s_hudLbl, hb);
  lv_bar_set_value(s_healthBar, (int)s_health, LV_ANIM_OFF);
  char fb[16]; snprintf(fb, sizeof(fb), "%.0f fps", s_fpsDisplay);
  lv_label_set_text(s_fpsLbl, fb);
  uint16_t fpsCol = s_fpsDisplay >= 18 ? T().good : s_fpsDisplay >= 10 ? T().accent : T().danger;
  lv_obj_set_style_text_color(s_fpsLbl, c565(fpsCol), 0);
}

static void gameTick(lv_timer_t* t) {
  if (s_state == INF_PLAYING) {
    if (s_invuln && millis() > s_invulnUntil) s_invuln = false;
    if (g_mpuReady && millis() - s_lastLookTouchMs > 150) {
      rotatePlayer(constrain(g_smoothRoll, -30.0f, 30.0f) * 0.00035f);
    }
    if (s_joyActive && s_isJoy) {
      float ox = (float)(s_joyCurX - s_joyAnchorX), oy = (float)(s_joyCurY - s_joyAnchorY);
      float mag = sqrtf(ox * ox + oy * oy);
      if (mag > JOY_RADIUS) { ox *= JOY_RADIUS / mag; oy *= JOY_RADIUS / mag; mag = JOY_RADIUS; }
      if (mag > 6.0f) {
        float nx = ox / JOY_RADIUS, ny = oy / JOY_RADIUS;
        float fwd = -ny, strafe = nx;
        float speed = 0.085f;
        float rightX = s_dirY, rightY = -s_dirX;
        float mx = (s_dirX * fwd + rightX * strafe) * speed;
        float my = (s_dirY * fwd + rightY * strafe) * speed;
        tryMove(mx, my);
      }
    }
    if (millis() - s_enemyLastTick >= 90) {
      for (int i = 0; i < s_enemyCount; i++) enemyTick(s_enemies[i]);
      s_enemyLastTick = millis();
    }
    if (s_gunKick > 0) { s_gunKick -= 0.06f; if (s_gunKick < 0) s_gunKick = 0; }
  } else if (s_state == INF_LEVEL_CLEAR) {
    if (millis() - s_stateChangeMs > 1200) nextLevel();
  }

  redraw();

  static InfState lastState = INF_PLAYING;
  if (s_state != INF_PLAYING && (!s_overlay || lastState != s_state)) {
    if (s_overlay) lv_obj_del(s_overlay);
    s_overlay = nullptr;
    showEndOverlay(lv_obj_get_parent(s_canvas));
  } else if (s_state == INF_PLAYING && s_overlay) {
    lv_obj_del(s_overlay);
    s_overlay = nullptr;
  }
  lastState = s_state;
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
  if (s_state == INF_GAME_OVER) { resetGame(); return; }
  if (s_state != INF_PLAYING) return;
  int x, y; if (!localCoord(e, &x, &y)) return;
  int fcx = CANVAS_W - 26, fcy = CANVAS_H - 26, fr = 20;
  int fdx = x - fcx, fdy = y - fcy;
  if (fdx * fdx + fdy * fdy <= fr * fr) { fire(); return; }
  if (x < CANVAS_W / 2) {
    s_isJoy = true; s_joyActive = true;
    s_joyAnchorX = x; s_joyAnchorY = y; s_joyCurX = x; s_joyCurY = y;
  } else {
    s_isJoy = false; s_lookActive = true;
    s_lookLastX = x; s_lookLastY = y; s_lastLookTouchMs = millis();
  }
}
static void canvas_pressing_cb(lv_event_t* e) {
  if (s_state != INF_PLAYING) return;
  int x, y; if (!localCoord(e, &x, &y)) return;
  if (s_isJoy && s_joyActive) {
    s_joyCurX = x; s_joyCurY = y;
  } else if (s_lookActive) {
    int dx = x - s_lookLastX, dy = y - s_lookLastY;
    rotatePlayer(dx * 0.0045f);
    s_lookLastX = x; s_lookLastY = y;
    s_lastLookTouchMs = millis();
  }
}
static void canvas_released_cb(lv_event_t* e) {
  s_joyActive = false; s_lookActive = false; s_isJoy = false;
}

void infernoScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen("");

  s_hudLbl = lv_label_create(scr);
  lv_obj_set_pos(s_hudLbl, 6, 25);
  lv_obj_set_style_text_color(s_hudLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(s_hudLbl, &lv_font_montserrat_14, 0);

  s_fpsLbl = lv_label_create(scr);
  lv_obj_set_pos(s_fpsLbl, 320 - 50, 25);
  lv_obj_set_style_text_font(s_fpsLbl, &lv_font_montserrat_14, 0);

  s_healthBar = lv_bar_create(scr);
  lv_obj_set_size(s_healthBar, 100, 8);
  lv_obj_set_pos(s_healthBar, 6, 195);
  lv_bar_set_range(s_healthBar, 0, 100);
  lv_obj_set_style_bg_color(s_healthBar, c565(T().surface2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_healthBar, c565(T().danger), LV_PART_INDICATOR);

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
  lv_obj_add_event_cb(s_canvas, canvas_released_cb, LV_EVENT_RELEASED, NULL);

  s_overlay = nullptr;
  s_fpsWindowStartMs = 0; s_fpsFrameCount = 0; s_fpsDisplay = 0;
  s_hitFlashMs = 0;
  resetGame();
  redraw();
  lv_timer_t* timer = lv_timer_create(gameTick, 50, NULL); // ~20fps target
  chromeBindTimerToScreen(scr, timer);
}
