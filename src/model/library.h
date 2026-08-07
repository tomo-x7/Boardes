#pragma once

#include <QImage>
#include <QMap>
#include <QString>
#include <QVector>
#include <memory>
#include <optional>

#include "board.h"
#include "part.h"

// ライブラリに設定できるライセンス。
enum class LicenseKind {
	CC0_1_0,
	CC_BY_4_0,
	CC_BY_SA_4_0,
	CC_BY_NC_4_0,
	MIT,
	Apache_2_0,
	CERN_OHL_P,
	CERN_OHL_S,
	AllRightsReserved,
	Custom,
};

QString licenseSpdxId(LicenseKind kind);       // Custom/AllRightsReserved は空文字
QString licenseDisplayName(LicenseKind kind);  // UI 表示用の既定名 (Custom は呼び出し側で customName を使う)
LicenseKind licenseKindFromSpdx(const QString &spdx);

// シリアライズ用の安定した内部キー (SPDX が無い AllRightsReserved/Custom も含めて一意に表せる)。
QString licenseKindToKey(LicenseKind kind);
LicenseKind licenseKindFromKey(const QString &key);

struct LicenseInfo {
	LicenseKind kind = LicenseKind::AllRightsReserved;
	QString customName;        // kind==Custom のときの表示名
	QString customUrl;         // kind==Custom のときの参照 URL
	QString customLicenseText;  // kind==Custom のときのライセンス全文 (任意。非空ならパッケージに LICENSE.txt として同梱)

	QString displayName() const {
		return kind == LicenseKind::Custom ? customName : licenseDisplayName(kind);
	}
};

// 複製・再配布に関するルール。ライセンスから自動導出されるが、
// pass-compat のように明示的に上書きされる場合もあるため Library に保持する。
enum class DerivativePolicy {
	Any,            // 派生物のライセンスは自由に選べる
	MustMatchSame,  // 派生物も同じライセンスに固定 (コピーレフト)
	NcFamilyOnly,   // 派生物は非営利系ライセンスに固定
};

struct RedistributionRule {
	bool allowed = false;
	bool attributionRequired = false;
	DerivativePolicy derivativePolicy = DerivativePolicy::Any;
};

// ライセンス種別から複製時のデフォルトルールを導出する。
RedistributionRule redistributionRuleFor(LicenseKind kind);

// 複製元ライブラリの記録。
struct BasedOn {
	QString libraryId;
	QString name;
	QString version;
	QString licenseLabel;
};

struct CategoryInfo {
	QString id;
	QString name;
	QImage icon;  // 16x16、無ければ null
	int order = 0;
};

// 部品・基板・カテゴリをまとめて管理する配布単位。
class Library {
public:
	QString id;
	QString name;
	QString version;
	QString author;
	QString authorUrl;
	QString homepage;
	QString description;
	LicenseInfo license;
	RedistributionRule redistribution;
	bool readOnly = false;
	std::optional<BasedOn> basedOn;

	QVector<CategoryInfo> categories;
	QMap<QString, std::shared_ptr<Part>> parts;        // partId -> Part
	QMap<QString, QString> partCategory;               // partId -> categoryId
	QMap<QString, std::shared_ptr<BoardSpec>> boards;  // boardId -> BoardSpec

	std::shared_ptr<Part> part(const QString &partId) const {
		return parts.value(partId);
	}
	std::shared_ptr<BoardSpec> board(const QString &boardId) const {
		return boards.value(boardId);
	}
	QVector<QString> partIdsInCategory(const QString &categoryId) const;
	CategoryInfo *category(const QString &categoryId);
	const CategoryInfo *category(const QString &categoryId) const;
};
