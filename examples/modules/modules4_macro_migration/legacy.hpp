// 传统头文件: 使用宏
#pragma once

#define LEGACY_VERSION "1.0"
#define LEGACY_MAX(a, b) ((a) > (b) ? (a) : (b))

inline int legacy_add(int a, int b) {
    return a + b;
}
