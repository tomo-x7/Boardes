#include "about.h"

#include "theme.h"
#include "ui_about.h"

About::About(QWidget *parent) : QDialog(parent), ui(new Ui::About) {
	ui->setupUi(this);
	// QTextBrowser のリッチテキストは QSS の [isLink="true"] セレクタが効かないため、
	// ドキュメント側のスタイルシートでリンク色を当てる (仕様書§16)。.ui 側の元の HTML を
	// 保持しておき、テーマ変更のたびにスタイルシートを差し替えて再適用する。
	const QString originalHtml = ui->licenseBrowser->toHtml();
	const auto applyLinkColor = [this, originalHtml] {
		ui->licenseBrowser->document()->setDefaultStyleSheet(
			QStringLiteral("a { color: %1; } a:hover { color: %2; }")
				.arg(Theme::instance().linkColor().name(), Theme::instance().linkHoverColor().name()));
		ui->licenseBrowser->setHtml(originalHtml);
	};
	applyLinkColor();
	connect(&Theme::instance(), &Theme::changed, this, applyLinkColor);
}

About::~About() {
	delete ui;
}
