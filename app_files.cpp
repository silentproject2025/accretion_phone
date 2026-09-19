#include "app_files.h"
#include "screen_chrome.h"
#include "theme.h"
#include "storage_manager.h"
#include "sys_state.h"
#include "wifi_manager.h"
#include <Arduino.h>

#define MAX_LIST 40
static StorageFileEntry s_files[MAX_LIST];
static int s_fileCount = 0;
static String s_viewName = "";
static String s_viewContent = "";

static void rebuildSelf() { filesScreenShow(); }

static void rescan() {
  s_fileCount = storageListFiles(s_files, MAX_LIST);
}

static void open_btn_cb(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  s_viewName = s_files[idx].name;
  s_viewContent = storageReadText(s_viewName.c_str());
  if (s_viewContent.length() == 0) s_viewContent = "(kosong / gagal dibaca)";
  rebuildSelf();
}
static void delete_btn_cb(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  storageDeleteFile(s_files[idx].name.c_str());
  chromeShowToast("File dihapus");
  rescan();
  rebuildSelf();
}
static void close_view_btn_cb(lv_event_t* e) {
  s_viewName = ""; s_viewContent = "";
  rebuildSelf();
}

static String humanSize(uint32_t b) {
  char buf[24];
  if (b < 1024) snprintf(buf, sizeof(buf), "%u B", (unsigned)b);
  else snprintf(buf, sizeof(buf), "%.1f KB", b / 1024.0f);
  return String(buf);
}

static void buildList(lv_obj_t* scr) {
  // Status web uploader -- sengaja ditampilkan sbg info APA ADANYA (belum
  // di-port), bukan status hidup/mati beneran spt file lama.
  lv_obj_t* statusLbl = lv_label_create(scr);
  lv_label_set_text(statusLbl, g_wifiConnected ? "Web uploader: belum di-port" : "WiFi belum terhubung");
  lv_obj_set_style_text_color(statusLbl, c565(T().subtext), 0);
  lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(statusLbl, 8, 40); // baris sendiri di bawah judul, biar teksnya gak kepotong tepi layar

  if (!g_sdReady && !g_ffatReady) {
    lv_obj_t* errLbl = lv_label_create(scr);
    lv_label_set_text(errLbl, "Storage tidak terdeteksi");
    lv_obj_set_style_text_color(errLbl, c565(T().danger), 0);
    lv_obj_set_style_text_font(errLbl, &lv_font_montserrat_14, 0);
    lv_obj_align(errLbl, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_t* content = lv_obj_create(scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 58);
  lv_obj_set_size(content, 320, 207 - 58);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(content, 6, 0);
  lv_obj_set_style_pad_row(content, 5, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  if (s_fileCount == 0) {
    lv_obj_t* emptyLbl = lv_label_create(content);
    lv_label_set_text(emptyLbl, "Kosong / belum ada file.");
    lv_obj_set_style_text_color(emptyLbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_14, 0);
    return;
  }

  for (int i = 0; i < s_fileCount; i++) {
    lv_obj_t* row = lv_obj_create(content);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 300, 30);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_bg_color(row, c565(T().surface), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(row, 8, 0);

    lv_obj_t* nameCol = lv_obj_create(row);
    lv_obj_remove_style_all(nameCol);
    lv_obj_set_flex_flow(nameCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(nameCol, 140, LV_SIZE_CONTENT);
    lv_obj_t* nameLbl = lv_label_create(nameCol);
    String shortName = s_files[i].name;
    if (shortName.length() > 18) shortName = shortName.substring(0, 16) + "..";
    lv_label_set_text(nameLbl, shortName.c_str());
    lv_obj_set_style_text_color(nameLbl, c565(T().text), 0);
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
    lv_obj_t* sizeLbl = lv_label_create(nameCol);
    lv_label_set_text(sizeLbl, humanSize(s_files[i].size).c_str());
    lv_obj_set_style_text_color(sizeLbl, c565(T().subtext), 0);
    lv_obj_set_style_text_font(sizeLbl, &lv_font_montserrat_14, 0);

    lv_obj_t* btnRow = lv_obj_create(row);
    lv_obj_remove_style_all(btnRow);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btnRow, 6, 0);
    lv_obj_set_size(btnRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    lv_obj_t* openBtn = lv_obj_create(btnRow);
    lv_obj_remove_style_all(openBtn);
    lv_obj_set_size(openBtn, 48, 22);
    lv_obj_set_style_radius(openBtn, 4, 0);
    lv_obj_set_style_bg_color(openBtn, c565(T().good), 0);
    lv_obj_set_style_bg_opa(openBtn, LV_OPA_COVER, 0);
    lv_obj_add_flag(openBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(openBtn, open_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* openLbl = lv_label_create(openBtn);
    lv_label_set_text(openLbl, "Buka");
    lv_obj_set_style_text_color(openLbl, c565(T().bg), 0);
    lv_obj_set_style_text_font(openLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(openLbl);

    lv_obj_t* delBtn = lv_obj_create(btnRow);
    lv_obj_remove_style_all(delBtn);
    lv_obj_set_size(delBtn, 48, 22);
    lv_obj_set_style_radius(delBtn, 4, 0);
    lv_obj_set_style_bg_color(delBtn, c565(T().danger), 0);
    lv_obj_set_style_bg_opa(delBtn, LV_OPA_COVER, 0);
    lv_obj_add_flag(delBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(delBtn, delete_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* delLbl = lv_label_create(delBtn);
    lv_label_set_text(delLbl, "Hapus");
    lv_obj_set_style_text_color(delLbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(delLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(delLbl);
  }
}

static void buildViewer(lv_obj_t* scr) {
  lv_obj_t* closeBtn = lv_obj_create(scr);
  lv_obj_remove_style_all(closeBtn);
  lv_obj_set_size(closeBtn, 60, 20);
  lv_obj_set_pos(closeBtn, 320 - 68, 24);
  lv_obj_set_style_radius(closeBtn, 4, 0);
  lv_obj_set_style_bg_color(closeBtn, c565(T().surface2), 0);
  lv_obj_set_style_bg_opa(closeBtn, LV_OPA_COVER, 0);
  lv_obj_add_flag(closeBtn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(closeBtn, close_view_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* closeLbl = lv_label_create(closeBtn);
  lv_label_set_text(closeLbl, "Tutup");
  lv_obj_set_style_text_color(closeLbl, c565(T().accent), 0);
  lv_obj_set_style_text_font(closeLbl, &lv_font_montserrat_14, 0);
  lv_obj_center(closeLbl);

  lv_obj_t* box = lv_obj_create(scr);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, 4, 46);
  lv_obj_set_size(box, 320 - 8, 207 - 46);
  lv_obj_set_style_radius(box, 6, 0);
  lv_obj_set_style_bg_color(box, c565(T().surface), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(box, 8, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(box, 4, 0);
  lv_obj_set_scroll_dir(box, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* nameLbl = lv_label_create(box);
  String nb = "File: " + s_viewName;
  lv_label_set_text(nameLbl, nb.c_str());
  lv_obj_set_style_text_color(nameLbl, c565(T().accent2), 0);
  lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);

  lv_obj_t* divider = lv_obj_create(box);
  lv_obj_remove_style_all(divider);
  lv_obj_set_size(divider, 280, 1);
  lv_obj_set_style_bg_color(divider, c565(T().divider), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

  String shown = s_viewContent;
  bool truncated = shown.length() > 3000;
  if (truncated) shown = shown.substring(0, 3000) + "\n\n[...dipotong, file lebih panjang dari ini...]";

  lv_obj_t* contentLbl = lv_label_create(box);
  lv_label_set_long_mode(contentLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(contentLbl, 280);
  lv_label_set_text(contentLbl, shown.c_str());
  lv_obj_set_style_text_color(contentLbl, c565(T().text), 0);
  lv_obj_set_style_text_font(contentLbl, &lv_font_montserrat_14, 0);
}

void filesScreenShow() {
  lv_obj_t* scr = chromeCreateAppScreen(s_viewName.length() > 0 ? "" : "File Explorer");
  if (s_viewName.length() > 0) {
    buildViewer(scr);
  } else {
    rescan();
    buildList(scr);
  }
}
