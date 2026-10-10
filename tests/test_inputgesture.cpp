#include <QKeyEvent>
#include <QTest>

#include "ui/input/gesturecapturedialog.h"
#include "ui/input/inputgesture.h"

// Phase 18: InputGesture の判定・シリアライズのユニットテスト。
class TestInputGesture : public QObject {
	Q_OBJECT

private slots:
	void keyGestureRoundTripsThroughStorageString();
	void mouseGestureRoundTripsThroughStorageString();
	void wheelGestureRoundTripsThroughStorageString();
	void fromStorageStringRejectsMalformedInput();
	void matchesKeyRequiresExactModifiers();
	void matchesKeyIgnoresKeypadModifierOnBothSides();
	void captureDoesNotStoreKeypadModifier();
	void matchesKeyFallsBackToNativeScanCode();
	void matchesMouseButtonDistinguishesKind();
	void invalidGesturesAreRejected();
};

void TestInputGesture::keyGestureRoundTripsThroughStorageString() {
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_R;
	g.mods = Qt::ControlModifier | Qt::ShiftModifier;

	const QString stored = g.toStorageString();
	const auto restored = InputGesture::fromStorageString(stored);
	QVERIFY(restored.has_value());
	QCOMPARE(*restored, g);
}

void TestInputGesture::mouseGestureRoundTripsThroughStorageString() {
	for (InputKind kind : {InputKind::MouseButton, InputKind::MouseDouble, InputKind::MouseDrag}) {
		InputGesture g;
		g.kind = kind;
		g.button = Qt::RightButton;
		g.mods = Qt::NoModifier;
		const auto restored = InputGesture::fromStorageString(g.toStorageString());
		QVERIFY(restored.has_value());
		QCOMPARE(*restored, g);
	}
}

void TestInputGesture::wheelGestureRoundTripsThroughStorageString() {
	InputGesture g;
	g.kind = InputKind::Wheel;
	g.wheelDelta = -1;
	g.mods = Qt::AltModifier;
	const auto restored = InputGesture::fromStorageString(g.toStorageString());
	QVERIFY(restored.has_value());
	QCOMPARE(*restored, g);
}

void TestInputGesture::fromStorageStringRejectsMalformedInput() {
	QVERIFY(!InputGesture::fromStorageString(QString()).has_value());
	QVERIFY(!InputGesture::fromStorageString(QStringLiteral("nonsense")).has_value());
	QVERIFY(!InputGesture::fromStorageString(QStringLiteral("key:not-a-number:0:0")).has_value());
	QVERIFY(!InputGesture::fromStorageString(QStringLiteral("key:82:0")).has_value());  // 要素数不足
	// key=0 かつ nativeScanCode=0 は isValid() が false になるため拒否される。
	QVERIFY(!InputGesture::fromStorageString(QStringLiteral("key:0:0:0")).has_value());
}

void TestInputGesture::matchesKeyRequiresExactModifiers() {
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = Qt::Key_R;
	g.mods = Qt::NoModifier;

	QVERIFY(g.matchesKey(Qt::Key_R, 0, Qt::NoModifier));
	QVERIFY(!g.matchesKey(Qt::Key_R, 0, Qt::ShiftModifier));  // 修飾キーが余分だと一致しない
	QVERIFY(!g.matchesKey(Qt::Key_F, 0, Qt::NoModifier));     // キー自体が違う
}

void TestInputGesture::matchesKeyIgnoresKeypadModifierOnBothSides() {
	// テンキーの Enter は Key_Enter + KeypadModifier として届く。
	InputGesture enter;
	enter.kind = InputKind::Key;
	enter.key = Qt::Key_Enter;
	enter.mods = Qt::NoModifier;
	QVERIFY(enter.matchesKey(Qt::Key_Enter, 0, Qt::NoModifier));
	QVERIFY(enter.matchesKey(Qt::Key_Enter, 0, Qt::KeypadModifier));

	// 以前に記録された、KeypadModifier ごと保存されている割り当ても、どちらにも一致する。
	InputGesture recorded = enter;
	recorded.mods = Qt::KeypadModifier;
	QVERIFY(recorded.matchesKey(Qt::Key_Enter, 0, Qt::NoModifier));
	QVERIFY(recorded.matchesKey(Qt::Key_Enter, 0, Qt::KeypadModifier));

	// ほかの修飾キーは、KeypadModifier が付いていても厳密に比較する。
	QVERIFY(!enter.matchesKey(Qt::Key_Enter, 0, Qt::KeypadModifier | Qt::ShiftModifier));
	QVERIFY(!recorded.matchesKey(Qt::Key_Enter, 0, Qt::ControlModifier));
	InputGesture ctrlC;
	ctrlC.kind = InputKind::Key;
	ctrlC.key = Qt::Key_C;
	ctrlC.mods = Qt::ControlModifier;
	QVERIFY(ctrlC.matchesKey(Qt::Key_C, 0, Qt::ControlModifier | Qt::KeypadModifier));
	QVERIFY(!ctrlC.matchesKey(Qt::Key_C, 0, Qt::KeypadModifier));
	QVERIFY(!ctrlC.matchesKey(Qt::Key_C, 0, Qt::ControlModifier | Qt::ShiftModifier | Qt::KeypadModifier));

	// キーそのものは今までどおり区別する。ネイティブスキャンコードでの照合も同じ。
	QVERIFY(!enter.matchesKey(Qt::Key_Return, 0, Qt::KeypadModifier));
	InputGesture scanCode;
	scanCode.kind = InputKind::Key;
	scanCode.nativeScanCode = 191;
	QVERIFY(scanCode.matchesKey(0, 191, Qt::KeypadModifier));
	QVERIFY(!scanCode.matchesKey(0, 192, Qt::KeypadModifier));
}

void TestInputGesture::captureDoesNotStoreKeypadModifier() {
	// テンキーの Enter を Shift と一緒に押す。Shift は記録するが、KeypadModifier は記録しない。
	GestureCaptureDialog shifted(InputKind::Key);
	QKeyEvent shiftedPress(QEvent::KeyPress, Qt::Key_Enter, Qt::KeypadModifier | Qt::ShiftModifier);
	QApplication::sendEvent(&shifted, &shiftedPress);
	QVERIFY(shifted.capturedGesture().has_value());
	QCOMPARE(shifted.capturedGesture()->key, static_cast<int>(Qt::Key_Enter));
	QCOMPARE(shifted.capturedGesture()->mods, Qt::KeyboardModifiers(Qt::ShiftModifier));

	// テンキーの Enter だけなら、修飾キーなしの Enter として記録される。通常の Enter と区別が
	// 付かない割り当てになり (保存される値も同じ)、どちらのキーにも一致する。
	GestureCaptureDialog plain(InputKind::Key);
	QKeyEvent keypadPress(QEvent::KeyPress, Qt::Key_Enter, Qt::KeypadModifier);
	QApplication::sendEvent(&plain, &keypadPress);
	QVERIFY(plain.capturedGesture().has_value());
	InputGesture mainEnter;
	mainEnter.kind = InputKind::Key;
	mainEnter.key = Qt::Key_Enter;
	QCOMPARE(*plain.capturedGesture(), mainEnter);
	QCOMPARE(plain.capturedGesture()->toStorageString(), mainEnter.toStorageString());
	QVERIFY(plain.capturedGesture()->matchesKey(Qt::Key_Enter, 0, Qt::NoModifier));
	QVERIFY(plain.capturedGesture()->matchesKey(Qt::Key_Enter, 0, Qt::KeypadModifier));
}

void TestInputGesture::matchesKeyFallsBackToNativeScanCode() {
	// Qt がキー名を認識できない (左手デバイス等の) キーは key==0 で、
	// nativeScanCode だけで識別する。
	InputGesture g;
	g.kind = InputKind::Key;
	g.key = 0;
	g.nativeScanCode = 191;
	g.mods = Qt::NoModifier;

	QVERIFY(g.matchesKey(0, 191, Qt::NoModifier));
	QVERIFY(!g.matchesKey(0, 192, Qt::NoModifier));
	// ジェスチャー側の key が 0 (未設定) の間は、イベント側の key の値に関わらず
	// nativeScanCode だけで判定する。
	QVERIFY(g.matchesKey(Qt::Key_R, 191, Qt::NoModifier));
	QVERIFY(!g.matchesKey(Qt::Key_R, 192, Qt::NoModifier));
}

void TestInputGesture::matchesMouseButtonDistinguishesKind() {
	InputGesture g;
	g.kind = InputKind::MouseDrag;
	g.button = Qt::MiddleButton;
	g.mods = Qt::NoModifier;

	QVERIFY(g.matchesMouseButton(Qt::MiddleButton, Qt::NoModifier, InputKind::MouseDrag));
	// 同じボタン・同じ修飾キーでも、判定に使う種類 (asKind) が違えば一致しない
	// (単発クリックとドラッグを取り違えないようにするため)。
	QVERIFY(!g.matchesMouseButton(Qt::MiddleButton, Qt::NoModifier, InputKind::MouseButton));
	QVERIFY(!g.matchesMouseButton(Qt::LeftButton, Qt::NoModifier, InputKind::MouseDrag));
}

void TestInputGesture::invalidGesturesAreRejected() {
	InputGesture keyGesture;
	keyGesture.kind = InputKind::Key;
	QVERIFY(!keyGesture.isValid());  // key==0 かつ nativeScanCode==0

	InputGesture mouseGesture;
	mouseGesture.kind = InputKind::MouseButton;
	QVERIFY(!mouseGesture.isValid());  // button==NoButton

	InputGesture wheelGesture;
	wheelGesture.kind = InputKind::Wheel;
	QVERIFY(!wheelGesture.isValid());  // wheelDelta==0
}

QTEST_MAIN(TestInputGesture)
#include "test_inputgesture.moc"
