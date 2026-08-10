#include "exportpackagedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "../model/librarymanager.h"
#include "theme.h"

ExportPackageDialog::ExportPackageDialog(LibraryManager *libraryManager, const QStringList &dependencyLibraryIds,
										 QWidget *parent)
	: QDialog(parent), m_libraryManager(libraryManager) {
	setWindowTitle(tr("設計パッケージとしてエクスポート"));

	auto *layout = new QVBoxLayout(this);
	auto *hint = new QLabel(
		tr("同梱する依存ライブラリを選んでください。再配布不可のライブラリを含めると、\n"
		   "書き出したファイルは第三者への配布や公開に使えません (自分用のバックアップのみ)。"),
		this);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	for (const QString &libId : dependencyLibraryIds) {
		const auto lib = m_libraryManager ? m_libraryManager->library(libId) : nullptr;
		const QString name = lib ? lib->name : libId;
		const bool allowed = lib && lib->redistribution.allowed;

		auto *check = new QCheckBox(this);
		check->setObjectName(libId);
		check->setChecked(allowed);
		if (allowed) {
			check->setText(tr("%1 (再配布可)").arg(name));
		} else {
			check->setText(tr("%1 (再配布不可)").arg(name));
			// 仕様書§3.2のerrorトークン (ライト/ダークで値が異なる)。
			check->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::instance().errorColor().name()));
		}
		layout->addWidget(check);
		m_checks.append(check);
	}

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &ExportPackageDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addStretch(1);
	layout->addWidget(buttons);
	resize(420, 320);
}

QSet<QString> ExportPackageDialog::includeLibraryIds() const {
	QSet<QString> ids;
	for (QCheckBox *c : m_checks) {
		if (c->isChecked()) {
			ids.insert(c->objectName());
		}
	}
	return ids;
}

void ExportPackageDialog::onAccept() {
	QStringList nonRedistributableChecked;
	for (QCheckBox *c : m_checks) {
		if (!c->isChecked()) {
			continue;
		}
		const auto lib = m_libraryManager ? m_libraryManager->library(c->objectName()) : nullptr;
		if (!lib || !lib->redistribution.allowed) {
			nonRedistributableChecked << (lib ? lib->name : c->objectName());
		}
	}
	if (!nonRedistributableChecked.isEmpty()) {
		const auto btn = QMessageBox::question(
			this, tr("確認"),
			tr("次のライブラリは再配布が許可されていません:\n・%1\n\n"
			   "書き出したファイルはバックアップや自分用の持ち出しのみに使い、"
			   "第三者への配布や公開はしないでください。続けますか？")
				.arg(nonRedistributableChecked.join(QStringLiteral("\n・"))));
		if (btn != QMessageBox::Yes) {
			return;
		}
	}
	accept();
}
