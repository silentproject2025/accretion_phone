#include "app_2048.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>
#include <cstring>

static int  s_board[4][4];
static int  s_score = 0;
static bool s_over = false, s_win = false;

static void buildUI(); // forward decl

// Dipakai utk rebuild layar SETELAH state berubah (gerakan/restart) --
// TIDAK reset papan. Beda dgn game2048ScreenShow() (entry point publik dari
// Home) yg SELALU reset papan, sama spt g2048Enter() manggil g2048Reset()
// tiap kali app ini dibuka dari Home di file lama.
static void rebuildSelf() {
  lv_obj_t* prev = lv_scr_act();
  buildUI();
  lv_obj_del_async(prev);
}

static void addRandom() {
  int emptyR[16], emptyC[16], n = 0;
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (s_board[r][c] == 0) { emptyR[n] = r; emptyC[n] = c; n++; }
  if (n == 0) return;
  int pick = random(0, n);
  s_board[emptyR[pick]][emptyC[pick]] = (random(0, 10) < 9) ? 2 : 4;
}
static void resetGame() {
  memset(s_board, 0, sizeof(s_board));
  s_score = 0; s_over = false; s_win = false;
  addRandom(); addRandom();
}
static bool compressMergeLeft() {
  bool moved = false;
  for (int r = 0; r < 4; r++) {
    int line[4], n = 0;
    for (int c = 0; c < 4; c++) if (s_board[r][c] != 0) line[n++] = s_board[r][c];
    for (int i = 0; i < n - 1; i++) {
      if (line[i] == line[i + 1]) {
        line[i] *= 2; s_score += line[i];
        if (line[i] == 2048) s_win = true;
        for (int k = i + 1; k < n - 1; k++) line[k] = line[k + 1];
        n--;
      }
    }
    for (int c = 0; c < 4; c++) {
      int v = c < n ? line[c] : 0;
      if (s_board[r][c] != v) moved = true;
      s_board[r][c] = v;
    }
  }
  return moved;
}
static void rotateCW() {
  int tmp[4][4];
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) tmp[c][3 - r] = s_board[r][c];
  memcpy(s_board, tmp, sizeof(tmp));
}
static bool boardFull() {
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (s_board[r][c] == 0) return false;
  return true;
}
static bool hasMove() {
  if (!boardFull()) return true;
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
    int v = s_board[r][c];
    if (c < 3 && s_board[r][c + 1] == v) return true;
    if (r < 3 && s_board[r + 1][c] == v) return true;
  }
  return false;
}
// dir: 0=kiri, 1=atas, 2=kanan, 3=bawah -- sama encoding file lama.
static void doMove(int dir) {
  if (s_over) return;
  for (int i = 0; i < dir; i++) rotateCW();
  bool moved = compressMergeLeft();
  for (int i = 0; i < (4 - dir) % 4; i++) rotateCW();
  if (moved) {
    addRandom();
    if (!hasMove()) s_over = true;
  }
}

static void restart_click_cb(lv_event_t* e) {
  resetGame();
  rebuildSelf();
}

static void gesture_cb(lv_event_t* e) {
  if (s_over || s_win) { resetGame(); rebuildSelf(); return; }
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  lv_indev_wait_release(indev); // cegah 1 swipe kepicu berkali-kali
  int mv;
  switch (dir) {
    case LV_DIR_LEFT:  mv = 0; break;
    case LV_DIR_TOP:   mv = 1; break;
    case LV_DIR_RIGHT: mv = 2; break;
    case LV_DIR_BOTTOM:mv = 3; break;
    default: return;
  }
  doMove(mv);
  rebuildSelf();
}

void game2048ScreenShow() {
  resetGame(); // entry point publik (dari Home) -- selalu mulai papan baru
  buildUI();
}

static void buildUI() {
  // Judul bawaan chrome sengaja dikosongkan -- sama spt file lama, slot
  // judul di sini dipakai buat "Skor: N" (bukan nama app), jadi kalau
  // dikasih "2048" di sini bakal numpuk sama label skor di posisi yg sama.
  lv_obj_t* scr = chromeCreateAppScreen("");
  lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

  char sb[24]; snprintf(sb, sizeof(sb), "Skor: %d", s_score);
  lv_obj_t* scoreLbl = lv_label_create(scr);
  lv_label_set_text(scoreLbl, sb);
  lv_obj_set_style_text_color(scoreLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(scoreLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(scoreLbl, 8, 26);

  const int cell = 44, gap = 5;
  int boardW = 4 * cell + 5 * gap;

  lv_obj_t* boardBg = lv_obj_create(scr);
  lv_obj_remove_style_all(boardBg);
  lv_obj_set_size(boardBg, boardW, boardW);
  lv_obj_align(boardBg, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_set_style_radius(boardBg, 8, 0);
  lv_obj_set_style_bg_color(boardBg, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(boardBg, LV_OPA_COVER, 0);
  lv_obj_clear_flag(boardBg, LV_OBJ_FLAG_SCROLLABLE);
  // SENGAJA tidak dikasih LV_OBJ_FLAG_CLICKABLE: biar objek ini "transparan"
  // ke input, jadi swipe di atas papan tetap kena `scr` (yg pasang
  // gesture_cb), bukan ketelen papan/cell-nya sendiri yg gak punya handler.

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int v = s_board[r][c];
      uint16_t cc = (v == 0) ? T().surface2 : (v <= 4 ? T().good : v <= 32 ? T().accent : v <= 256 ? T().accent2 : T().danger);
      lv_obj_t* cellObj = lv_obj_create(boardBg);
      lv_obj_remove_style_all(cellObj);
      lv_obj_set_size(cellObj, cell, cell);
      lv_obj_set_pos(cellObj, gap + c * (cell + gap), gap + r * (cell + gap));
      lv_obj_set_style_radius(cellObj, 6, 0);
      lv_obj_set_style_bg_color(cellObj, c565(cc), 0);
      lv_obj_set_style_bg_opa(cellObj, LV_OPA_COVER, 0);
      if (v > 0) {
        lv_obj_t* vl = lv_label_create(cellObj);
        char vb[6]; snprintf(vb, sizeof(vb), "%d", v);
        lv_label_set_text(vl, vb);
        lv_obj_set_style_text_color(vl, c565(v <= 4 ? T().bg : 0xFFFF), 0);
        lv_obj_set_style_text_font(vl, &lv_font_montserrat_20, 0);
        lv_obj_center(vl);
      }
    }
  }

  if (s_over || s_win) {
    lv_obj_t* overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 160, 60);
    lv_obj_center(overlay);
    lv_obj_set_style_radius(overlay, 10, 0);
    lv_obj_set_style_bg_color(overlay, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_border_color(overlay, c565(s_win ? T().good : T().danger), 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay, restart_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* msgLbl = lv_label_create(overlay);
    lv_label_set_text(msgLbl, s_win ? "KAMU MENANG!" : "GAME OVER");
    lv_obj_set_style_text_color(msgLbl, c565(s_win ? T().good : T().danger), 0);
    lv_obj_set_style_text_font(msgLbl, &lv_font_montserrat_14, 0);

    lv_obj_t* hintLbl = lv_label_create(overlay);
    lv_label_set_text(hintLbl, "Ketuk utk main lagi");
    lv_obj_set_style_text_color(hintLbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_14, 0);
  }
}
