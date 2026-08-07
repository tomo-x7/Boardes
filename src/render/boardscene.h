#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QString>

#include "../core/geometry.h"
#include "../model/document.h"
#include "../model/wire.h"

class LibraryManager;
class BoardItem;
class PlacementItem;
class WireItem;
class LabelItem;
class OverlayItem;
class ToolManager;
class QGraphicsSceneContextMenuEvent;

// 1つの Document を「表面から見た状態」または「裏面から見た状態」として描画する
// QGraphicsScene。表裏で Document は共有し、BoardScene を2つ (front/back) 作る。
//
// 全アイテムは m_root (鏡像化を担うコンテナ) の子として追加する。裏面シーンでは
// m_root に水平反転の QTransform をかけることで、実際に基板を透かして裏から
// 見たときの配置になる (座標データ自体は表裏で共通)。
class BoardScene : public QGraphicsScene {
	Q_OBJECT

public:
	explicit BoardScene(Side side, QObject *parent = nullptr);
	~BoardScene() override;

	Side side() const {
		return m_side;
	}
	QGraphicsItem *rootItem() const {
		return m_root;
	}

	// document/libraryManager は非所有。document を切り替えたら呼ぶ (nullptr で空にできる)。
	void setDocument(Document *document, LibraryManager *libraryManager);
	Document *document() const {
		return m_document;
	}

	// Document::board / placements / wires が (Undo コマンド経由などで) 変わった後に呼ぶ。
	void syncBoard();
	void syncPlacements();  // ラベルの同期も一緒に行う
	void syncWires();

	void setShowPinNumbers(bool show);
	void setShowLabels(bool show);
	void setForcePartOutline(bool onlyOutline);
	void setLayerVisible(WireLayer layer, bool visible);
	bool isLayerVisible(WireLayer layer) const;

	// uuid から対応する描画アイテムを引く (選択・ヒットテスト等で使う)。
	PlacementItem *placementItemFor(const QString &uuid) const {
		return m_placementItems.value(uuid);
	}
	WireItem *wireItemFor(const QString &uuid) const {
		return m_wireItems.value(uuid);
	}
	QList<PlacementItem *> allPlacementItems() const {
		return m_placementItems.values();
	}
	QList<WireItem *> allWireItems() const {
		return m_wireItems.values();
	}
	OverlayItem *overlay() const {
		return m_overlay;
	}

	// 非所有。マウス/キー入力をアクティブなツールへ転送する先。nullptr ならデフォルトの
	// QGraphicsScene 標準動作 (ラバーバンド選択など) のみになる。
	void setToolManager(ToolManager *toolManager) {
		m_toolManager = toolManager;
	}

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
	Side m_side;
	QGraphicsItem *m_root;
	Document *m_document = nullptr;
	LibraryManager *m_libraryManager = nullptr;

	BoardItem *m_boardItem = nullptr;
	OverlayItem *m_overlay = nullptr;
	QHash<QString, PlacementItem *> m_placementItems;
	QHash<QString, WireItem *> m_wireItems;
	QHash<QString, LabelItem *> m_labelItems;

	bool m_showPinNumbers = false;
	bool m_showLabels = true;
	bool m_forceOutline = false;
	QHash<int, bool> m_layerVisible;  // WireLayer -> visible、既定 true

	ToolManager *m_toolManager = nullptr;

	void syncLabels();
	void clearAllItems();
};
