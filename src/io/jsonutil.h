#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QPoint>
#include <QRect>
#include <QSize>

// JSON シリアライズで繰り返し使う小さな変換ヘルパー群。
namespace jsonutil {

inline QJsonArray fromPoint(QPoint p) {
	return QJsonArray{p.x(), p.y()};
}
inline QPoint toPoint(const QJsonArray &a, QPoint def = QPoint()) {
	if (a.size() < 2) return def;
	return QPoint(a[0].toInt(), a[1].toInt());
}

inline QJsonArray fromSize(QSize s) {
	return QJsonArray{s.width(), s.height()};
}
inline QSize toSize(const QJsonArray &a, QSize def = QSize()) {
	if (a.size() < 2) return def;
	return QSize(a[0].toInt(), a[1].toInt());
}

// [x, y, w, h]
inline QJsonArray fromRect(QRect r) {
	return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}
inline QRect toRect(const QJsonArray &a, QRect def = QRect()) {
	if (a.size() < 4) return def;
	return QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
}

inline QString fromColor(QColor c) {
	return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}
inline QColor toColor(const QString &s, QColor def = QColor()) {
	if (s.isEmpty()) return def;
	QColor c(s);
	return c.isValid() ? c : def;
}

inline QJsonArray fromPointList(const QVector<QPoint> &pts) {
	QJsonArray out;
	for (const auto &p : pts) {
		out.append(fromPoint(p));
	}
	return out;
}
inline QVector<QPoint> toPointList(const QJsonArray &a) {
	QVector<QPoint> out;
	out.reserve(a.size());
	for (const auto &v : a) {
		out.append(toPoint(v.toArray()));
	}
	return out;
}

}  // namespace jsonutil
