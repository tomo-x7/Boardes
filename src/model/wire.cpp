#include "wire.h"

QString wireLayerToString(WireLayer layer) {
	switch (layer) {
	case WireLayer::BackBare:
		return QStringLiteral("backBare");
	case WireLayer::FrontBare:
		return QStringLiteral("frontBare");
	case WireLayer::BackInsulated:
		return QStringLiteral("backInsulated");
	case WireLayer::FrontInsulated:
		return QStringLiteral("frontInsulated");
	case WireLayer::Outline:
		return QStringLiteral("outline");
	}
	return QStringLiteral("frontBare");
}

WireLayer wireLayerFromString(const QString &s) {
	if (s == QStringLiteral("backBare")) return WireLayer::BackBare;
	if (s == QStringLiteral("backInsulated")) return WireLayer::BackInsulated;
	if (s == QStringLiteral("frontInsulated")) return WireLayer::FrontInsulated;
	if (s == QStringLiteral("outline")) return WireLayer::Outline;
	return WireLayer::FrontBare;
}
