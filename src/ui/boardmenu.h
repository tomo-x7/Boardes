#pragma once

#include <QMenu>

#include "../model/board.h"

class LibraryManager;

// メインウィンドウの「基板」メニュー。メニューバーとツールバーの基板ボタンの両方から同じものを開く。
// ライブラリに入っている基板を一覧し、選ぶとそのまま現在の設計データに使う (以前はライブラリ管理
// ダイアログを経由しないと基板を選べなかった)。一覧は開くたび (aboutToShow) に作り直すので、
// ライブラリの変更を追いかける必要は無い。
//
// 設計データ (Document) には触らず、操作はすべてシグナルで MainWindow に任せる
// (LibraryManagerDialog::boardApplyRequested と同じ分担)。
class BoardMenu : public QMenu {
	Q_OBJECT

public:
	explicit BoardMenu(LibraryManager *libraryManager, QWidget *parent = nullptr);

	// 現在の設計データの基板。一覧のチェック表示と、編集・登録の有効/無効に使う。
	// 非所有。設計データを入れ替えたら設定し直すこと。
	void setCurrentBoard(const BoardSpec *board) {
		m_currentBoard = board;
	}

	// 一覧を作り直す。aboutToShow で自動的に呼ばれる (テストからも呼べるよう公開している)。
	void rebuild();

	// ツールバーの基板ボタンに出す文字列 (「基板: 名前 (列×行)」、未設定なら「基板: 未設定」)。
	static QString buttonText(const BoardSpec *board);
	// 基板が設定されているか (新規の設計データは基板サイズが空)。
	static bool hasBoard(const BoardSpec *board) {
		return board && !board->size.isEmpty();
	}

signals:
	// ライブラリの基板が選ばれた。
	void boardChosen(const BoardSpec &board);
	void editCurrentRequested();
	void newBoardRequested();
	void registerCurrentRequested();

private:
	LibraryManager *m_libraryManager;
	const BoardSpec *m_currentBoard = nullptr;

	void addBoardActions(QMenu *menu, const QString &libraryId);
};
