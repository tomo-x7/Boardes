#include "library.h"

QString licenseSpdxId(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("CC0-1.0");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("CC-BY-4.0");
	case LicenseKind::CC_BY_SA_4_0:
		return QStringLiteral("CC-BY-SA-4.0");
	case LicenseKind::CC_BY_NC_4_0:
		return QStringLiteral("CC-BY-NC-4.0");
	case LicenseKind::MIT:
		return QStringLiteral("MIT");
	case LicenseKind::Apache_2_0:
		return QStringLiteral("Apache-2.0");
	case LicenseKind::CERN_OHL_P:
		return QStringLiteral("CERN-OHL-P-2.0");
	case LicenseKind::CERN_OHL_S:
		return QStringLiteral("CERN-OHL-S-2.0");
	case LicenseKind::AllRightsReserved:
	case LicenseKind::Custom:
	default:
		return QString();
	}
}

QString licenseDisplayName(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("CC0 1.0 (パブリックドメイン相当)");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("CC BY 4.0 (表示)");
	case LicenseKind::CC_BY_SA_4_0:
		return QStringLiteral("CC BY-SA 4.0 (表示-継承)");
	case LicenseKind::CC_BY_NC_4_0:
		return QStringLiteral("CC BY-NC 4.0 (表示-非営利)");
	case LicenseKind::MIT:
		return QStringLiteral("MIT License");
	case LicenseKind::Apache_2_0:
		return QStringLiteral("Apache License 2.0");
	case LicenseKind::CERN_OHL_P:
		return QStringLiteral("CERN-OHL-P-2.0 (Permissive)");
	case LicenseKind::CERN_OHL_S:
		return QStringLiteral("CERN-OHL-S-2.0 (Strongly Reciprocal)");
	case LicenseKind::AllRightsReserved:
		return QStringLiteral("全権利留保");
	case LicenseKind::Custom:
		return QStringLiteral("カスタム");
	}
	return QStringLiteral("全権利留保");
}

LicenseKind licenseKindFromSpdx(const QString &spdx) {
	if (spdx == QStringLiteral("CC0-1.0")) return LicenseKind::CC0_1_0;
	if (spdx == QStringLiteral("CC-BY-4.0")) return LicenseKind::CC_BY_4_0;
	if (spdx == QStringLiteral("CC-BY-SA-4.0")) return LicenseKind::CC_BY_SA_4_0;
	if (spdx == QStringLiteral("CC-BY-NC-4.0")) return LicenseKind::CC_BY_NC_4_0;
	if (spdx == QStringLiteral("MIT")) return LicenseKind::MIT;
	if (spdx == QStringLiteral("Apache-2.0")) return LicenseKind::Apache_2_0;
	if (spdx == QStringLiteral("CERN-OHL-P-2.0")) return LicenseKind::CERN_OHL_P;
	if (spdx == QStringLiteral("CERN-OHL-S-2.0")) return LicenseKind::CERN_OHL_S;
	if (spdx.isEmpty()) return LicenseKind::AllRightsReserved;
	return LicenseKind::Custom;
}

QString licenseKindToKey(LicenseKind kind) {
	switch (kind) {
	case LicenseKind::CC0_1_0:
		return QStringLiteral("cc0-1.0");
	case LicenseKind::CC_BY_4_0:
		return QStringLiteral("cc-by-4.0");
	case LicenseKind::CC_BY_SA_4_0:
		return QStringLiteral("cc-by-sa-4.0");
	case LicenseKind::CC_BY_NC_4_0:
		return QStringLiteral("cc-by-nc-4.0");
	case LicenseKind::MIT:
		return QStringLiteral("mit");
	case LicenseKind::Apache_2_0:
		return QStringLiteral("apache-2.0");
	case LicenseKind::CERN_OHL_P:
		return QStringLiteral("cern-ohl-p-2.0");
	case LicenseKind::CERN_OHL_S:
		return QStringLiteral("cern-ohl-s-2.0");
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
	if (key == QStringLiteral("cc-by-sa-4.0")) return LicenseKind::CC_BY_SA_4_0;
	if (key == QStringLiteral("cc-by-nc-4.0")) return LicenseKind::CC_BY_NC_4_0;
	if (key == QStringLiteral("mit")) return LicenseKind::MIT;
	if (key == QStringLiteral("apache-2.0")) return LicenseKind::Apache_2_0;
	if (key == QStringLiteral("cern-ohl-p-2.0")) return LicenseKind::CERN_OHL_P;
	if (key == QStringLiteral("cern-ohl-s-2.0")) return LicenseKind::CERN_OHL_S;
	if (key == QStringLiteral("custom")) return LicenseKind::Custom;
	return LicenseKind::AllRightsReserved;
}

RedistributionRule redistributionRuleFor(LicenseKind kind) {
	RedistributionRule rule;
	switch (kind) {
	case LicenseKind::CC0_1_0:
		rule.allowed = true;
		rule.attributionRequired = false;
		rule.derivativePolicy = DerivativePolicy::Any;
		break;
	case LicenseKind::MIT:
	case LicenseKind::Apache_2_0:
	case LicenseKind::CC_BY_4_0:
	case LicenseKind::CERN_OHL_P:
		rule.allowed = true;
		rule.attributionRequired = true;
		rule.derivativePolicy = DerivativePolicy::Any;
		break;
	case LicenseKind::CC_BY_SA_4_0:
	case LicenseKind::CERN_OHL_S:
		rule.allowed = true;
		rule.attributionRequired = true;
		rule.derivativePolicy = DerivativePolicy::MustMatchSame;
		break;
	case LicenseKind::CC_BY_NC_4_0:
		rule.allowed = true;
		rule.attributionRequired = true;
		rule.derivativePolicy = DerivativePolicy::NcFamilyOnly;
		break;
	case LicenseKind::AllRightsReserved:
	case LicenseKind::Custom:
	default:
		rule.allowed = false;
		rule.attributionRequired = false;
		rule.derivativePolicy = DerivativePolicy::Any;
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
