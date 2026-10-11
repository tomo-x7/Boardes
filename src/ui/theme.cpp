#include "theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleHints>
#include <QTextStream>

namespace {

// QSS (light.qss / dark.qss) と同じカラートークン。
struct Tokens {
	QColor surface0, surface1, surface2, surface3;
	QColor textPrimary, textDisabled;
	QColor link;
};

// QSS で文字色を指定していないところ (QLabel、一覧・表の項目、ツールチップ、palette(mid) を使う補足の
// 文字など) はアプリのパレットの色で描かれる。パレットが OS のまま (ライト) だと、ダークテーマでは
// 暗い文字が暗い背景に重なって読めない (OS がライトのとき、実行中に切り替えたとき、OS のテーマ判定と
// パレットが食い違う環境など)。QSS と同じトークンでパレットも揃える。
QPalette paletteFor(bool dark) {
	const Tokens t = dark ? Tokens{QColor(0x1C, 0x1C, 0x1C), QColor(0x23, 0x23, 0x23), QColor(0x2C, 0x2C, 0x2C),
								   QColor(0x38, 0x38, 0x38), QColor(0xF2, 0xF2, 0xF2), QColor(0x8C, 0x8C, 0x8C),
								   QColor(0x6C, 0x9B, 0xD1)}
						  : Tokens{QColor(0xFF, 0xFF, 0xFF), QColor(0xFA, 0xFA, 0xFA), QColor(0xF2, 0xF2, 0xF2),
								   QColor(0xE4, 0xE4, 0xE4), QColor(0x1F, 0x1F, 0x1F), QColor(0x9A, 0x9A, 0x9A),
								   QColor(0x34, 0x65, 0xA4)};
	// 立体表現用の役割 (Light/Dark/Shadow 等) はスタイル既定のまま。部品エディタのキャンバスは Dark を
	// 自前で上書きしている (parteditordialog.cpp)。
	QPalette p = QApplication::style()->standardPalette();
	const auto set = [&p](QPalette::ColorRole role, const QColor &color) { p.setColor(QPalette::All, role, color); };
	set(QPalette::Window, t.surface1);
	set(QPalette::WindowText, t.textPrimary);
	set(QPalette::Base, t.surface0);
	set(QPalette::AlternateBase, t.surface1);
	set(QPalette::Text, t.textPrimary);
	set(QPalette::Button, t.surface2);
	set(QPalette::ButtonText, t.textPrimary);
	set(QPalette::BrightText, t.textPrimary);
	// 選択はグレーの濃淡だけで表す (QSS と同じ。ブランドカラーは無い)。部品セレクタの「配置中」バーもこれ。
	set(QPalette::Highlight, t.surface3);
	set(QPalette::HighlightedText, t.textPrimary);
	set(QPalette::ToolTipBase, t.surface0);
	set(QPalette::ToolTipText, t.textPrimary);
	set(QPalette::PlaceholderText, t.textDisabled);
	// palette(mid) は補足の文字 (基板エディタの導出サイズ、ライセンスの説明など) と枠線に使っている。
	set(QPalette::Mid, t.textDisabled);
	set(QPalette::Link, t.link);
	set(QPalette::LinkVisited, t.link);
	for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
		p.setColor(QPalette::Disabled, role, t.textDisabled);
	}
	return p;
}

}  // namespace

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
	// 9pt: 使い込む前提の道具なので、GIMP や CAD と同程度の情報密度に合わせる (以前は 10pt)。
	QFont font(QStringLiteral("Noto Sans JP"), 9);
	qApp->setFont(font);

	QStyleHints *hints = QGuiApplication::styleHints();
	apply(hints->colorScheme() == Qt::ColorScheme::Dark);
	connect(hints, &QStyleHints::colorSchemeChanged, this,
			[this](Qt::ColorScheme scheme) { apply(scheme == Qt::ColorScheme::Dark); });
}

void Theme::apply(bool dark) {
	m_dark = dark;
	// パレットを先に替える (スタイルシートの適用で各ウィジェットが再ポリッシュされるときに新しい色を使うよう)。
	qApp->setPalette(paletteFor(dark));
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
