#pragma once

#include <QString>
#include <Qt>
#include <optional>

// キーボード/マウスの「1つの操作」を表す最小単位 (Phase 18: 操作カスタマイズ)。
// ショートカットキーだけでなく、マウスボタン・ダブルクリック・ドラッグ・ホイールも
// 同じ型で表す。Qt が名前を知らないキー (左手デバイス等の独自キー) は
// nativeScanCode で識別できるようにしてある。
enum class InputKind {
	Key,          // キーボードの単発押下
	MouseButton,  // マウスボタンの単発押下 (右クリックでの確定、等)
	MouseDouble,  // ダブルクリック
	MouseDrag,    // ボタンを押したままドラッグ (パン等)
	Wheel,        // ホイール回転
};

struct InputGesture {
	InputKind kind = InputKind::Key;
	int key = 0;                          // Qt::Key。0 なら nativeScanCode 側で識別する
	quint32 nativeScanCode = 0;           // key が 0 (Qt が認識できないキー) のときの識別子
	Qt::KeyboardModifiers mods = Qt::NoModifier;
	Qt::MouseButton button = Qt::NoButton;
	int wheelDelta = 0;                   // +1 = 奥へ回す / -1 = 手前へ (0 = 未使用)

	bool isValid() const;

	// 人間向けの表示文字列。例: "Ctrl+R" / "右クリック" / "スキャンコード #191"。
	QString toDisplayString() const;

	// QSettings 保存用のシリアライズ。
	QString toStorageString() const;
	static std::optional<InputGesture> fromStorageString(const QString &s);

	// キーボードイベントにマッチするか。key が 0 の場合は nativeScanCode で照合する
	// (Qt がキー名を認識できない、左手デバイス等の独自キーに対応するため)。
	bool matchesKey(int eventKey, quint32 eventNativeScanCode, Qt::KeyboardModifiers eventMods) const;

	// マウスボタン系 (MouseButton/MouseDouble/MouseDrag) の入力にマッチするか。
	// asKind は呼び出し側が「今どの種類の判定をしているか」を渡す
	// (同じ右クリックでも押下判定とドラッグ判定を区別するため)。
	bool matchesMouseButton(Qt::MouseButton eventButton, Qt::KeyboardModifiers eventMods, InputKind asKind) const;

	bool operator==(const InputGesture &other) const;
	bool operator!=(const InputGesture &other) const {
		return !(*this == other);
	}
};
