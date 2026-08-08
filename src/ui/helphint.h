#pragma once

#include <QPoint>
#include <QString>
#include <QToolButton>
#include <QToolTip>
#include <QWidget>

// 注釈の表示方法を統一するためのヘルパ。
//
// 方針: 小さい字の説明ラベルを画面に常駐させない。説明は「？」ボタンのホバー／
// クリック、または入力欄・ボタン自身のツールチップで出す
// (例外: ステータスバー左のリアルタイム操作ヒントと、動的な計算結果を示すラベルは
// このヘルパの対象外 — それらは注釈ではなく状態表示なので残す)。
namespace helphint {

// ラベルの右に置く「？」ボタン。ホバーでもクリックでも同じ説明を出す。
inline QToolButton *button(const QString &text, QWidget *parent = nullptr) {
	auto *btn = new QToolButton(parent);
	btn->setText(QStringLiteral("?"));
	btn->setAutoRaise(true);
	btn->setToolTip(text);
	btn->setWhatsThis(text);
	btn->setFixedSize(18, 18);
	btn->setFocusPolicy(Qt::NoFocus);
	QObject::connect(btn, &QToolButton::clicked, btn, [btn, text] {
		QToolTip::showText(btn->mapToGlobal(btn->rect().bottomLeft()), text, btn);
	});
	return btn;
}

// 入力欄・ボタン自身に説明を付ける (ツールチップ + What's This)。
inline void attach(QWidget *w, const QString &text) {
	w->setToolTip(text);
	w->setWhatsThis(text);
}

}  // namespace helphint
