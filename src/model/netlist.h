#pragma once

#include <QHash>
#include <QPoint>
#include <QVector>

#include "../core/geometry.h"

class Document;
class LibraryManager;

// ネット計算上の1ノード = (単位座標, 面)。
struct NetNode {
	QPoint pos;
	Side side;

	bool operator==(const NetNode &other) const {
		return pos == other.pos && side == other.side;
	}
};

inline size_t qHash(const NetNode &n, size_t seed = 0) {
	return qHashMulti(seed, n.pos.x(), n.pos.y(), static_cast<int>(n.side));
}

// Union-Find で電気的な接続 (ネット) を求める。
//
// 接続規則 (すべて実データ・PasS の仕様書から確認済み):
//   - 部品ピン (番号ありのピン) がある穴          : Front と Back を接続 (足が貫通している)
//   - ToolThruHole 部品のピン位置                 : Front と Back を接続 (スルーホール)
//   - 片面基板 (doubleSided==false) の表面配線    : 経路上の各点で Front と Back を接続
//     (ジャンパ線は裏面で半田付けされるため)
//   - 裸線 (FrontBare/BackBare)                   : 経路上の格子点をすべて接続
//     (途中で別の配線と接触しても導通する)
//   - 被覆配線 (*Insulated)                       : 隣接する頂点間だけを接続
//     (経路の途中で他と接触しても絶縁されている)
//   - 外形線 (Outline)                            : 電気的な意味を持たないため無視
class Netlist {
public:
	void rebuild(const Document &document, LibraryManager *libraryManager);

	int netCount() const {
		return m_netCount;
	}
	// 属するネットが無ければ -1。
	int netIdAt(QPoint pos, Side side) const;
	bool sameNet(QPoint posA, Side sideA, QPoint posB, Side sideB) const;
	QVector<NetNode> nodesInNet(int netId) const;

private:
	struct UnionFind {
		QVector<int> parent;
		int add() {
			parent.append(parent.size());
			return parent.size() - 1;
		}
		int find(int x) {
			while (parent[x] != x) {
				parent[x] = parent[parent[x]];
				x = parent[x];
			}
			return x;
		}
		void unite(int a, int b) {
			a = find(a);
			b = find(b);
			if (a != b) {
				parent[a] = b;
			}
		}
	};

	QHash<NetNode, int> m_nodeIndex;
	UnionFind m_uf;
	QHash<int, int> m_rootToNetId;  // find(root) -> 連番のネットid
	int m_netCount = 0;

	int indexFor(QPoint pos, Side side);
	void unite(QPoint posA, Side sideA, QPoint posB, Side sideB);
	// from -> to の線分上の格子点をすべて連結する (直交・45度の線分のみ正しく機能する)。
	void uniteAlongLine(QPoint from, QPoint to, Side side);
};
