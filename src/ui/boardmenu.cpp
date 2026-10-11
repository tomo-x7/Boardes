#include "boardmenu.h"

#include <algorithm>

#include "../model/librarymanager.h"

namespace {

// ライブラリから選んだ基板は id ごと設計データへコピーされる。id・名前・格子・サイズが同じなら
// 「今使っている基板」とみなしてチェックを付ける (別ライブラリへ複製した同じ基板にも付く)。
bool sameBoard(const BoardSpec &a, const BoardSpec &b) {
	return a.id == b.id && a.name == b.name && a.cols == b.cols && a.rows == b.rows && a.pitch == b.pitch &&
		   a.size == b.size;
}

QString displayName(const BoardSpec &board) {
	return board.name.isEmpty() ? board.id : board.name;
}

QString gridText(const BoardSpec &board) {
	return QStringLiteral("%1×%2").arg(board.cols).arg(board.rows);
}

// メニュー項目の文字列では & がアクセラレータ扱いになるので重ねてエスケープする。
QString escapeMnemonic(QString text) {
	return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

}  // namespace

BoardMenu::BoardMenu(LibraryManager *libraryManager, QWidget *parent)
	: QMenu(tr("基板(&B)"), parent), m_libraryManager(libraryManager) {
	connect(this, &QMenu::aboutToShow, this, &BoardMenu::rebuild);
	rebuild();
}

QString BoardMenu::buttonText(const BoardSpec *board) {
	if (!hasBoard(board)) {
		return tr("基板: 未設定");
	}
	return tr("基板: %1 (%2)").arg(escapeMnemonic(displayName(*board)), gridText(*board));
}

void BoardMenu::rebuild() {
	// clear() はサブメニュー自体を消さないので、前回作ったものを先に捨てる。
	qDeleteAll(findChildren<QMenu *>(Qt::FindDirectChildrenOnly));
	clear();

	QStringList libraryIds;
	if (m_libraryManager) {
		const auto &libs = m_libraryManager->libraries();
		for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
			if (!it.value()->boards.isEmpty()) {
				libraryIds << it.key();
			}
		}
	}

	if (libraryIds.isEmpty()) {
		QAction *none = addAction(tr("ライブラリに基板がありません"));
		none->setEnabled(false);
	} else if (libraryIds.size() == 1) {
		// 基板を持つライブラリが1つだけなら、階層を挟まずにそのまま並べる。
		addBoardActions(this, libraryIds.first());
	} else {
		for (const QString &libId : libraryIds) {
			const auto lib = m_libraryManager->library(libId);
			QMenu *sub =
				addMenu(QStringLiteral("%1 (%2)").arg(escapeMnemonic(lib->name), QString::number(lib->boards.size())));
			addBoardActions(sub, libId);
		}
	}

	addSeparator();
	const bool hasCurrent = hasBoard(m_currentBoard);
	QAction *editAction = addAction(tr("現在の基板を編集..."));
	editAction->setEnabled(hasCurrent);
	connect(editAction, &QAction::triggered, this, &BoardMenu::editCurrentRequested);
	QAction *newAction = addAction(tr("新規基板..."));
	connect(newAction, &QAction::triggered, this, &BoardMenu::newBoardRequested);
	QAction *registerAction = addAction(tr("現在の基板をマイライブラリに登録"));
	registerAction->setEnabled(hasCurrent);
	connect(registerAction, &QAction::triggered, this, &BoardMenu::registerCurrentRequested);
}

void BoardMenu::addBoardActions(QMenu *menu, const QString &libraryId) {
	const auto lib = m_libraryManager->library(libraryId);
	if (!lib) {
		return;
	}
	QList<std::shared_ptr<BoardSpec>> boards = lib->boards.values();
	// 並びはライブラリ管理ダイアログのツリーと同じ (名前順)。
	std::sort(boards.begin(), boards.end(),
			  [](const auto &a, const auto &b) { return displayName(*a) < displayName(*b); });
	const bool hasCurrent = hasBoard(m_currentBoard);
	for (const auto &board : boards) {
		// タブ以降はショートカット欄と同じ位置に右寄せで出る。格子の大きさを並べて見比べられるようにする。
		QAction *a =
			menu->addAction(QStringLiteral("%1\t%2").arg(escapeMnemonic(displayName(*board)), gridText(*board)));
		a->setCheckable(true);
		a->setChecked(hasCurrent && sameBoard(*m_currentBoard, *board));
		connect(a, &QAction::triggered, this, [this, board] { emit boardChosen(*board); });
	}
}
