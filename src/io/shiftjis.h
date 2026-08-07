#pragma once

#include <QByteArray>
#include <QString>

// Shift-JIS (CP932) デコーダ。
//
// Qt6 の軽量な QStringDecoder は UTF-8/16/32・Latin1・System しか名前解決できず、
// Shift-JIS は含まれない (Qt5 の QTextCodec 相当は Core5Compat という別モジュールが
// 必要で、CI 環境のインストールに追加のセットアップを要求してしまう)。
// PasS のカテゴリ名 (<Cat>.txt) が Shift-JIS 固定のため、追加モジュール無しで
// 動く自前デコーダを用意する。変換テーブルは vendor/shiftjis_table.h (Python の
// cp932 コーデックから機械生成) を使う。
namespace shiftjis {

// 変換できないバイト列があっても U+FFFD (置換文字) を挟んで処理を続ける。
QString decode(const QByteArray &bytes);

}  // namespace shiftjis
