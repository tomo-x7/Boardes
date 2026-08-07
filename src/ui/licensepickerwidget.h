#pragma once

#include <QVector>
#include <QWidget>

#include "../model/library.h"

class QComboBox;
class QLineEdit;
class QTextEdit;
class QLabel;

// ライセンス種別 (LicenseKind) の選択 + Custom 選択時の追加フィールド + そのライセンスから
// 自動導出される再配布ルールのプレビューをまとめたウィジェット。
// LibraryMetadataDialog と DuplicateLibraryDialog の両方から使う共通部品。
class LicensePickerWidget : public QWidget {
	Q_OBJECT

public:
	explicit LicensePickerWidget(QWidget *parent = nullptr);

	void setLicense(const LicenseInfo &info);
	LicenseInfo license() const;

	// 空なら全10種類を選択可能にする (既定)。非空なら列挙された種類のみに絞り込む。
	// 複製時、元ライセンスの派生ポリシー (コピーレフト/NC系固定) を強制するために使う。
	// 現在の選択が絞り込み後のリストに含まれなければ、リストの先頭が選ばれる。
	void setAllowedKinds(const QVector<LicenseKind> &kinds);

signals:
	void licenseChanged();

private slots:
	void onKindComboChanged(int index);

private:
	QComboBox *m_kindCombo;
	QWidget *m_customFieldsWidget;
	QLineEdit *m_customNameEdit;
	QLineEdit *m_customUrlEdit;
	QTextEdit *m_customTextEdit;
	QLabel *m_previewLabel;
	QVector<LicenseKind> m_allowedKinds;  // 空なら全種類

	void rebuildComboItems();
	void updateCustomFieldsVisibility();
	void updatePreview();
};
