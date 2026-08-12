// 故障注入探针类型 —— 各章测试共用。
//
// 为什么要有这个文件（T-009）：第 3 章的 test.cpp 里长出了四个探针类型，
// 第 2 章一开工就要原样再写一遍。复制粘贴的探针会各自漂移，
// 到时候「两个单元的强异常保证判据是否一致」就没人说得清了。
//
// **这里只放测试用的探针，不放任何数据结构实现。** 各章的容器必须各自手写——
// 那是教学内容本身（见 collab/DECISION_LOG.md D-001 第 2 条）。
//
// 用法：#include "support/fault_injection.hpp"（tools/check_code.py 已把 code/ 加进包含路径）
#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

namespace dsa::testing {

/// 第 N 次**拷贝赋值**抛异常。用来验证扩容/搬迁的强异常保证。
/// 注意它没有移动赋值（用户声明了拷贝赋值就不会隐式生成），所以搬迁一定走拷贝路径。
struct Fragile {
    int v{0};
    inline static int assignments = 0;
    inline static int throw_at = 0;  // 0 表示不抛

    Fragile() = default;
    explicit Fragile(int x) : v(x) {}
    Fragile(const Fragile&) = default;
    Fragile& operator=(const Fragile& other) {
        if (throw_at != 0 && ++assignments == throw_at) {
            throw std::runtime_error("Fragile: 注入的拷贝失败");
        }
        v = other.v;
        return *this;
    }

    static void reset(int at = 0) {
        assignments = 0;
        throw_at = at;
    }
};

/// 移动构造 `noexcept`、但移动赋值会抛。
///
/// 这个形状就是红队 T-002 打穿第一版实现的那把钥匙：`std::move_if_noexcept`
/// 看的是移动**构造**，而容器搬迁执行的是移动**赋值**，两者异常规格可以不同。
/// 任何「扩容时搬迁元素」的容器都该用它验一遍（见 DECISION_LOG D-005）。
struct ThrowingMoveAssignment {
    int v{0};
    inline static int moves = 0;
    inline static int throw_at = 0;

    ThrowingMoveAssignment() = default;
    explicit ThrowingMoveAssignment(int x) : v(x) {}
    ThrowingMoveAssignment(const ThrowingMoveAssignment&) = default;
    ThrowingMoveAssignment(ThrowingMoveAssignment&& other) noexcept : v(other.v) { other.v = -1; }
    ThrowingMoveAssignment& operator=(const ThrowingMoveAssignment&) = default;
    ThrowingMoveAssignment& operator=(ThrowingMoveAssignment&& other) {
        if (throw_at != 0 && ++moves == throw_at) {
            throw std::runtime_error("ThrowingMoveAssignment: 注入的移动赋值失败");
        }
        v = other.v;
        other.v = -1;
        return *this;
    }

    static void reset(int at = 0) {
        moves = 0;
        throw_at = at;
    }
};

/// 让 `new T[n]` 本身抛 `std::bad_alloc`。验证「分配失败时原容器纹丝不动」。
struct AllocationFailure {
    int v{0};
    inline static bool fail_next_array_allocation = false;

    AllocationFailure() = default;
    explicit AllocationFailure(int x) : v(x) {}

    static void* operator new[](std::size_t bytes) {
        if (fail_next_array_allocation) {
            fail_next_array_allocation = false;
            throw std::bad_alloc();
        }
        return ::operator new[](bytes);
    }
    static void operator delete[](void* p) noexcept { ::operator delete[](p); }

    static void arm() { fail_next_array_allocation = true; }
};

/// 数拷贝次数。用来证明「该走移动的地方没有偷偷深拷贝」。
/// 移动赋值是 `noexcept`，形状与 `std::string`、`std::unique_ptr` 一致。
struct CheapMove {
    int v{0};
    inline static int copies = 0;
    inline static int moves = 0;

    CheapMove() = default;
    explicit CheapMove(int x) : v(x) {}
    CheapMove(const CheapMove&) = default;
    CheapMove(CheapMove&&) noexcept = default;
    CheapMove& operator=(const CheapMove& other) {
        v = other.v;
        ++copies;
        return *this;
    }
    CheapMove& operator=(CheapMove&& other) noexcept {
        v = other.v;
        ++moves;
        return *this;
    }

    static void reset() {
        copies = 0;
        moves = 0;
    }
};

/// 只数拷贝，不数移动。用来证明只读接口（如 peek）确实零拷贝。
struct Counted {
    int v{0};
    inline static int copies = 0;

    Counted() = default;
    explicit Counted(int x) : v(x) {}
    Counted(const Counted& other) : v(other.v) { ++copies; }
    Counted& operator=(const Counted& other) {
        v = other.v;
        ++copies;
        return *this;
    }

    static void reset() { copies = 0; }
};

}  // namespace dsa::testing
