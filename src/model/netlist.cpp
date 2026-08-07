#include "netlist.h"

#include <algorithm>
#include <cmath>

#include "document.h"
#include "librarymanager.h"
#include "part.h"
#include "placement.h"

int Netlist::indexFor(QPoint pos, Side side) {
	const NetNode key{pos, side};
	auto it = m_nodeIndex.constFind(key);
	if (it != m_nodeIndex.constEnd()) {
		return it.value();
	}
	const int idx = m_uf.add();
	m_nodeIndex.insert(key, idx);
	return idx;
}

void Netlist::unite(QPoint posA, Side sideA, QPoint posB, Side sideB) {
	m_uf.unite(indexFor(posA, sideA), indexFor(posB, sideB));
}

void Netlist::uniteAlongLine(QPoint from, QPoint to, Side side) {
	const int dx = to.x() - from.x();
	const int dy = to.y() - from.y();
	const int steps = std::max(std::abs(dx), std::abs(dy));
	if (steps == 0) {
		unite(from, side, to, side);
		return;
	}
	// PasS の配線は直交または45度のみなので、1単位刻みで補間すればすべての格子点を
	// 正確に踏める (斜めの場合 dx==±dy になるため、x,y とも毎ステップ ±1 動く)。
	QPoint prev = from;
	for (int i = 1; i <= steps; ++i) {
		const int x = from.x() + static_cast<int>(std::lround(dx * (static_cast<double>(i) / steps)));
		const int y = from.y() + static_cast<int>(std::lround(dy * (static_cast<double>(i) / steps)));
		const QPoint cur(x, y);
		unite(prev, side, cur, side);
		prev = cur;
	}
}

void Netlist::rebuild(const Document &document, LibraryManager *libraryManager) {
	m_nodeIndex.clear();
	m_uf.parent.clear();
	m_rootToNetId.clear();
	m_netCount = 0;

	// 1. 部品ピン (番号あり) がある穴、および ToolThruHole 部品は表裏を貫通する。
	for (const auto &placement : document.placements) {
		const auto part = libraryManager ? libraryManager->resolvePart(placement->libraryId, placement->partId)
										 : nullptr;
		if (!part) {
			continue;
		}
		const bool isThruHole = part->kind == PartKind::ToolThruHole;
		for (const auto &pin : part->pins) {
			if (!pin.hasNumber() && !isThruHole) {
				continue;  // 番号なしの単純なドリル穴はネットに関与しない
			}
			const QPoint world = resolvedPinPosition(part->size(), *placement, pin.pos);
			unite(world, Side::Front, world, Side::Back);
		}
	}

	// 2. 片面基板では、表面配線 (ジャンパ) の経路上の全点で表裏が半田付けされている
	//    ものとして扱う。
	if (!document.board.doubleSided) {
		for (const auto &wire : document.wires) {
			if (wire->layer != WireLayer::FrontBare && wire->layer != WireLayer::FrontInsulated) {
				continue;
			}
			for (const auto &p : wire->points) {
				unite(p, Side::Front, p, Side::Back);
			}
		}
	}

	// 3. 配線自体の接続。
	for (const auto &wire : document.wires) {
		if (wire->layer == WireLayer::Outline || wire->points.size() < 2) {
			continue;
		}
		const auto side = wireSide(wire->layer);
		if (!side.has_value()) {
			continue;
		}
		if (isInsulated(wire->layer)) {
			// 被覆配線: 頂点間だけを接続する (経路の途中は絶縁されている)。
			for (int i = 1; i < wire->points.size(); ++i) {
				unite(wire->points[i - 1], *side, wire->points[i], *side);
			}
		} else {
			// 裸線: 経路上のすべての格子点を接続する。他の配線が同じ格子点を
			// 通っていれば、ノードキー (座標+面) が一致するため自動的に同一ネットになる。
			for (int i = 1; i < wire->points.size(); ++i) {
				uniteAlongLine(wire->points[i - 1], wire->points[i], *side);
			}
		}
	}

	// ネット id を 0 起点の連番に振り直す。
	for (auto it = m_nodeIndex.constBegin(); it != m_nodeIndex.constEnd(); ++it) {
		const int root = m_uf.find(it.value());
		if (!m_rootToNetId.contains(root)) {
			m_rootToNetId.insert(root, m_rootToNetId.size());
		}
	}
	m_netCount = m_rootToNetId.size();
}

int Netlist::netIdAt(QPoint pos, Side side) const {
	const auto it = m_nodeIndex.constFind(NetNode{pos, side});
	if (it == m_nodeIndex.constEnd()) {
		return -1;
	}
	// find() は本来非constだが経路圧縮のためだけの副作用なので、ここでは
	// 経路圧縮なしの素朴な走査で根を求める (netIdAt は const で呼びたいため)。
	int x = it.value();
	while (m_uf.parent[x] != x) {
		x = m_uf.parent[x];
	}
	return m_rootToNetId.value(x, -1);
}

bool Netlist::sameNet(QPoint posA, Side sideA, QPoint posB, Side sideB) const {
	const int a = netIdAt(posA, sideA);
	const int b = netIdAt(posB, sideB);
	return a >= 0 && a == b;
}

QVector<NetNode> Netlist::nodesInNet(int netId) const {
	QVector<NetNode> out;
	for (auto it = m_nodeIndex.constBegin(); it != m_nodeIndex.constEnd(); ++it) {
		if (netIdAt(it.key().pos, it.key().side) == netId) {
			out.append(it.key());
		}
	}
	return out;
}
