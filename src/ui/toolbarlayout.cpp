#include "toolbarlayout.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSettings>

namespace toolbarlayout {

namespace {

QJsonObject toJson(const ToolbarLayout &layout) {
	QJsonObject o;
	o["id"] = layout.id;
	o["title"] = layout.title;
	o["builtin"] = layout.builtin;
	o["visible"] = layout.visible;
	o["style"] = static_cast<int>(layout.style);
	o["items"] = QJsonArray::fromStringList(layout.items);
	return o;
}

// 形式が壊れていれば false を返す (呼び出し側は全体を defaults() にフォールバックする)。
bool fromJson(const QJsonValue &v, ToolbarLayout &out) {
	if (!v.isObject()) {
		return false;
	}
	const QJsonObject o = v.toObject();
	if (!o.contains("id") || !o["id"].isString() || o["id"].toString().isEmpty()) {
		return false;
	}
	if (!o.contains("items") || !o["items"].isArray()) {
		return false;
	}
	out.id = o["id"].toString();
	out.title = o["title"].toString();
	out.builtin = o["builtin"].toBool();
	out.visible = o["visible"].toBool(true);
	out.style = static_cast<Qt::ToolButtonStyle>(o["style"].toInt(static_cast<int>(Qt::ToolButtonTextBesideIcon)));
	if (out.style < Qt::ToolButtonIconOnly || out.style > Qt::ToolButtonFollowStyle) {
		out.style = Qt::ToolButtonTextBesideIcon;
	}
	out.items.clear();
	for (const auto &item : o["items"].toArray()) {
		out.items << item.toString();
	}
	return true;
}

}  // namespace

const QVector<ToolbarLayout> &defaults() {
	static const QVector<ToolbarLayout> layouts = {
		{QStringLiteral("view"), QObject::tr("表示"), true, true, Qt::ToolButtonTextBesideIcon,
		 {QStringLiteral("toolbar.view.zoomIn"), QStringLiteral("toolbar.view.zoomOut"),
		  QStringLiteral("toolbar.view.zoomReset"), QStringLiteral("toolbar.view.fit"), QStringLiteral("-"),
		  QStringLiteral("toolbar.view.orientation"), QStringLiteral("toolbar.view.linkViews"), QStringLiteral("-"),
		  QStringLiteral("toolbar.view.wireFrontBare"), QStringLiteral("toolbar.view.wireBackBare"),
		  QStringLiteral("toolbar.view.wireFrontInsulated"), QStringLiteral("toolbar.view.wireBackInsulated"),
		  QStringLiteral("toolbar.view.outline"), QStringLiteral("toolbar.view.partOutline"),
		  QStringLiteral("toolbar.view.pinNumbers"), QStringLiteral("toolbar.view.labels"),
		  QStringLiteral("toolbar.view.pinMarkers"), QStringLiteral("toolbar.view.pinMarkerColor")}},
		{QStringLiteral("tools"), QObject::tr("ツール"), true, true, Qt::ToolButtonTextBesideIcon,
		 {QStringLiteral("toolbar.tools.select"), QStringLiteral("-"), QStringLiteral("toolbar.tools.wireBare"),
		  QStringLiteral("toolbar.tools.wireInsulated"), QStringLiteral("toolbar.tools.wireOutline"),
		  QStringLiteral("-"), QStringLiteral("toolbar.tools.draft"), QStringLiteral("-"),
		  QStringLiteral("widget:snapGranularity")}},
	};
	return layouts;
}

QVector<ToolbarLayout> load() {
	QSettings settings;
	if (!settings.contains(QStringLiteral("toolbars/layout"))) {
		return defaults();
	}
	const QByteArray raw = settings.value(QStringLiteral("toolbars/layout")).toByteArray();
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
	if (err.error != QJsonParseError::NoError || !doc.isArray()) {
		return defaults();  // 壊れていれば黙って既定値に戻す (ダイアログは出さない)
	}
	QVector<ToolbarLayout> layouts;
	for (const auto &v : doc.array()) {
		ToolbarLayout layout;
		if (fromJson(v, layout)) {
			layouts.append(layout);
		}
		// 個々のツールバー定義が壊れていても、そこだけ捨てて残りは使う。
	}
	if (layouts.isEmpty()) {
		return defaults();
	}
	return layouts;
}

void save(const QVector<ToolbarLayout> &layouts) {
	QJsonArray arr;
	for (const auto &layout : layouts) {
		arr.append(toJson(layout));
	}
	QSettings settings;
	settings.setValue(QStringLiteral("toolbars/layout"), QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

}  // namespace toolbarlayout
