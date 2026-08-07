#pragma once

#include <QImage>
#include <QString>

class BoardScene;

// 画面表示 (表示レイヤトグルの現在の状態を反映) をラスタ/ベクタ画像として書き出す。
namespace imageexport {

enum class Target {
	Front,
	Back,
	Both,  // 表裏を横に並べた1枚
};

struct Options {
	Target target = Target::Front;
	int scale = 1;  // 1..8 倍
	bool transparentBackground = false;
};

// scene の現在の表示内容 (選択状態は除く) をレンダリングする。
QImage renderToImage(BoardScene *scene, const Options &options);
QImage renderBothSides(BoardScene *front, BoardScene *back, const Options &options);

// target に応じてどちらか/両方を自動選択してレンダリングする便利関数。
QImage render(BoardScene *front, BoardScene *back, const Options &options);

bool saveAsPng(BoardScene *front, BoardScene *back, const Options &options, const QString &filePath);
bool saveAsSvg(BoardScene *front, BoardScene *back, const Options &options, const QString &filePath);

}  // namespace imageexport
