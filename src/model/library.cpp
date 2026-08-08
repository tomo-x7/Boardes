#include "library.h"

QString licenseSpdxId(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("CC0-1.0");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("CC-BY-4.0");
	case LicenseKind::MIT:
		return QStringLiteral("MIT");
	case LicenseKind::AllRightsReserved:
	case LicenseKind::Custom:
	default:
		return QString();
	}
}

QString licenseDisplayName(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("CC0 1.0 (権利放棄・自由に利用可)");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("CC BY 4.0 (表示すれば自由に利用可)");
	case LicenseKind::MIT:
		return QStringLiteral("MIT License");
	case LicenseKind::AllRightsReserved:
		return QStringLiteral("全権利留保 (再配布不可)");
	case LicenseKind::Custom:
		return QStringLiteral("カスタム");
	}
	return QStringLiteral("全権利留保 (再配布不可)");
}

LicenseKind licenseKindFromSpdx(const QString &spdx) {
	if (spdx == QStringLiteral("CC0-1.0")) return LicenseKind::CC0_1_0;
	if (spdx == QStringLiteral("CC-BY-4.0")) return LicenseKind::CC_BY_4_0;
	if (spdx == QStringLiteral("MIT")) return LicenseKind::MIT;
	if (spdx.isEmpty()) return LicenseKind::AllRightsReserved;
	// 廃止した種別 (CC-BY-SA-4.0 / CC-BY-NC-4.0 / Apache-2.0 / CERN-OHL-*) を含め、
	// 未知の SPDX は Custom として扱う (呼び出し側が customName に元の文字列を残すこと)。
	return LicenseKind::Custom;
}

QString licenseKindToKey(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("cc0-1.0");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("cc-by-4.0");
	case LicenseKind::MIT:
		return QStringLiteral("mit");
	case LicenseKind::AllRightsReserved:
		return QStringLiteral("all-rights-reserved");
	case LicenseKind::Custom:
		return QStringLiteral("custom");
	}
	return QStringLiteral("all-rights-reserved");
}

LicenseKind licenseKindFromKey(const QString &key) {
	if (key == QStringLiteral("cc0-1.0")) return LicenseKind::CC0_1_0;
	if (key == QStringLiteral("cc-by-4.0")) return LicenseKind::CC_BY_4_0;
	if (key == QStringLiteral("mit")) return LicenseKind::MIT;
	if (key == QStringLiteral("all-rights-reserved") || key.isEmpty()) return LicenseKind::AllRightsReserved;
	// "custom" はもちろん、廃止した種別のキー (cc-by-sa-4.0 / cc-by-nc-4.0 / apache-2.0 /
	// cern-ohl-p-2.0 / cern-ohl-s-2.0 など) もここに来る。黙って全権利留保に落とすと
	// 「再配布可能だったはずのライブラリが不可になる」という驚きになるので、
	// Custom として保持する (呼び出し側 (libraryio.cpp) が既知の旧キーなら customName/
	// customUrl を補完する)。
	return LicenseKind::Custom;
}

RedistributionRule redistributionRuleFor(LicenseKind kind) {
	RedistributionRule rule;
	switch (kind) {
	case LicenseKind::CC0_1_0:
		rule.allowed = true;
		rule.attributionRequired = false;
		break;
	case LicenseKind::MIT:
	case LicenseKind::CC_BY_4_0:
		rule.allowed = true;
		rule.attributionRequired = true;
		break;
	case LicenseKind::AllRightsReserved:
	case LicenseKind::Custom:
	default:
		// Custom は「手動指定」が原則だが、安全側の既定として不可にしておく。
		rule.allowed = false;
		rule.attributionRequired = false;
		break;
	}
	return rule;
}

QVector<QString> Library::partIdsInCategory(const QString &categoryId) const {
	QVector<QString> out;
	for (auto it = partCategory.constBegin(); it != partCategory.constEnd(); ++it) {
		if (it.value() == categoryId) {
			out.append(it.key());
		}
	}
	return out;
}

CategoryInfo *Library::category(const QString &categoryId) {
	for (auto &c : categories) {
		if (c.id == categoryId) {
			return &c;
		}
	}
	return nullptr;
}

const CategoryInfo *Library::category(const QString &categoryId) const {
	for (const auto &c : categories) {
		if (c.id == categoryId) {
			return &c;
		}
	}
	return nullptr;
}
