#pragma once

// コピー/ムーブ許可設定用マクロ群。
#define COPYABLE(C)                 \
	C(const C &) = default;        \
	C &operator=(const C &) = default;
#define NONCOPYABLE(C)              \
	C(const C &) = delete;         \
	C &operator=(const C &) = delete;
#define MOVABLE(C)                  \
	C(C &&) = default;             \
	C &operator=(C &&) = default;
#define NONMOVABLE(C)               \
	C(C &&) = delete;              \
	C &operator=(C &&) = delete;
#define MOVEONLY(C) NONCOPYABLE(C) MOVABLE(C)
