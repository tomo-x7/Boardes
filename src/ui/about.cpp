#include "about.h"

#include <QRegularExpression>

#include "theme.h"
#include "ui_about.h"

About::About(QWidget *parent) : QDialog(parent), ui(new Ui::About) {
	ui->setupUi(this);
	// QTextBrowser のリッチテキストは QSS の [isLink="true"] セレクタが効かないため、
	// ドキュメント側のスタイルシートでリンク色を当てる (仕様書§16)。.ui 側の元の HTML を
	// 保持しておき、テーマ変更のたびにスタイルシートを差し替えて再適用する。
	// 既知の差分 (改善提案2 #7): QTextBrowser::toHtml() は「現在のパレットで解決済みの色」を
	// インラインの color: として焼き込んで返す。setupUi() の初回 setHtml() 時点でリンクは
	// (当時の既定パレットの) 純青 #0000ff で焼き込まれ、以後は document()->
	// setDefaultStyleSheet() で a{color:...} を足しても、詳細度で勝るインライン color: に
	// 負けて反映されなかった (ダークでのコントラスト比が WCAG AA 未達)。焼き込まれた
	// color: 宣言を全部剥がした「素の」HTML を保持しておき、テーマ変更のたびにそこへ
	// 改めて色を注入し直す。
	QString rawHtml = ui->licenseBrowser->toHtml();
	rawHtml.remove(QRegularExpression(QStringLiteral("\\s*color:[^;\"]*;?")));
	const auto applyLinkColor = [this, rawHtml] {
		ui->licenseBrowser->document()->setDefaultStyleSheet(
			QStringLiteral("a { color: %1; } a:hover { color: %2; }")
				.arg(Theme::instance().linkColor().name(), Theme::instance().linkHoverColor().name()));
		ui->licenseBrowser->setHtml(rawHtml);
	};
	applyLinkColor();
	connect(&Theme::instance(), &Theme::changed, this, applyLinkColor);
}

About::~About() {
	delete ui;
}
