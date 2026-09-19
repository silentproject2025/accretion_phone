#include "app_tictactoe.h"
#include "screen_chrome.h"
#include "theme.h"
#include <Arduino.h>

#define TTT_MAX_N 5

static char s_board[TTT_MAX_N * TTT_MAX_N];
static bool s_over = false;
static char s_winner = ' ';
static int  s_n = 3;
static int  s_mode = 0;   // 0=vs CPU, 1=2 Pemain
static char s_turn = 'X';
static bool s_setup = true;

static void rebuildSelf() {
  tictactoeScreenShow();
}

static void resetBoard() {
  for (int i = 0; i < s_n * s_n; i++) s_board[i] = ' ';
  s_over = false; s_winner = ' '; s_turn = 'X';
}

// =============================================
// Logika menang & AI -- disalin PERSIS dari tttCheckWin()/tttAiMove() file
// lama, cuma nama variabel disesuaikan (tttBoard -> s_board, dst).
// =============================================
static char lineWinner(int* idxs, int n) {
  char first = s_board[idxs[0]];
  if (first == ' ') return ' ';
  for (int k = 1; k < n; k++) if (s_board[idxs[k]] != first) return ' ';
  return first;
}
static char checkWin() {
  int n = s_n, line[TTT_MAX_N];
  for (int r = 0; r < n; r++) { for (int c = 0; c < n; c++) line[c] = r * n + c; char w = lineWinner(line, n); if (w != ' ') return w; }
  for (int c = 0; c < n; c++) { for (int r = 0; r < n; r++) line[r] = r * n + c; char w = lineWinner(line, n); if (w != ' ') return w; }
  for (int k = 0; k < n; k++) line[k] = k * n + k; { char w = lineWinner(line, n); if (w != ' ') return w; }
  for (int k = 0; k < n; k++) line[k] = k * n + (n - 1 - k); { char w = lineWinner(line, n); if (w != ' ') return w; }
  bool full = true; for (int i = 0; i < n * n; i++) if (s_board[i] == ' ') full = false;
  if (full) return 'D';
  return ' ';
}
static bool lineNearWin(int* idxs, int n, char me, int& emptyIdx) {
  int emptyCount = 0, foundEmpty = -1;
  for (int k = 0; k < n; k++) {
    char v = s_board[idxs[k]];
    if (v == ' ') { emptyCount++; foundEmpty = idxs[k]; if (emptyCount > 1) return false; }
    else if (v != me) return false;
  }
  if (emptyCount == 1) { emptyIdx = foundEmpty; return true; }
  return false;
}
static void aiMove() {
  int n = s_n, line[TTT_MAX_N];
  for (int pass = 0; pass < 2; pass++) {
    char me = pass == 0 ? 'O' : 'X';
    for (int r = 0; r < n; r++) { for (int c = 0; c < n; c++) line[c] = r * n + c; int e; if (lineNearWin(line, n, me, e)) { s_board[e] = 'O'; return; } }
    for (int c = 0; c < n; c++) { for (int r = 0; r < n; r++) line[r] = r * n + c; int e; if (lineNearWin(line, n, me, e)) { s_board[e] = 'O'; return; } }
    for (int k = 0; k < n; k++) line[k] = k * n + k; { int e; if (lineNearWin(line, n, me, e)) { s_board[e] = 'O'; return; } }
    for (int k = 0; k < n; k++) line[k] = k * n + (n - 1 - k); { int e; if (lineNearWin(line, n, me, e)) { s_board[e] = 'O'; return; } }
  }
  int cx = n / 2, cy = n / 2;
  if (s_board[cy * n + cx] == ' ') { s_board[cy * n + cx] = 'O'; return; }
  if (n % 2 == 0) {
    int cands[4] = { (cy - 1) * n + (cx - 1), (cy - 1) * n + cx, cy * n + (cx - 1), cy * n + cx };
    for (int i = 0; i < 4; i++) if (s_board[cands[i]] == ' ') { s_board[cands[i]] = 'O'; return; }
  }
  int empty[TTT_MAX_N * TTT_MAX_N], cnt = 0;
  for (int i = 0; i < n * n; i++) if (s_board[i] == ' ') empty[cnt++] = i;
  if (cnt > 0) s_board[empty[random(0, cnt)]] = 'O';
}

// ---------- Layar SETUP (pilih mode & ukuran) ----------
static void mode_btn_cb(lv_event_t* e) {
  s_mode = (int)(intptr_t)lv_event_get_user_data(e);
  rebuildSelf();
}
static void size_btn_cb(lv_event_t* e) {
  s_n = (int)(intptr_t)lv_event_get_user_data(e);
  rebuildSelf();
}
static void start_btn_cb(lv_event_t* e) {
  resetBoard();
  s_setup = false;
  rebuildSelf();
}

static void buildSetupUI(lv_obj_t* scr) {
  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 44);
  lv_obj_set_size(content, 320, 207 - 44);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 8, 0);
  lv_obj_set_style_pad_row(content, 14, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* modeLbl = lv_label_create(content);
  lv_label_set_text(modeLbl, "Mode:");
  lv_obj_set_style_text_color(modeLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(modeLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* modeRow = lv_obj_create(content);
  lv_obj_remove_style_all(modeRow);
  lv_obj_set_size(modeRow, 300, 30);
  lv_obj_set_flex_flow(modeRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(modeRow, 8, 0);
  const char* modeLabels[2] = {"vs CPU", "2 Pemain"};
  for (int i = 0; i < 2; i++) {
    lv_obj_t* b = lv_obj_create(modeRow);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 28);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_bg_color(b, c565(i == s_mode ? T().accent : T().surface2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, mode_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, modeLabels[i]);
    lv_obj_set_style_text_color(l, c565(i == s_mode ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  lv_obj_t* sizeLbl = lv_label_create(content);
  lv_label_set_text(sizeLbl, "Ukuran papan (= tingkat kesulitan):");
  lv_obj_set_style_text_color(sizeLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(sizeLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* sizeRow = lv_obj_create(content);
  lv_obj_remove_style_all(sizeRow);
  lv_obj_set_size(sizeRow, 300, 30);
  lv_obj_set_flex_flow(sizeRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(sizeRow, 8, 0);
  for (int i = 0; i < 3; i++) {
    int n = i + 3; // 3,4,5
    lv_obj_t* b = lv_obj_create(sizeRow);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 28);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_bg_color(b, c565(n == s_n ? T().accent : T().surface2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, size_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)n);
    lv_obj_t* l = lv_label_create(b);
    char buf[8]; snprintf(buf, sizeof(buf), "%dx%d", n, n);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_color(l, c565(n == s_n ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  lv_obj_t* startBtn = lv_obj_create(content);
  lv_obj_remove_style_all(startBtn);
  lv_obj_set_size(startBtn, 300, 34);
  lv_obj_set_style_radius(startBtn, 8, 0);
  lv_obj_set_style_bg_color(startBtn, c565(T().accent), 0);
  lv_obj_set_style_bg_opa(startBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(startBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(startBtn, start_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* startLbl = lv_label_create(startBtn);
  lv_label_set_text(startLbl, "Mulai");
  lv_obj_set_style_text_color(startLbl, c565(T().bg), 0);
  lv_obj_center(startLbl);
}

// ---------- Layar MAIN (papan) ----------
static void cell_click_cb(lv_event_t* e) {
  if (s_over) { resetBoard(); rebuildSelf(); return; }
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (s_board[idx] != ' ') return;

  if (s_mode == 0) { // vs CPU: user selalu 'X'
    s_board[idx] = 'X';
    char w = checkWin();
    if (w != ' ') { s_over = true; s_winner = w; rebuildSelf(); return; }
    aiMove();
    w = checkWin();
    if (w != ' ') { s_over = true; s_winner = w; }
  } else { // PvP
    s_board[idx] = s_turn;
    char w = checkWin();
    if (w != ' ') { s_over = true; s_winner = w; rebuildSelf(); return; }
    s_turn = (s_turn == 'X') ? 'O' : 'X';
  }
  rebuildSelf();
}

static void change_btn_cb(lv_event_t* e) {
  s_setup = true;
  rebuildSelf();
}

static void buildGameUI(lv_obj_t* scr) {
  // Judul dinamis: mode + (giliran kalau PvP) -- di atas title chrome bawaan
  // kita timpa dgn label sendiri krn butuh teks dinamis per state.
  lv_obj_t* titleLbl = lv_label_create(scr);
  char title[32];
  if (s_mode == 0) snprintf(title, sizeof(title), "TTT %dx%d - vs CPU", s_n, s_n);
  else snprintf(title, sizeof(title), "TTT %dx%d - Giliran %c", s_n, s_n, s_turn);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_color(titleLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(titleLbl, 8, 26);

  // Tombol "Ganti" -- balik ke setup kapan saja.
  lv_obj_t* changeBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(changeBtn);
  lv_obj_set_size(changeBtn, 58, 18);
  lv_obj_set_pos(changeBtn, 320 - 64, 24);
  lv_obj_set_style_radius(changeBtn, 4, 0);
  lv_obj_set_style_bg_color(changeBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(changeBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(changeBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(changeBtn, change_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* changeLbl = lv_label_create(changeBtn);
  lv_label_set_text(changeLbl, "Ganti");
  lv_obj_set_style_text_color(changeLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(changeLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(changeLbl);

  // Papan -- grid n x n, cell persegi, ukuran menyesuaikan sisa ruang.
  int availH = 207 - 48;
  int cellSize = min(280 / s_n, availH / s_n);
  if (cellSize > 56) cellSize = 56;

  lv_obj_t* board = lv_obj_create(scr);
  lv_obj_remove_style_all(board);
  lv_obj_set_size(board, cellSize * s_n, cellSize * s_n);
  lv_obj_align(board, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_set_flex_flow(board, LV_FLEX_FLOW_ROW_WRAP);
  // Gap antar cell pakai pad_row/pad_column punya PARENT (board) -- LVGL v8
  // gak punya "margin" per-objek spt CSS, jadi gap antar flex-child diatur
  // dari sisi container, bukan dari cell-nya sendiri.
  lv_obj_set_style_pad_row(board, 2, 0);
  lv_obj_set_style_pad_column(board, 2, 0);
  lv_obj_clear_flag(board, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < s_n * s_n; i++) {
    lv_obj_t* cell = lv_obj_create(board);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, cellSize - 2, cellSize - 2);
    lv_obj_set_style_radius(cell, 6, 0);
    lv_obj_set_style_bg_color(cell, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cell, cell_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    if (s_board[i] != ' ') {
      lv_obj_t* mark = lv_label_create(cell);
      char mb[2] = { s_board[i], 0 };
      lv_label_set_text(mark, mb);
      lv_obj_set_style_text_color(mark, c565(s_board[i] == 'X' ? T().accent : T().danger), 0);
      lv_obj_set_style_text_font(mark, &lv_font_montserrat_20, 0);
      lv_obj_center(mark);
    }
  }

  if (s_over) {
    const char* msg;
    if (s_winner == 'D') msg = "Seri!";
    else if (s_mode == 0) msg = (s_winner == 'X') ? "Kamu Menang!" : "CPU Menang!";
    else msg = (s_winner == 'X') ? "X Menang!" : "O Menang!";

    lv_obj_t* overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 160, 50);
    lv_obj_align(overlay, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_set_style_radius(overlay, 10, 0);
    lv_obj_set_style_bg_color(overlay, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_border_color(overlay, c565(T().accent), 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* msgLbl = lv_label_create(overlay);
    lv_label_set_text(msgLbl, msg);
    lv_obj_set_style_text_color(msgLbl, c565(T().text), 0);
    lv_obj_set_style_text_font(msgLbl, &lv_font_montserrat_14, 0);

    lv_obj_t* hintLbl = lv_label_create(overlay);
    lv_label_set_text(hintLbl, "Ketuk papan utk main lagi");
    lv_obj_set_style_text_color(hintLbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(hintLbl, &lv_font_montserrat_14, 0);
  }
}

void tictactoeScreenShow() {
  // Layar game pakai judul teks DINAMIS (mode+giliran) yg dibikin sendiri
  // di buildGameUI() -- judul bawaan chrome sengaja dikosongkan di situ
  // (kalau dikasih "Tic-Tac-Toe" di sini, bakal numpuk/tabrakan sama label
  // dinamis yg posisinya persis sama).
  lv_obj_t* scr = chromeCreateAppScreen(s_setup ? "Tic-Tac-Toe" : "");
  if (s_setup) buildSetupUI(scr);
  else buildGameUI(scr);
}
