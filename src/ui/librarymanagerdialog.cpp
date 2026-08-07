#include "librarymanagerdialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "../model/librarymanager.h"
#include "duplicatelibrarydialog.h"
#include "librarymetadatadialog.h"

LibraryManagerDialog::LibraryManagerDialog(LibraryManager *libraryManager, QWidget *parent)
	: QDialog(parent), m_libraryManager(libraryManager) {
	setWindowTitle(tr("ライブラリ管理"));

	m_list = new QListWidget(this);
	m_detailsLabel = new QLabel(this);
	m_detailsLabel->setWordWrap(true);
	m_detailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	auto *installButton = new QPushButton(tr("インストール (.blib)..."), this);
	m_duplicateButton = new QPushButton(tr("複製..."), this);
	m_exportButton = new QPushButton(tr("エクスポート (.blib)..."), this);
	m_editMetadataButton = new QPushButton(tr("メタデータを編集..."), this);
	m_removeButton = new QPushButton(tr("削除"), this);

	connect(installButton, &QPushButton::clicked, this, &LibraryManagerDialog::onInstallBlib);
	connect(m_duplicateButton, &QPushButton::clicked, this, &LibraryManagerDialog::onDuplicate);
	connect(m_exportButton, &QPushButton::clicked, this, &LibraryManagerDialog::onExport);
	connect(m_editMetadataButton, &QPushButton::clicked, this, &LibraryManagerDialog::onEditMetadata);
	connect(m_removeButton, &QPushButton::clicked, this, &LibraryManagerDialog::onRemove);
	connect(m_list, &QListWidget::currentItemChanged, this, &LibraryManagerDialog::onSelectionChanged);

	auto *buttonColumn = new QVBoxLayout();
	buttonColumn->addWidget(installButton);
	buttonColumn->addSpacing(12);
	buttonColumn->addWidget(m_duplicateButton);
	buttonColumn->addWidget(m_exportButton);
	buttonColumn->addWidget(m_editMetadataButton);
	buttonColumn->addWidget(m_removeButton);
	buttonColumn->addStretch(1);

	auto *listColumn = new QVBoxLayout();
	listColumn->addWidget(m_list, /*stretch=*/2);
	listColumn->addWidget(m_detailsLabel, /*stretch=*/1);

	auto *columns = new QHBoxLayout();
	columns->addLayout(listColumn, 1);
	columns->addLayout(buttonColumn);

	auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(columns, /*stretch=*/1);
	mainLayout->addWidget(closeButtons);
	resize(640, 440);

	connect(m_libraryManager, &LibraryManager::librariesChanged, this, &LibraryManagerDialog::refreshList);
	refreshList();
}

QString LibraryManagerDialog::selectedLibraryId() const {
	auto *item = m_list->currentItem();
	return item ? item->data(Qt::UserRole).toString() : QString();
}

void LibraryManagerDialog::refreshList() {
	const QString previouslySelected = selectedLibraryId();
	m_list->clear();

	const auto &libs = m_libraryManager->libraries();
	for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
		const auto &lib = it.value();
		auto *item = new QListWidgetItem(tr("%1 (%2)").arg(lib->name, lib->id));
		item->setData(Qt::UserRole, lib->id);
		if (lib->readOnly) {
			item->setForeground(palette().color(QPalette::Disabled, QPalette::WindowText));
		}
		m_list->addItem(item);
	}

	for (int i = 0; i < m_list->count(); ++i) {
		if (m_list->item(i)->data(Qt::UserRole).toString() == previouslySelected) {
			m_list->setCurrentRow(i);
			return;
		}
	}
	if (m_list->count() > 0) {
		m_list->setCurrentRow(0);
	} else {
		onSelectionChanged();  // 空リストの状態を反映する
	}
}

void LibraryManagerDialog::onSelectionChanged() {
	const QString id = selectedLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		m_detailsLabel->setText(tr("(ライブラリが選択されていません)"));
		updateButtonStates();
		return;
	}

	QStringList lines;
	lines << tr("名前: %1").arg(lib->name);
	lines << tr("ID: %1").arg(lib->id);
	lines << tr("バージョン: %1").arg(lib->version.isEmpty() ? tr("(未設定)") : lib->version);
	lines << tr("作者: %1").arg(lib->author.isEmpty() ? tr("(未設定)") : lib->author);
	lines << tr("ライセンス: %1").arg(lib->license.displayName());
	lines << tr("編集: %1 / 再配布: %2")
				 .arg(lib->readOnly ? tr("不可") : tr("可"), lib->redistribution.allowed ? tr("可") : tr("不可"));
	lines << tr("部品: %1件 / 基板: %2件 / カテゴリ: %3件")
				 .arg(lib->parts.size())
				 .arg(lib->boards.size())
				 .arg(lib->categories.size());
	if (lib->basedOn.has_value()) {
		lines << tr("複製元: %1 (v%2, %3)")
					 .arg(lib->basedOn->name, lib->basedOn->version, lib->basedOn->licenseLabel);
	}
	if (!lib->description.isEmpty()) {
		lines << QString();
		lines << lib->description;
	}
	m_detailsLabel->setText(lines.join(QStringLiteral("\n")));
	updateButtonStates();
}

void LibraryManagerDialog::updateButtonStates() {
	const QString id = selectedLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	m_duplicateButton->setEnabled(lib != nullptr);
	m_exportButton->setEnabled(lib && lib->redistribution.allowed && lib->id != LibraryManager::myLibraryId());
	m_editMetadataButton->setEnabled(lib && !lib->readOnly);
	m_removeButton->setEnabled(lib && lib->id != LibraryManager::myLibraryId() &&
							   lib->id != LibraryManager::passCompatId());
}

void LibraryManagerDialog::onInstallBlib() {
	const QString path =
		QFileDialog::getOpenFileName(this, tr("ライブラリをインストール"), QString(), tr("ライブラリパッケージ (*.blib)"));
	if (path.isEmpty()) {
		return;
	}
	const auto result = m_libraryManager->installBlib(path);
	if (!result.ok) {
		QMessageBox::warning(this, tr("インストール失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("インストール完了"),
							  tr("部品: %1件 / 基板: %2件 / カテゴリ: %3件をインストールしました。")
								  .arg(result.partCount)
								  .arg(result.boardCount)
								  .arg(result.categoryCount));
}

void LibraryManagerDialog::onDuplicate() {
	const QString id = selectedLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		return;
	}
	DuplicateLibraryDialog dialog(*lib, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const auto spec = dialog.spec();
	const auto result = m_libraryManager->duplicateLibrary(id, spec);
	if (!result.ok) {
		QMessageBox::warning(this, tr("複製失敗"), result.error);
		return;
	}
	QMessageBox::information(this, tr("複製完了"), tr("「%1」として複製しました。").arg(spec.newName));
}

void LibraryManagerDialog::onExport() {
	const QString id = selectedLibraryId();
	if (id.isEmpty()) {
		return;
	}
	QString path =
		QFileDialog::getSaveFileName(this, tr("ライブラリをエクスポート"), QString(), tr("ライブラリパッケージ (*.blib)"));
	if (path.isEmpty()) {
		return;
	}
	if (!path.endsWith(QStringLiteral(".blib"), Qt::CaseInsensitive)) {
		path += QStringLiteral(".blib");
	}
	if (!m_libraryManager->exportLibrary(id, path)) {
		QMessageBox::warning(this, tr("エクスポート失敗"), tr("エクスポートできませんでした (再配布不可の可能性があります)。"));
		return;
	}
	QMessageBox::information(this, tr("エクスポート完了"), tr("エクスポートしました: %1").arg(path));
}

void LibraryManagerDialog::onRemove() {
	const QString id = selectedLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib) {
		return;
	}
	const auto btn = QMessageBox::question(
		this, tr("確認"),
		tr("ライブラリ「%1」を削除します。これを参照している設計データは、開いたときに部品が"
		   "解決できなくなります。よろしいですか？")
			.arg(lib->name));
	if (btn != QMessageBox::Yes) {
		return;
	}
	if (!m_libraryManager->removeLibrary(id)) {
		QMessageBox::warning(this, tr("削除失敗"), tr("削除できませんでした。"));
	}
}

void LibraryManagerDialog::onEditMetadata() {
	const QString id = selectedLibraryId();
	const auto lib = id.isEmpty() ? nullptr : m_libraryManager->library(id);
	if (!lib || lib->readOnly) {
		return;
	}
	LibraryMetadataDialog dialog(this);
	dialog.setLibrary(*lib);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	Library updated = *lib;
	dialog.applyTo(updated);
	if (!m_libraryManager->updateLibraryMetadata(id, updated)) {
		QMessageBox::warning(this, tr("更新失敗"), tr("メタデータを更新できませんでした。"));
	}
}
