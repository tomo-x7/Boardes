#include "shiftjis.h"

#include <cstdint>

#include "../vendor/shiftjis_table.h"

namespace shiftjis {

namespace {

// kDoubleByte は sjis コード昇順にソート済みなので二分探索できる。
int findDoubleByte(uint16_t code) {
	int lo = 0;
	int hi = static_cast<int>(sizeof(shiftjis_table::kDoubleByte) / sizeof(shiftjis_table::kDoubleByte[0])) - 1;
	while (lo <= hi) {
		const int mid = (lo + hi) / 2;
		const uint16_t v = shiftjis_table::kDoubleByte[mid].sjis;
		if (v == code) {
			return mid;
		}
		if (v < code) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}
	return -1;
}

bool isLeadByte(uint8_t b) {
	return (b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC);
}

}  // namespace

QString decode(const QByteArray &bytes) {
	QString out;
	out.reserve(bytes.size());

	const int n = bytes.size();
	int i = 0;
	while (i < n) {
		const auto b0 = static_cast<uint8_t>(bytes[i]);

		if (b0 == 0x00) {
			out.append(QChar(0));
			++i;
			continue;
		}

		if (isLeadByte(b0) && i + 1 < n) {
			const auto b1 = static_cast<uint8_t>(bytes[i + 1]);
			const auto code = static_cast<uint16_t>((static_cast<uint16_t>(b0) << 8) | b1);
			const int idx = findDoubleByte(code);
			if (idx >= 0) {
				out.append(QChar(shiftjis_table::kDoubleByte[idx].unicode));
				i += 2;
				continue;
			}
		}

		const uint16_t single = shiftjis_table::kSingleByte[b0];
		if (single != 0) {
			out.append(QChar(single));
		} else {
			out.append(QChar(0xFFFD));  // 置換文字
		}
		++i;
	}

	return out;
}

}  // namespace shiftjis
