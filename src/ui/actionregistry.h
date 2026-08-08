#pragma once

#include <QHash>
#include <QList>
#include <QString>

class QAction;

// commandId → QAction* の対応表。ツールバーのカスタマイズ (Phase 19) が
// 「どの commandId をどのツールバーに置くか」を扱うための土台。QAction 自体は
// MainWindow が所有し続ける (ここは非所有の登録簿)。同じ QAction を複数の
// ツールバーに置くことも Qt の仕組み上そのままでき、ここでは1つの commandId に
// つき1つの QAction を対応付ければよい。
class ActionRegistry {
public:
	void add(const QString &commandId, QAction *action);
	QAction *action(const QString &commandId) const;
	// 登録順 (カスタマイズダイアログの「利用可能なコマンド」表示順として使う)。
	QList<QString> commandIds() const {
		return m_order;
	}

private:
	QHash<QString, QAction *> m_actions;
	QList<QString> m_order;
};
