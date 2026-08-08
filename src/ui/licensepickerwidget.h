#pragma once

#include <QVector>
#include <QWidget>

#include "../model/library.h"

class QComboBox;
class QLineEdit;
class QTextEdit;
class QLabel;
class QCheckBox;

// ライセンス種別 (LicenseKind) の選択 + Custom 選択時の追加フィールド + そのライセンスから
// 導出される再配布ルールのプレビューをまとめたウィジェット。
// LibraryMetadataDialog と DuplicateLibraryDialog の両方から使う共通部品。
//
// Custom 選択時だけ、再配布可否・著作権表示要否を手動で指定できる (それ以外の種別は
// redistributionRuleFor() で自動導出される固定値)。
class LicensePickerWidget : public QWidget {
	Q_OBJECT

public:
	explicit LicensePickerWidget(QWidget *parent = nullptr);

	void setLicense(const LicenseInfo &info);
	LicenseInfo license() const;

	// Custom のときの手動指定チェックボックスを初期化する (それ以外の種別では無視される)。
	void setRedistributionRule(const RedistributionRule &rule);
	// 現在の選択に対応する再配布ルール。Custom 以外は redistributionRuleFor(kind)、
	// Custom はチェックボックスの状態から組み立てる。
	RedistributionRule redistributionRule() const;

	// 空なら全種類を選択可能にする (既定)。非空なら列挙された種類のみに絞り込む。
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
	QCheckBox *m_customAllowedCheck;
	QCheckBox *m_customAttributionCheck;
	QLabel *m_previewLabel;
	QVector<LicenseKind> m_allowedKinds;  // 空なら全種類

	void rebuildComboItems();
	void updateCustomFieldsVisibility();
	void updatePreview();
};
