#pragma once

#include <QDialog>
#include <QStringList>

class LibraryManager;
class PartListModel;
class QComboBox;
class QLineEdit;
class QListView;
class QLabel;

// 「他ライブラリから複製」用の部品ピッカー。まず複製元ライブラリを1つ選び、
// そのライブラリの部品を検索・複数選択する
// (LibraryManager::copyPartsBetween が単一の複製元しか取らないため、コピー元は
// 常に1ライブラリに定まる設計にしてある)。
class PartPickerDialog : public QDialog {
	Q_OBJECT

public:
	// excludeLibraryId: 複製元候補から除外するライブラリ id (コピー先自身)。
	explicit PartPickerDialog(LibraryManager *libraryManager, const QString &excludeLibraryId,
							  QWidget *parent = nullptr);

	QString sourceLibraryId() const;
	QStringList selectedPartIds() const;

private slots:
	void onLibraryComboChanged(int index);
	void onSearchTextChanged(const QString &text);
	void onSelectionChanged();

private:
	LibraryManager *m_libraryManager;
	QString m_excludeLibraryId;
	QComboBox *m_libraryCombo;
	QLineEdit *m_searchEdit;
	QListView *m_listView;
	PartListModel *m_model;
	QLabel *m_countLabel;

	void rebuildLibraryCombo();
	void applyFilter();
};
