#include <QTest>

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
