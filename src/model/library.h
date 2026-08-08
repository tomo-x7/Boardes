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
//
// 方針: 手元にあるライブラリファイルはユーザーのものであり、バックアップや編集は
// 常に自由にできる。それはそれとして、公開・再配布はライセンスで示された原作者の
// 意向を尊重する (Phase 14)。派生物のライセンスを強制固定する仕組み (コピーレフト
// 強制) は Boardes 側では持たない — 尊重すべきは「再配布してよいか」までであり、
// 派生ライセンスの選択はユーザーの責任とする。選択肢が多すぎると分かりにくいため、
// よく使われる5種類に絞ってある。
enum class LicenseKind {
	CC0_1_0,            // 権利放棄。表示不要で自由に利用可
	CC_BY_4_0,           // 表示すれば自由に利用可
	MIT,                 // MIT License
	AllRightsReserved,   // 全権利留保 (既定)。再配布不可
	Custom,              // カスタム。再配布可否・表示義務は手動指定
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

// 複製・再配布に関するルール。ライセンスから自動導出されるが、pass-compat のように
// 明示的に上書きされる場合もあるため Library に保持する。
struct RedistributionRule {
	bool allowed = false;
	bool attributionRequired = false;
};

// ライセンス種別から複製時のデフォルトルールを導出する。kind==Custom のときは
// 「不可」を安全側の既定にする (呼び出し側の UI がユーザーに明示選択させること)。
RedistributionRule redistributionRuleFor(LicenseKind kind);

// 複製元・取り込み元ライブラリの記録。1つとは限らない (他ライブラリから部品を
// 複製した場合など、複数の由来を持ちうる) ので配列で持つ。
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
//
// readOnly は廃止した (Phase 14) — 手元にあるライブラリはすべて編集・バックアップが
// 自由。公開・再配布の可否だけを redistribution.allowed で管理する。
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
	QVector<BasedOn> basedOn;

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
