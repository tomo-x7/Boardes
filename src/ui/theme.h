#pragma once

#include <QColor>
#include <QFont>
#include <QObject>

// アプリ全体の配色テーマ (ライト/ダーク) を管理するシングルトン。Phase 20
// (claude.ai/design「Boardes UI」プロジェクトとの連携によるUI刷新) で追加。
// カラートークンの値は Boardes UI 刷新仕様書.html §3 に基づく。
//
// QSS 自体は resources/qss/light.qss・dark.qss に丸ごと用意してある (CSS変数がQSSに無いため、
// C++側でテンプレート化するのではなく2ファイルを丸ごと切り替える方式)。このクラスは
// (1) 起動時にどちらを読み込むか判定して qApp に適用する、(2) OS 側のテーマ切替に実行時
// 追従する、(3) QSS のプロパティセレクタでは表現しづらい色 (DRC重要度・リンク色・配線色等)を
// コード側 (drcpanel.cpp・icons.cpp 等) に提供する、という3役を担う。
class Theme : public QObject {
	Q_OBJECT
public:
	static Theme &instance();

	// QApplication 生成直後に一度だけ呼ぶ (main.cpp)。フォント登録・初期スタイルシート適用・
	// OS テーマ変更の購読を行う。
	void init();

	bool isDark() const { return m_dark; }

	// OS のテーマ判定を無視して強制的に切り替える。design/screenshot_all.cpp が
	// ライト/ダーク両方のスクリーンショットを撮るためのもので、通常のアプリ実行では
	// 使わない (init() が OS の設定に追従する)。
	void forceTheme(bool dark) { apply(dark); }

	// DRC 重要度 (エラー/警告)。仕様書 §3.2、ライト/ダークで値が異なる。
	QColor errorColor() const;
	QColor warningColor() const;
	// リンク色。仕様書 §16、ライト/ダークで値が異なる。
	QColor linkColor() const;
	QColor linkHoverColor() const;
	// ツールバーアイコンの通常色/強調(チェック)色。text-secondary / text-primary に相当。
	QColor iconNormalColor() const;
	QColor iconActiveColor() const;

	// 以下はテーマに関わらず変えない固定色 (仕様書 §3.3・§9)。
	// 配線色は src/render/items/wireitem.cpp の描画色と同じ値。そちらを変更する場合は
	// こちらも合わせること (QGraphicsView キャンバス内部の描画は今回の刷新の対象外)。
	static QColor wireFrontColor();
	static QColor wireBackColor();
	static QColor wireFrontInsulatedColor();
	static QColor wireBackInsulatedColor();
	// 部品エディタの基準点 (anchor) マーカー色。ピンの赤/青丸と混同しないための固定色。
	static QColor anchorMarkerColor();

	// 数値専用フィールド用の等幅フォント (仕様書§4)。QSS の `[mono="true"]` セレクタは
	// QWidget にしか効かないため、QTableWidgetItem/QTreeWidgetItem のセル文字にはこれを
	// 直接 setFont() する。
	static QFont monoFont();

	// QDialog は「Enter キーで実行される既定ボタン」を自動的に1つ選び、Fusion 系スタイルは
	// それを QPushButton:default (塗りつぶし) の見た目で強調する。QDialogButtonBox の
	// OK/作成/保存ボタンならそれで正しいが、本刷新でダイアログ本文に並べた大量の
	// 単発アクションボタン (「新規ライブラリ...」等) までがこの理由だけで黒塗りに
	// なってしまうと「これが主要アクションだ」という誤った印象を与える。
	// QDialogButtonBox に属さない QPushButton の autoDefault/default を一括で
	// 無効化するヘルパ。各ダイアログのレイアウト構築が終わった直後に呼ぶこと。
	static void suppressAutoDefault(QWidget *root);

signals:
	// ライト/ダークが切り替わった (OS側の変更、または起動時の初回適用) ときに発火する。
	// DRC パネルの再着色・ツールバーアイコンの再生成などが購読する。
	void changed();

private:
	Theme() = default;
	void apply(bool dark);

	bool m_dark = false;
};
