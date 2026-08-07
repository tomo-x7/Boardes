#pragma once

#include <QString>
#include <QUuid>

// UUID ベースの ID 生成ヘルパー。配置・配線など「同一性」が意味を持つオブジェクトに使う。
namespace ids {

inline QString newUuid() {
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace ids
