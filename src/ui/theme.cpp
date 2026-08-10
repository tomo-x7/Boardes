#include "theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPushButton>
#include <QStyleHints>
#include <QTextStream>

Theme &Theme::instance() {
	static Theme theme;
	return theme;
}

void Theme::init() {
	// resources/boardes.qrc は boardes_core (静的ライブラリ) 側にリンクされている。
	// 静的ライブラリ内の .qrc はリンカに「参照されていない」と判断されて丸ごと
	// 捨てられることがあるため (Qt の既知の注意点)、Q_INIT_RESOURCE で明示的に
	// 初期化を強制する。呼び出しは名前空間の外側であること (この関数は Theme クラスの
	// メンバなので条件を満たす)。
	Q_INIT_RESOURCE(boardes);

	// Noto Sans JP (可変フォント、Regular〜Bold を1ファイルでカバー) を埋め込みリソースから
	// 登録する。失敗しても致命的ではない (Qt の既定フォールバックフォントで表示されるだけ)。
	QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/NotoSansJP.ttf"));
	QFont font(QStringLiteral("Noto Sans JP"), 10);
	qApp->setFont(font);

	QStyleHints *hints = QGuiApplication::styleHints();
	apply(hints->colorScheme() == Qt::ColorScheme::Dark);
	connect(hints, &QStyleHints::colorSchemeChanged, this,
			[this](Qt::ColorScheme scheme) { apply(scheme == Qt::ColorScheme::Dark); });
}

void Theme::apply(bool dark) {
	m_dark = dark;
	QFile file(dark ? QStringLiteral(":/qss/dark.qss") : QStringLiteral(":/qss/light.qss"));
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qApp->setStyleSheet(QTextStream(&file).readAll());
	}
	emit changed();
}

QColor Theme::errorColor() const {
	return m_dark ? QColor(0xE5, 0x67, 0x5A) : QColor(0xC0, 0x39, 0x2B);
}

QColor Theme::warningColor() const {
	return m_dark ? QColor(0xE8, 0xB2, 0x3D) : QColor(0x9A, 0x6B, 0x00);
}

QColor Theme::linkColor() const {
	return m_dark ? QColor(0x6C, 0x9B, 0xD1) : QColor(0x34, 0x65, 0xA4);
}

QColor Theme::linkHoverColor() const {
	return m_dark ? QColor(0x8F, 0xB4, 0xE0) : QColor(0x1F, 0x45, 0x71);
}

QColor Theme::iconNormalColor() const {
	return m_dark ? QColor(0xBE, 0xBE, 0xBE) : QColor(0x54, 0x54, 0x54);
}

QColor Theme::iconActiveColor() const {
	return m_dark ? QColor(0xF2, 0xF2, 0xF2) : QColor(0x1F, 0x1F, 0x1F);
}

QColor Theme::wireFrontColor() {
	return QColor(0x20, 0x60, 0xD0);
}

QColor Theme::wireBackColor() {
	return QColor(0xD0, 0x50, 0x20);
}

QColor Theme::wireFrontInsulatedColor() {
	return QColor(0x60, 0x90, 0xE0);
}

QColor Theme::wireBackInsulatedColor() {
	return QColor(0xE0, 0x80, 0x50);
}

QColor Theme::anchorMarkerColor() {
	return QColor(0x3D, 0xBE, 0x5B);
}

void Theme::suppressAutoDefault(QWidget *root) {
	if (!root) {
		return;
	}
	const auto buttons = root->findChildren<QPushButton *>();
	for (QPushButton *b : buttons) {
		// QDialogButtonBox::addButton() はボタンをボックス自身の子にする。ボックス配下の
		// ボタン (OK/作成/保存/キャンセル等) はそのまま Qt の既定挙動に任せる。
		if (qobject_cast<QDialogButtonBox *>(b->parentWidget())) {
			continue;
		}
		b->setAutoDefault(false);
		b->setDefault(false);
	}
}

QFont Theme::monoFont() {
	QFont f;
	f.setFamilies({QStringLiteral("Consolas"), QStringLiteral("SF Mono"), QStringLiteral("monospace")});
	f.setStyleHint(QFont::Monospace);
	return f;
}
