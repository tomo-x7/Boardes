#pragma once

#include <QDialog>
#include <QSet>
#include <QString>

class LibraryManager;
class QCheckBox;

// .bpkg (設計パッケージ) エクスポート時に、依存ライブラリのうちどれを同梱するかを
// 選ばせるダイアログ。再配布可能なものは既定 ON、不可のものは既定 OFF + 赤字。
// 不可のものを ON にしたまま OK すると、続行確認の警告を出す (Phase 14)。
class ExportPackageDialog : public QDialog {
	Q_OBJECT

public:
	// dependencyLibraryIds: 対象ドキュメントが依存する全ライブラリ id。
	ExportPackageDialog(LibraryManager *libraryManager, const QStringList &dependencyLibraryIds,
						QWidget *parent = nullptr);

	// OK で閉じた後、実際に同梱するライブラリ id の集合。
	QSet<QString> includeLibraryIds() const;

private slots:
	void onAccept();

private:
	LibraryManager *m_libraryManager;
	QList<QCheckBox *> m_checks;  // objectName にライブラリ id を持たせている
};
