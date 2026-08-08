#pragma once

#include <QPointF>
#include <QVector>

#include "tool.h"

// PasS の「下書きモード」相当。左ボタンを押しながらフリーハンドで線を引ける。
// モデル (Document) には一切影響せず、保存・エクスポートもされない一時的な
// 表示専用レイヤ (BoardScene::overlay()) に描く。
class DraftTool : public Tool {
public:
	explicit DraftTool(ToolContext *context) : Tool(context) {
	}

	void deactivate() override;
	bool mousePress(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseMove(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool mouseRelease(BoardScene *scene, QGraphicsSceneMouseEvent *event) override;
	bool cancel() override;
	QString statusHint(const Keymap &keymap) const override;

private:
	BoardScene *m_activeScene = nullptr;
	QVector<QPointF> m_currentStroke;
};
