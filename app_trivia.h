// app_trivia.h
// Port dari APP: TRIVIA QUIZ file lama -- ambil soal dari OpenTDB
// (opentdb.com) lewat subsistem http_client.h yg baru. Parser base64/HTML
// entity/JSON manual (bukan ArduinoJson) disalin PERSIS dari file lama,
// termasuk daftar 12 kategori & warnanya.
#pragma once

void triviaScreenShow();
