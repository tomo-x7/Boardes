#pragma once

#define COPYABLE(ClassName) \
	ClassName(const ClassName &) = default; \
	ClassName &operator=(const ClassName &) = default;

#define NONCOPYABLE(ClassName) \
	ClassName(const ClassName &) = delete; \
	ClassName &operator=(const ClassName &) = delete;

#define MOVABLE(ClassName) \
	ClassName(ClassName &&) = default; \
	ClassName &operator=(ClassName &&) = default;

#define NONMOVABLE(ClassName) \
	ClassName(ClassName &&) = delete; \
	ClassName &operator=(ClassName &&) = delete;

#define MOVEONLY(ClassName) \
	NONCOPYABLE(ClassName) \
	MOVABLE(ClassName)
