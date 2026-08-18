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

/// 移动是 `noexcept` 的，但**拷贝**会在第 N 次抛异常。
///
/// 用途：验证「只在移动路径上要求 noexcept」的容器（如第 5 章 MinHeap，
/// 其 static_assert 只约束移动）在**拷贝构造**失败时是否清理干净。
/// `Fragile` 在这类容器上根本实例化不了——它的移动赋值不是 noexcept——
/// 所以需要这个形状单独覆盖拷贝路径。
struct NothrowMoveThrowingCopy {
    int v{0};
    inline static int copies = 0;
    inline static int throw_at = 0;  // 0 表示不抛

    NothrowMoveThrowingCopy() = default;
    explicit NothrowMoveThrowingCopy(int x) : v(x) {}
    NothrowMoveThrowingCopy(NothrowMoveThrowingCopy&&) noexcept = default;
    NothrowMoveThrowingCopy& operator=(NothrowMoveThrowingCopy&&) noexcept = default;

    NothrowMoveThrowingCopy(const NothrowMoveThrowingCopy& other) : v(other.v) { bump(); }
    NothrowMoveThrowingCopy& operator=(const NothrowMoveThrowingCopy& other) {
        bump();
        v = other.v;
        return *this;
    }

    static void reset(int at = 0) {
        copies = 0;
        throw_at = at;
    }

    /// 堆之类的容器要比较元素；比较不计拷贝、不抛异常。
    friend bool operator<(const NothrowMoveThrowingCopy& a,
                          const NothrowMoveThrowingCopy& b) noexcept {
        return a.v < b.v;
    }

private:
    static void bump() {
        if (throw_at != 0 && ++copies == throw_at) {
            throw std::runtime_error("NothrowMoveThrowingCopy: 注入的拷贝失败");
        }
    }
};

/// 第 N 次**拷贝构造**抛异常。
///
/// `Fragile` 打的是拷贝**赋值**——那是「先默认构造整块槽位、再往里赋值」的容器
/// （`new T[n]`）才会走的路。改成裸存储 + placement new 之后，搬迁执行的是
/// 拷贝**构造**，injection 也得跟着挪到构造上，否则用例会安静地不再触发（T-004）。
///
/// 它没有移动构造（用户声明了拷贝构造就不会隐式生成），所以 `std::move_if_noexcept`
/// 必然选中拷贝构造这条路。
struct ThrowingCopyConstruction {
    int v{0};
    inline static int copies = 0;
    inline static int throw_at = 0;  // 0 表示不抛

    ThrowingCopyConstruction() = default;
    explicit ThrowingCopyConstruction(int x) : v(x) {}
    ThrowingCopyConstruction(const ThrowingCopyConstruction& other) : v(other.v) {
        if (throw_at != 0 && ++copies == throw_at) {
            throw std::runtime_error("ThrowingCopyConstruction: 注入的拷贝构造失败");
        }
    }
    ThrowingCopyConstruction& operator=(const ThrowingCopyConstruction&) = default;

    static void reset(int at = 0) {
        copies = 0;
        throw_at = at;
    }
};

/// 移动构造**不是** `noexcept` 且会抛；拷贝构造正常。
///
/// 这是 `std::move_if_noexcept` 的判据本身：遇到这种 T，扩容必须退回拷贝，
/// 于是注入的移动失败**一次都不该发生**。用它守住「强异常保证没被移动优化吃掉」。
struct ThrowingMoveConstruction {
    int v{0};
    inline static int moves = 0;
    inline static int copies = 0;
    inline static int throw_at = 0;

    ThrowingMoveConstruction() = default;
    explicit ThrowingMoveConstruction(int x) : v(x) {}
    ThrowingMoveConstruction(const ThrowingMoveConstruction& other) : v(other.v) { ++copies; }
    ThrowingMoveConstruction(ThrowingMoveConstruction&& other) : v(other.v) {  // 故意不 noexcept
        ++moves;
        if (throw_at != 0 && moves == throw_at) {
            throw std::runtime_error("ThrowingMoveConstruction: 注入的移动构造失败");
        }
        other.v = -1;
    }
    ThrowingMoveConstruction& operator=(const ThrowingMoveConstruction&) = default;

    static void reset(int at = 0) {
        moves = 0;
        copies = 0;
        throw_at = at;
    }
};

/// 数**构造**次数（不是赋值）。移动构造是 `noexcept`，形状与 `std::string` 一致。
/// 用来证明「移动构造不抛的 T，扩容时一次都不该深拷贝」。
struct CountedConstruction {
    int v{0};
    inline static int copies = 0;
    inline static int moves = 0;

    CountedConstruction() = default;
    explicit CountedConstruction(int x) : v(x) {}
    CountedConstruction(const CountedConstruction& other) : v(other.v) { ++copies; }
    CountedConstruction(CountedConstruction&& other) noexcept : v(other.v) { ++moves; }
    CountedConstruction& operator=(const CountedConstruction&) = default;
    CountedConstruction& operator=(CountedConstruction&&) noexcept = default;

    static void reset() {
        copies = 0;
        moves = 0;
    }
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
