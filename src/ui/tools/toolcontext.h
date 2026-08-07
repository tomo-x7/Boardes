#pragma once

#include <QString>
#include <QVector>

#include "../../core/geometry.h"
#include "../../model/placement.h"
#include "../../model/wire.h"

class Document;
class LibraryManager;
class BoardScene;
class SnapEngine;

// 全ツールが共有する編集コンテキスト。ToolManager が保持し、各 Tool に渡す。
struct ToolContext {
	Document *document = nullptr;
	LibraryManager *libraryManager = nullptr;
	BoardScene *frontScene = nullptr;
	BoardScene *backScene = nullptr;
	SnapEngine *snapEngine = nullptr;

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
};
