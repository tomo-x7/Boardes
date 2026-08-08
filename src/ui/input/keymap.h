#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <Qt>

#include "inputgesture.h"

class QKeyEvent;

// ショートカット/マウス割り当てのカスタマイズを保持する。commandId → 上書きされた
// InputGesture 一覧。上書きが無いコマンドは CommandDef::defaults をそのまま使う。
// QSettings ("keymap/<commandId>") に保存する (Phase 18)。
//
// 壊れた設定値への対応 (Phase 17 の方針を踏襲): load() は commandId ごとに
// InputGesture::fromStorageString() でデコードし、失敗した値だけを個別に捨てる。
// 1コマンド分が全滅した場合はそのコマンドの上書きを行わない (＝既定値にフォールバック)。
// ダイアログは出さない (ユーザー操作を妨げないため)。
class Keymap : public QObject {
	Q_OBJECT
public:
	explicit Keymap(QObject *parent = nullptr);

	void load();
	void save() const;
	void resetAll();
	void reset(const QString &commandId);

	// commandId に割り当てられているジェスチャー一覧。上書きが無ければ既定値を返す。
	QVector<InputGesture> gesturesFor(const QString &commandId) const;
	void setGestures(const QString &commandId, const QVector<InputGesture> &gestures);
	bool isCustomized(const QString &commandId) const;
	// 既定値から上書きされているコマンド id 一覧 (書き出し/一覧表示用)。
	QStringList customizedCommandIds() const {
		return m_overrides.keys();
	}

	struct Conflict {
		QString commandIdA;
		QString commandIdB;
		InputGesture gesture;
	};
	// 同一カテゴリ内 (および "global" との重複) での割り当て重複。
	// KeymapDialog が警告表示に使う (保存はブロックしない)。
	QVector<Conflict> conflicts() const;

	bool matchesKey(const QString &commandId, const QKeyEvent *event) const;
	bool matchesMouseButton(const QString &commandId, Qt::MouseButton button, Qt::KeyboardModifiers mods,
							 InputKind asKind) const;

	// ステータスバー等に出す表示文字列。複数のジェスチャーは「または」で結合する。
	QString displayFor(const QString &commandId) const;

signals:
	// 割り当てが変わった (KeymapDialog での変更・resetAll 等) ときに発火する。
	// ToolManager はこれを受けてステータスバーのヒントを再表示する。
	void changed();

private:
	QHash<QString, QVector<InputGesture>> m_overrides;
};
