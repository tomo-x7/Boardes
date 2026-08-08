#pragma once

#include <QAbstractListModel>
#include <QIcon>
#include <QWidget>
#include <memory>

#include "../model/part.h"

class LibraryManager;
class QComboBox;
class QLineEdit;
class QListView;
class QLabel;
class QPushButton;
class QWidget;

enum class PartSelectorMode {
	ByLibrary,
	ByCategory,
	All,
};

// 部品一覧のモデル。現在のフィルタ (モード/ライブラリ/カテゴリ/検索語) に一致する
// 部品だけを保持する。フィルタが変わるたびに rebuild() で作り直す
// (部品数は多くても数百件程度なので毎回全走査で十分)。
class PartListModel : public QAbstractListModel {
	Q_OBJECT

public:
	enum Roles {
		LibraryIdRole = Qt::UserRole + 1,
		PartIdRole,
		CategoryIdRole,
	};

	explicit PartListModel(QObject *parent = nullptr);

	void setLibraryManager(LibraryManager *manager);
	void setFilter(PartSelectorMode mode, const QString &libraryId, const QString &categoryId, const QString &query);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;

private:
	struct Entry {
		QString libraryId;
		QString partId;
		QString categoryId;
		std::shared_ptr<Part> part;
	};

	LibraryManager *m_libraryManager = nullptr;
	QVector<Entry> m_entries;
	PartSelectorMode m_mode = PartSelectorMode::All;
	QString m_libraryFilter;
	QString m_categoryFilter;
	QString m_query;
	mutable QHash<QString, QIcon> m_iconCache;

	void rebuild();
	QIcon iconFor(const Entry &entry) const;
};

// 左側の部品パレット。ライブラリ別/カテゴリ別/全部品の切替、検索、
// アイコングリッド表示 (QListView::IconMode) をまとめたウィジェット。
class PartSelector : public QWidget {
	Q_OBJECT

public:
	explicit PartSelector(QWidget *parent = nullptr);

	void setLibraryManager(LibraryManager *manager);

	// 配置ツールが (Esc・右クリック・他ツールへの切替で) 解除されたときに呼ぶ。
	// 「配置中」バーを消し、一覧の選択も外す。
	void clearPendingPart();

signals:
	// 部品がクリック (またはキーボード操作) で選択された。配置ツールに部品をセットする
	// 用途を想定 (シングルクリックで配置対象になる。ダブルクリックは要求しない)。
	void partSelected(const QString &libraryId, const QString &partId);

private slots:
	void onModeChanged(int index);
	void onLibraryComboChanged(int index);
	void onCategoryComboChanged(int index);
	void onSearchTextChanged(const QString &text);
	void onCurrentChanged(const QModelIndex &current);
	void onLibrariesChanged();

private:
	LibraryManager *m_libraryManager = nullptr;
	QComboBox *m_modeCombo;
	QComboBox *m_libraryCombo;
	QComboBox *m_categoryCombo;
	QLineEdit *m_searchEdit;
	QListView *m_listView;
	PartListModel *m_model;

	// 「配置中: <アイコン> <名前> [解除]」バー。未選択のあいだは隠す。
	QWidget *m_pendingBar;
	QLabel *m_pendingIcon;
	QLabel *m_pendingLabel;
	QPushButton *m_pendingClearButton;

	void rebuildLibraryCombo();
	void rebuildCategoryCombo();
	void updateFilterVisibility();
	void applyFilter();
	PartSelectorMode currentMode() const;
};
