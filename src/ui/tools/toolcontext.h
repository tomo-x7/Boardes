#pragma once

#include <QString>
#include <QVector>

#include "../../core/geometry.h"
#include "../../model/placement.h"
#include "../../model/wire.h"
#include "../input/keymap.h"

class Document;
class LibraryManager;
class BoardScene;
class SnapEngine;
class ToolManager;

// 全ツールが共有する編集コンテキスト。ToolManager が保持し、各 Tool に渡す。
struct ToolContext {
	Document *document = nullptr;
	LibraryManager *libraryManager = nullptr;
	BoardScene *frontScene = nullptr;
	BoardScene *backScene = nullptr;
	SnapEngine *snapEngine = nullptr;
	// 非所有。ツールが「自分自身を解除して選択ツールに戻る」ために使う
	// (例: 配置ツール中の右クリック)。ToolManager のコンストラクタが自分自身を設定する。
	ToolManager *toolManager = nullptr;
	// 非所有。ショートカット/マウス割り当てのカスタマイズ (Phase 18)。MainWindow が
	// 生成・保持し、常に非 null であることが前提 (ToolManager 構築前に設定すること)。
	const Keymap *keymap = nullptr;

	// PlacePartTool 用: 現在配置しようとしている部品と、その回転状態。
	QString pendingLibraryId;
	QString pendingPartId;
	Rotation pendingRotation = Rotation::R0;

	// コピー&ペースト用の簡易クリップボード (uuid は貼り付け時に新規発行する)。
	QVector<Placement> clipboardPlacements;
	QVector<Wire> clipboardWires;

	BoardScene *sceneFor(Side side) const {
		return side == Side::Front ? frontScene : backScene;
	}
	void syncBothScenesPlacements() const;
	void syncBothScenesWires() const;

	// keymap が未設定 (主にテストで省略された場合) でも安全に呼べるよう、
	// 何もカスタマイズされていない既定値のみのフォールバックを返す。
	const Keymap &keymapOrDefault() const {
		static const Keymap fallback;
		return keymap ? *keymap : fallback;
	}
};
