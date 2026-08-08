#include "inputgesture.h"

#include <QKeySequence>
#include <QObject>
#include <QStringList>

namespace {

QString modsPrefix(Qt::KeyboardModifiers mods) {
	QStringList parts;
	if (mods & Qt::ControlModifier) parts << QStringLiteral("Ctrl");
	if (mods & Qt::AltModifier) parts << QStringLiteral("Alt");
	if (mods & Qt::ShiftModifier) parts << QStringLiteral("Shift");
	if (mods & Qt::MetaModifier) parts << QStringLiteral("Meta");
	if (parts.isEmpty()) {
		return QString();
	}
	return parts.join(QLatin1Char('+')) + QLatin1Char('+');
}

QString mouseButtonName(Qt::MouseButton b) {
	switch (b) {
	case Qt::LeftButton:
		return QObject::tr("左クリック");
	case Qt::RightButton:
		return QObject::tr("右クリック");
	case Qt::MiddleButton:
		return QObject::tr("中クリック");
	default:
		return QObject::tr("マウスボタン");
	}
}

QString kindTag(InputKind kind) {
	switch (kind) {
	case InputKind::Key:
		return QStringLiteral("key");
	case InputKind::MouseButton:
		return QStringLiteral("mousePress");
	case InputKind::MouseDouble:
		return QStringLiteral("mouseDouble");
	case InputKind::MouseDrag:
		return QStringLiteral("mouseDrag");
	case InputKind::Wheel:
		return QStringLiteral("wheel");
	}
	return QString();
}

}  // namespace

bool InputGesture::isValid() const {
	switch (kind) {
	case InputKind::Key:
		return key != 0 || nativeScanCode != 0;
	case InputKind::MouseButton:
	case InputKind::MouseDouble:
	case InputKind::MouseDrag:
		return button != Qt::NoButton;
	case InputKind::Wheel:
		return wheelDelta != 0;
	}
	return false;
}

QString InputGesture::toDisplayString() const {
	switch (kind) {
	case InputKind::Key:
		if (key != 0) {
			return modsPrefix(mods) + QKeySequence(key).toString(QKeySequence::NativeText);
		}
		return modsPrefix(mods) + QObject::tr("スキャンコード #%1").arg(nativeScanCode);
	case InputKind::MouseButton:
		return modsPrefix(mods) + mouseButtonName(button);
	case InputKind::MouseDouble:
		return modsPrefix(mods) + QObject::tr("%1(ダブル)").arg(mouseButtonName(button));
	case InputKind::MouseDrag:
		return modsPrefix(mods) + QObject::tr("%1ドラッグ").arg(mouseButtonName(button));
	case InputKind::Wheel:
		return modsPrefix(mods) + (wheelDelta >= 0 ? QObject::tr("ホイール奥") : QObject::tr("ホイール手前"));
	}
	return QString();
}

QString InputGesture::toStorageString() const {
	switch (kind) {
	case InputKind::Key:
		return QStringLiteral("%1:%2:%3:%4").arg(kindTag(kind)).arg(key).arg(nativeScanCode).arg(int(mods));
	case InputKind::MouseButton:
	case InputKind::MouseDouble:
	case InputKind::MouseDrag:
		return QStringLiteral("%1:%2:%3").arg(kindTag(kind)).arg(int(button)).arg(int(mods));
	case InputKind::Wheel:
		return QStringLiteral("%1:%2:%3").arg(kindTag(kind)).arg(wheelDelta).arg(int(mods));
	}
	return QString();
}

std::optional<InputGesture> InputGesture::fromStorageString(const QString &s) {
	const QStringList parts = s.split(QLatin1Char(':'));
	if (parts.isEmpty()) {
		return std::nullopt;
	}
	InputGesture g;
	bool ok1 = true, ok2 = true, ok3 = true;
	if (parts[0] == QStringLiteral("key")) {
		if (parts.size() != 4) return std::nullopt;
		g.kind = InputKind::Key;
		g.key = parts[1].toInt(&ok1);
		g.nativeScanCode = parts[2].toUInt(&ok2);
		g.mods = Qt::KeyboardModifiers(parts[3].toInt(&ok3));
	} else if (parts[0] == QStringLiteral("mousePress") || parts[0] == QStringLiteral("mouseDouble") ||
			   parts[0] == QStringLiteral("mouseDrag")) {
		if (parts.size() != 3) return std::nullopt;
		g.kind = parts[0] == QStringLiteral("mousePress")   ? InputKind::MouseButton
				 : parts[0] == QStringLiteral("mouseDouble") ? InputKind::MouseDouble
															   : InputKind::MouseDrag;
		g.button = Qt::MouseButton(parts[1].toInt(&ok1));
		g.mods = Qt::KeyboardModifiers(parts[2].toInt(&ok2));
	} else if (parts[0] == QStringLiteral("wheel")) {
		if (parts.size() != 3) return std::nullopt;
		g.kind = InputKind::Wheel;
		g.wheelDelta = parts[1].toInt(&ok1);
		g.mods = Qt::KeyboardModifiers(parts[2].toInt(&ok2));
	} else {
		return std::nullopt;
	}
	if (!ok1 || !ok2 || !ok3 || !g.isValid()) {
		return std::nullopt;
	}
	return g;
}

bool InputGesture::matchesKey(int eventKey, quint32 eventNativeScanCode, Qt::KeyboardModifiers eventMods) const {
	if (kind != InputKind::Key) {
		return false;
	}
	if (mods != eventMods) {
		return false;
	}
	if (key != 0) {
		return key == eventKey;
	}
	return nativeScanCode != 0 && nativeScanCode == eventNativeScanCode;
}

bool InputGesture::matchesMouseButton(Qt::MouseButton eventButton, Qt::KeyboardModifiers eventMods,
									  InputKind asKind) const {
	if (kind != asKind) {
		return false;
	}
	return button == eventButton && mods == eventMods;
}

bool InputGesture::operator==(const InputGesture &other) const {
	return kind == other.kind && key == other.key && nativeScanCode == other.nativeScanCode &&
		   mods == other.mods && button == other.button && wheelDelta == other.wheelDelta;
}
