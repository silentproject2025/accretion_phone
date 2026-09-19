#include "app_trivia.h"
#include "screen_chrome.h"
#include "theme.h"
#include "http_client.h"
#include "wifi_manager.h"
#include "sys_state.h"
#include <Arduino.h>

// =============================================
// Parser base64/HTML-entity/JSON manual -- disalin PERSIS dari file lama
// (triviaB64Decode/triviaHtmlDecode/triviaJsonGetXxx/dst). Ini BUKAN
// parser JSON umum, khusus bentuk respons OpenTDB.
// =============================================
static int triviaB64Val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}
static String triviaB64Decode(const String& in) {
  String out; out.reserve(in.length() * 3 / 4 + 4);
  int val = 0, bits = -8;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '=') break;
    int d = triviaB64Val(c);
    if (d < 0) continue;
    val = (val << 6) + d; bits += 6;
    if (bits >= 0) { out += (char)((val >> bits) & 0xFF); bits -= 8; }
  }
  return out;
}
static String triviaHtmlDecode(String s) {
  struct Ent { const char* pat; const char* rep; };
  static const Ent ents[] = {
    {"&quot;","\""}, {"&#039;","'"}, {"&apos;","'"}, {"&rsquo;","'"}, {"&lsquo;","'"},
    {"&rdquo;","\""}, {"&ldquo;","\""}, {"&ndash;","-"}, {"&mdash;","-"}, {"&hellip;","..."},
    {"&eacute;","e"}, {"&egrave;","e"}, {"&ecirc;","e"}, {"&uuml;","u"}, {"&ouml;","o"},
    {"&auml;","a"}, {"&ntilde;","n"}, {"&ccedil;","c"}, {"&aacute;","a"}, {"&iacute;","i"},
    {"&oacute;","o"}, {"&uacute;","u"}, {"&deg;","deg"}, {"&times;","x"}, {"&divide;","/"},
    {"&frac12;","1/2"}, {"&frac14;","1/4"}, {"&frac34;","3/4"}, {"&trade;","(TM)"},
    {"&lt;","<"}, {"&gt;",">"}, {"&nbsp;"," "}, {"&amp;","&"}, // &amp; PALING TERAKHIR
  };
  for (auto& e : ents) s.replace(e.pat, e.rep);
  return s;
}
static int triviaJsonGetInt(const String& s, const char* key) {
  String pat = String("\"") + key + "\":";
  int i = s.indexOf(pat);
  if (i < 0) return -1;
  i += pat.length();
  int j = i;
  while (j < (int)s.length() && (isDigit(s[j]) || s[j] == '-')) j++;
  if (j == i) return -1;
  return s.substring(i, j).toInt();
}
static String triviaJsonGetString(const String& obj, const char* key) {
  String pat = String("\"") + key + "\":\"";
  int i = obj.indexOf(pat);
  if (i < 0) return "";
  i += pat.length();
  int j = obj.indexOf('"', i);
  if (j < 0) return "";
  return obj.substring(i, j);
}
static int triviaJsonGetStringArray(const String& obj, const char* key, String out[], int maxN) {
  String pat = String("\"") + key + "\":[";
  int i = obj.indexOf(pat);
  if (i < 0) return 0;
  i += pat.length();
  int end = obj.indexOf(']', i);
  if (end < 0) return 0;
  String body = obj.substring(i, end);
  int n = 0, p = 0;
  while (n < maxN) {
    int q1 = body.indexOf('"', p); if (q1 < 0) break;
    int q2 = body.indexOf('"', q1 + 1); if (q2 < 0) break;
    out[n++] = body.substring(q1 + 1, q2);
    p = q2 + 1;
  }
  return n;
}
static String triviaExtractResultsArray(const String& full) {
  int i = full.indexOf("\"results\":[");
  if (i < 0) return "";
  i = full.indexOf('[', i);
  int depth = 0;
  for (int j = i; j < (int)full.length(); j++) {
    if (full[j] == '[') depth++;
    else if (full[j] == ']') { depth--; if (depth == 0) return full.substring(i + 1, j); }
  }
  return "";
}
static int triviaSplitObjects(const String& arrBody, String outObjs[], int maxN) {
  int n = 0, depth = 0, start = -1;
  for (int i = 0; i < (int)arrBody.length() && n < maxN; i++) {
    char c = arrBody[i];
    if (c == '{') { if (depth == 0) start = i; depth++; }
    else if (c == '}') { depth--; if (depth == 0 && start >= 0) { outObjs[n++] = arrBody.substring(start, i + 1); start = -1; } }
  }
  return n;
}

// =============================================
// Data tema/kategori -- id sesuai id kategori resmi OpenTDB, disalin persis.
// =============================================
struct TriviaCategory { int id; const char* name; uint16_t color; };
static TriviaCategory kCats[] = {
  {9,  "Umum",          0xFD40},
  {17, "Sains & Alam",  0x07E0},
  {18, "Komputer",      0x07FF},
  {21, "Olahraga",      0xF800},
  {22, "Geografi",      0x3ADF},
  {23, "Sejarah",       0xFC9F},
  {11, "Film",          0xFFE0},
  {12, "Musik",         0x861F},
  {15, "Video Game",    0x1FF9},
  {27, "Hewan",         0xFB40},
  {31, "Anime & Manga", 0xF81F},
  {20, "Mitologi",      0x04FF},
};
#define TRIVIA_CAT_COUNT 12
static const char* kDiffNames[4] = {"Mudah","Sedang","Sulit","Acak"};
static const int   kAmountOpts[3] = {5,10,15};

#define TRIVIA_MAX_Q 20
struct TriviaQuestion { String question; String answers[4]; int correctIdx; };
static TriviaQuestion s_qs[TRIVIA_MAX_Q];
static int s_count = 0, s_cur = 0, s_score = 0, s_correctCount = 0;
static int s_selected = -1;
static bool s_answered = false;
static unsigned long s_answerAtMs = 0;
static String s_errorMsg = "";

static int s_selCat = 0, s_selDiff = 0, s_selAmount = 1; // default: Umum, Mudah, 10 soal

enum TrvPage { TRV_SETUP, TRV_LOADING, TRV_QUESTION, TRV_RESULT, TRV_ERROR };
static TrvPage s_page = TRV_SETUP;

static void rebuildSelf() { triviaScreenShow(); }

// ---------- Fetch (async lewat http_client.h) ----------
static void startFetch() {
  if (!g_wifiConnected) {
    s_errorMsg = "WiFi belum terhubung. Sambungkan WiFi dulu lewat Setting.";
    s_page = TRV_ERROR; rebuildSelf(); return;
  }
  int catId = kCats[s_selCat].id;
  int amount = kAmountOpts[s_selAmount];
  String diffParam = "";
  if (s_selDiff == 0) diffParam = "&difficulty=easy";
  else if (s_selDiff == 1) diffParam = "&difficulty=medium";
  else if (s_selDiff == 2) diffParam = "&difficulty=hard";
  String url = "https://opentdb.com/api.php?amount=" + String(amount) + "&category=" + String(catId) +
               diffParam + "&type=multiple&encode=base64";

  if (!httpGetStart(url)) {
    s_errorMsg = "RAM internal HP lagi padat, atau ada proses lain msh jalan. Tutup app lain / coba lagi.";
    s_page = TRV_ERROR; rebuildSelf(); return;
  }
  s_page = TRV_LOADING; rebuildSelf();
}

static void processFetchResult() {
  if (httpGetState() == HTTP_DONE_ERR) {
    int code = httpGetStatusCode();
    s_errorMsg = "Gagal mengambil soal (HTTP " + String(code) + "). Cek koneksi internet.";
    httpGetReset();
    s_page = TRV_ERROR; rebuildSelf(); return;
  }
  String body = httpGetBody();
  httpGetReset();

  int rc = triviaJsonGetInt(body, "response_code");
  if (rc != 0) {
    switch (rc) {
      case 1: s_errorMsg = "Soal gak cukup utk tema/tingkat ini. Coba tema/jumlah lain."; break;
      case 2: s_errorMsg = "Parameter permintaan tidak valid."; break;
      default: s_errorMsg = "Server soal sedang bermasalah. Coba lagi nanti."; break;
    }
    s_page = TRV_ERROR; rebuildSelf(); return;
  }

  String resultsArr = triviaExtractResultsArray(body);
  String objs[TRIVIA_MAX_Q];
  int n = triviaSplitObjects(resultsArr, objs, TRIVIA_MAX_Q);

  int built = 0;
  for (int i = 0; i < n && built < TRIVIA_MAX_Q; i++) {
    String qB64 = triviaJsonGetString(objs[i], "question");
    String cB64 = triviaJsonGetString(objs[i], "correct_answer");
    String wrongB64[3];
    int wn = triviaJsonGetStringArray(objs[i], "incorrect_answers", wrongB64, 3);
    if (qB64.length() == 0 || cB64.length() == 0 || wn < 1) continue;

    String qText = triviaHtmlDecode(triviaB64Decode(qB64));
    String correctText = triviaHtmlDecode(triviaB64Decode(cB64));
    String opts[4]; int optN = 0;
    opts[optN++] = correctText;
    for (int k = 0; k < wn && optN < 4; k++) opts[optN++] = triviaHtmlDecode(triviaB64Decode(wrongB64[k]));
    while (optN < 4) opts[optN++] = "-";

    for (int k = optN - 1; k > 0; k--) { int r = random(0, k + 1); String tmp = opts[k]; opts[k] = opts[r]; opts[r] = tmp; }
    int correctIdx = 0;
    for (int k = 0; k < optN; k++) if (opts[k] == correctText) { correctIdx = k; break; }

    s_qs[built].question = qText;
    for (int k = 0; k < 4; k++) s_qs[built].answers[k] = opts[k];
    s_qs[built].correctIdx = correctIdx;
    built++;
  }

  if (built == 0) {
    s_errorMsg = "Gagal membaca data soal dari server.";
    s_page = TRV_ERROR; rebuildSelf(); return;
  }

  s_count = built; s_cur = 0; s_score = 0; s_correctCount = 0;
  s_selected = -1; s_answered = false;
  s_page = TRV_QUESTION; rebuildSelf();
}

static void advanceQuestion() {
  s_cur++;
  if (s_cur >= s_count) s_page = TRV_RESULT;
  else { s_selected = -1; s_answered = false; }
  rebuildSelf();
}

// Timer 1 dipakai gantian sesuai halaman aktif: poll hasil fetch (LOADING)
// atau auto-lanjut soal berikutnya stlh feedback benar/salah (QUESTION).
static void page_tick_cb(lv_timer_t* t) {
  if (s_page == TRV_LOADING) {
    HttpState st = httpGetState();
    if (st == HTTP_DONE_OK || st == HTTP_DONE_ERR) processFetchResult();
  } else if (s_page == TRV_QUESTION && s_answered && millis() - s_answerAtMs > 1400) {
    advanceQuestion();
  }
}

// ---------- Halaman SETUP ----------
static void cat_btn_cb(lv_event_t* e) { s_selCat = (int)(intptr_t)lv_event_get_user_data(e); rebuildSelf(); }
static void diff_btn_cb(lv_event_t* e) { s_selDiff = (int)(intptr_t)lv_event_get_user_data(e); rebuildSelf(); }
static void amt_btn_cb(lv_event_t* e) { s_selAmount = (int)(intptr_t)lv_event_get_user_data(e); rebuildSelf(); }
static void start_btn_cb(lv_event_t* e) { startFetch(); }

static void buildSetup(lv_obj_t* scr) {
  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 44);
  lv_obj_set_size(content, 320, 207 - 44);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 6, 0);
  lv_obj_set_style_pad_row(content, 10, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* catLbl = lv_label_create(content);
  lv_label_set_text(catLbl, "Tema");
  lv_obj_set_style_text_color(catLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(catLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* catGrid = lv_obj_create(content);
  lv_obj_remove_style_all(catGrid);
  lv_obj_set_size(catGrid, 300, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(catGrid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(catGrid, 6, 0);
  lv_obj_set_style_pad_column(catGrid, 6, 0);
  for (int i = 0; i < TRIVIA_CAT_COUNT; i++) {
    lv_obj_t* b = lv_obj_create(catGrid);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 94, 30);
    lv_obj_set_style_radius(b, 6, 0);
    bool sel = (i == s_selCat);
    lv_obj_set_style_bg_color(b, c565(sel ? kCats[i].color : T().surface), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    if (sel) { lv_obj_set_style_border_width(b, 1, 0); lv_obj_set_style_border_color(b, c565(T().text), 0); }
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, cat_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, kCats[i].name);
    lv_obj_set_style_text_color(l, c565(sel ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  lv_obj_t* diffLbl = lv_label_create(content);
  lv_label_set_text(diffLbl, "Tingkat");
  lv_obj_set_style_text_color(diffLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(diffLbl, &lv_font_montserrat_14, 0);
  lv_obj_t* diffRow = lv_obj_create(content);
  lv_obj_remove_style_all(diffRow);
  lv_obj_set_size(diffRow, 300, 28);
  lv_obj_set_flex_flow(diffRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(diffRow, 4, 0);
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_obj_create(diffRow);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 26);
    lv_obj_set_style_radius(b, 5, 0);
    bool sel = (i == s_selDiff);
    lv_obj_set_style_bg_color(b, c565(sel ? T().accent : T().surface2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, diff_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, kDiffNames[i]);
    lv_obj_set_style_text_color(l, c565(sel ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  lv_obj_t* amtLbl = lv_label_create(content);
  lv_label_set_text(amtLbl, "Jumlah Soal");
  lv_obj_set_style_text_color(amtLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(amtLbl, &lv_font_montserrat_14, 0);
  lv_obj_t* amtRow = lv_obj_create(content);
  lv_obj_remove_style_all(amtRow);
  lv_obj_set_size(amtRow, 300, 28);
  lv_obj_set_flex_flow(amtRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(amtRow, 4, 0);
  for (int i = 0; i < 3; i++) {
    lv_obj_t* b = lv_obj_create(amtRow);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 26);
    lv_obj_set_style_radius(b, 5, 0);
    bool sel = (i == s_selAmount);
    lv_obj_set_style_bg_color(b, c565(sel ? T().accent2 : T().surface2), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, amt_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* l = lv_label_create(b);
    char buf[10]; snprintf(buf, sizeof(buf), "%d Soal", kAmountOpts[i]);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_color(l, c565(sel ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
  }

  lv_obj_t* startBtn = lv_obj_create(content);
  lv_obj_remove_style_all(startBtn);
  lv_obj_set_size(startBtn, 300, 34);
  lv_obj_set_style_radius(startBtn, 8, 0);
  lv_obj_set_style_bg_color(startBtn, c565(T().good), 0);
  lv_obj_set_style_bg_opa(startBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(startBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(startBtn, start_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* startLbl = lv_label_create(startBtn);
  lv_label_set_text(startLbl, "Mulai Kuis");
  lv_obj_set_style_text_color(startLbl, c565(T().bg), 0);
  lv_obj_set_style_text_font(startLbl, &lv_font_montserrat_20, 0);
  lv_obj_center(startLbl);
}

// ---------- Halaman LOADING ----------
static void buildLoading(lv_obj_t* scr) {
  lv_obj_t* spinner = lv_spinner_create(scr, 1000, 70);
  lv_obj_set_size(spinner, 52, 52);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -10);
  lv_obj_set_style_arc_color(spinner, c565(T().accent2), LV_PART_INDICATOR);

  lv_obj_t* lbl = lv_label_create(scr);
  lv_label_set_text(lbl, "Mengambil soal dari server...");
  lv_obj_set_style_text_color(lbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 36);
}

// ---------- Halaman QUESTION ----------
static void answer_btn_cb(lv_event_t* e) {
  if (s_answered) return;
  s_selected = (int)(intptr_t)lv_event_get_user_data(e);
  s_answered = true;
  s_answerAtMs = millis();
  if (s_selected == s_qs[s_cur].correctIdx) { s_score += 10; s_correctCount++; }
  rebuildSelf();
}
static void buildQuestion(lv_obj_t* scr) {
  TriviaQuestion& q = s_qs[s_cur];

  lv_obj_t* catBadge = lv_obj_create(scr);
  lv_obj_remove_style_all(catBadge);
  lv_obj_set_size(catBadge, 100, 18);
  lv_obj_set_pos(catBadge, 6, 25);
  lv_obj_set_style_radius(catBadge, 4, 0);
  lv_obj_set_style_bg_color(catBadge, c565(kCats[s_selCat].color), 0);
  lv_obj_set_style_bg_opa(catBadge, LV_OPA_COVER, 0);
  lv_obj_t* catLbl = lv_label_create(catBadge);
  lv_label_set_text(catLbl, kCats[s_selCat].name);
  lv_obj_set_style_text_color(catLbl, c565(T().bg), 0);
  lv_obj_set_style_text_font(catLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(catLbl);

  lv_obj_t* infoLbl = lv_label_create(scr);
  char info[32]; snprintf(info, sizeof(info), "Soal %d/%d  Skor:%d", s_cur + 1, s_count, s_score);
  lv_label_set_text(infoLbl, info);
  lv_obj_set_style_text_color(infoLbl, c565(T().accent2), 0);
  lv_obj_set_style_text_font(infoLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(infoLbl, 320 - 150, 27);

  lv_obj_t* qLbl = lv_label_create(scr);
  lv_label_set_long_mode(qLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(qLbl, 300);
  lv_label_set_text(qLbl, q.question.c_str());
  lv_obj_set_style_text_color(qLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(qLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(qLbl, 8, 48);

  const char* letters[4] = {"A","B","C","D"};
  int top = 100, h = 24, gap = 5;
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_obj_create(scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 320 - 16, h);
    lv_obj_set_pos(b, 8, top + i * (h + gap));
    lv_obj_set_style_radius(b, 6, 0);
    uint16_t bg = T().surface;
    if (s_answered) {
      if (i == q.correctIdx) bg = T().good;
      else if (i == s_selected) bg = T().danger;
    }
    lv_obj_set_style_bg_color(b, c565(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, answer_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* l = lv_label_create(b);
    String line = String(letters[i]) + ". " + q.answers[i];
    lv_label_set_text(l, line.c_str());
    bool hi = s_answered && (i == q.correctIdx || i == s_selected);
    lv_obj_set_style_text_color(l, c565(hi ? T().bg : T().text), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l, 8, h / 2 - 8);
  }
}

// ---------- Halaman RESULT ----------
static void again_btn_cb(lv_event_t* e) { startFetch(); }
static void change_theme_btn_cb(lv_event_t* e) { s_page = TRV_SETUP; rebuildSelf(); }
static void buildResult(lv_obj_t* scr) {
  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 44);
  lv_obj_set_size(content, 320, 207 - 44);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content, 8, 0);

  lv_obj_t* t1 = lv_label_create(content);
  lv_label_set_text(t1, "Kuis Selesai!");
  lv_obj_set_style_text_color(t1, c565(T().accent), 0);
  lv_obj_set_style_text_font(t1, &lv_font_montserrat_20, 0);

  lv_obj_t* t2 = lv_label_create(content);
  char sb[32]; snprintf(sb, sizeof(sb), "%d/%d benar", s_correctCount, s_count);
  lv_label_set_text(t2, sb);
  lv_obj_set_style_text_color(t2, c565(T().text), 0);
  lv_obj_set_style_text_font(t2, &lv_font_montserrat_20, 0);

  int pct = s_count ? (s_correctCount * 100 / s_count) : 0;
  const char* msg = pct >= 80 ? "Hebat, jagoan trivia!" : pct >= 50 ? "Lumayan, terus asah!" : "Ayo coba lagi, pasti bisa!";
  lv_obj_t* t3 = lv_label_create(content);
  lv_label_set_text(t3, msg);
  lv_obj_set_style_text_color(t3, c565(T().subtext), 0);
  lv_obj_set_style_text_font(t3, &lv_font_montserrat_14, 0);

  lv_obj_t* againBtn = lv_obj_create(content);
  lv_obj_remove_style_all(againBtn);
  lv_obj_set_size(againBtn, 300, 30);
  lv_obj_set_style_radius(againBtn, 7, 0);
  lv_obj_set_style_bg_color(againBtn, c565(T().good), 0);
  lv_obj_set_style_bg_opa(againBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(againBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(againBtn, again_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* al = lv_label_create(againBtn);
  lv_label_set_text(al, "Main Lagi (tema sama)");
  lv_obj_set_style_text_color(al, c565(T().bg), 0);
  lv_obj_set_style_text_font(al, &lv_font_montserrat_14, 0);
  lv_obj_center(al);

  lv_obj_t* changeBtn = lv_obj_create(content);
  lv_obj_remove_style_all(changeBtn);
  lv_obj_set_size(changeBtn, 300, 30);
  lv_obj_set_style_radius(changeBtn, 7, 0);
  lv_obj_set_style_bg_color(changeBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(changeBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(changeBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(changeBtn, change_theme_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* cl = lv_label_create(changeBtn);
  lv_label_set_text(cl, "Ganti Tema");
  lv_obj_set_style_text_color(cl, c565(T().text), 0);
  lv_obj_set_style_text_font(cl, &lv_font_montserrat_14, 0);
  lv_obj_center(cl);
}

// ---------- Halaman ERROR ----------
static void retry_btn_cb(lv_event_t* e) { startFetch(); }
static void buildError(lv_obj_t* scr) {
  lv_obj_t* t1 = lv_label_create(scr);
  lv_label_set_text(t1, "Gagal memuat soal:");
  lv_obj_set_style_text_color(t1, c565(T().danger), 0);
  lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(t1, 8, 44);

  lv_obj_t* msgLbl = lv_label_create(scr);
  lv_label_set_long_mode(msgLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msgLbl, 300);
  lv_label_set_text(msgLbl, s_errorMsg.c_str());
  lv_obj_set_style_text_color(msgLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(msgLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(msgLbl, 8, 62);

  lv_obj_t* retryBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(retryBtn);
  lv_obj_set_size(retryBtn, 300, 30);
  lv_obj_align(retryBtn, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_style_radius(retryBtn, 7, 0);
  lv_obj_set_style_bg_color(retryBtn, c565(T().accent), 0);
  lv_obj_set_style_bg_opa(retryBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(retryBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(retryBtn, retry_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* rl = lv_label_create(retryBtn);
  lv_label_set_text(rl, "Coba Lagi");
  lv_obj_set_style_text_color(rl, c565(T().bg), 0);
  lv_obj_set_style_text_font(rl, &lv_font_montserrat_14, 0);
  lv_obj_center(rl);
}

void triviaScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen(s_page == TRV_QUESTION ? "" : "Trivia Quiz");

  switch (s_page) {
    case TRV_SETUP:    buildSetup(scr); break;
    case TRV_LOADING:  buildLoading(scr); break;
    case TRV_QUESTION: buildQuestion(scr); break;
    case TRV_RESULT:   buildResult(scr); break;
    case TRV_ERROR:    buildError(scr); break;
  }

  lv_timer_t* timer = lv_timer_create(page_tick_cb, 250, NULL);
  chromeBindTimerToScreen(scr, timer);
}
