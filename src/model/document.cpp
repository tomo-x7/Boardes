#include "document.h"

#include <QRegularExpression>

Document::Document() : m_undoStack(std::make_unique<QUndoStack>()) {
}

int Document::indexOfPlacement(const QString &uuid) const {
	for (int i = 0; i < placements.size(); ++i) {
		if (placements[i]->uuid == uuid) {
			return i;
		}
	}
	return -1;
}

int Document::indexOfWire(const QString &uuid) const {
	for (int i = 0; i < wires.size(); ++i) {
		if (wires[i]->uuid == uuid) {
			return i;
		}
	}
	return -1;
}

void Document::ensureDependency(const Dependency &dep) {
	for (const auto &d : dependencies) {
		if (d.libraryId == dep.libraryId) {
			return;
		}
	}
	dependencies.append(dep);
}

QString Document::nextRefDes(const QString &prefix) const {
	static const QRegularExpression trailingNumber(QStringLiteral("(\\d+)$"));
	int maxN = 0;
	for (const auto &p : placements) {
		if (!p->refDes.startsWith(prefix)) {
			continue;
		}
		const auto match = trailingNumber.match(p->refDes);
		if (match.hasMatch()) {
			maxN = std::max(maxN, match.captured(1).toInt());
		}
	}
	return prefix + QString::number(maxN + 1);
}

int Document::nextZValue() const {
	int maxZ = -1;
	for (const auto &p : placements) {
		maxZ = std::max(maxZ, p->z);
	}
	return maxZ + 1;
}
