#pragma once

#include <QObject>

class BoardView;

// 表裏ビューの表示位置・倍率を連動させる。有効時、片方をスクロール/ズームすると
// もう片方も「モデル空間で同じ点が中心に来る」ように追従する。BoardScene::toModel/
// fromModel を経由するため、裏面が反転していても正しく合う。
class ViewLinkController : public QObject {
	Q_OBJECT

public:
	explicit ViewLinkController(QObject *parent = nullptr);

	void setViews(BoardView *front, BoardView *back);
	void setEnabled(bool on);
	bool isEnabled() const {
		return m_enabled;
	}

private slots:
	void onFrontChanged();
	void onBackChanged();

private:
	BoardView *m_front = nullptr;
	BoardView *m_back = nullptr;
	bool m_enabled = false;
	bool m_syncing = false;

	void syncTo(BoardView *source, BoardView *target);
};
