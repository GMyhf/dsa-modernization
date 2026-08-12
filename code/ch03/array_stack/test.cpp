// ArrayStack 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 每一条用例都对着 legacy.md 里的一条缺陷：**如果实现退回原书的写法，
// 这里必须有一条会红**。写新用例时请照这个标准，别写「顺手测一下」的用例。
#include "modern.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what);
    }
}

// 缺陷 6/7：原书 pop 用 `bool pop(T&)` 出参 + cout 提示；这里是 optional。
void test_lifo_order() {
    dsa::ArrayStack<int> s(2);
    s.push(1);
    s.push(2);
    s.push(3);
    check(s.size() == 3, "push 三次后 size==3");
    check(s.top() == 3, "top 是最后压入的");
    check(s.pop() == 3, "pop 出 3");
    check(s.pop() == 2, "pop 出 2");
    check(s.pop() == 1, "pop 出 1");
    check(!s.pop().has_value(), "空栈 pop 返回 nullopt");
    check(!s.top().has_value(), "空栈 top 返回 nullopt");
    check(s.empty(), "弹空后 empty()");
}

// 缺陷 3：原书无参构造只设 top=-1，mSize/st 未初始化，析构 delete[] 野指针。
void test_default_constructed_is_usable() {
    dsa::ArrayStack<int> s;  // 原书这一步之后，析构就是未定义行为
    check(s.empty() && s.size() == 0, "默认构造是空栈");
    check(s.capacity() == 0, "默认构造不预分配");
    check(!s.pop().has_value(), "默认构造的栈可以安全 pop");
    s.push(42);
    check(s.top() == 42, "默认构造的栈可以正常 push");
}

// 缺陷 4：原书没有拷贝构造/拷贝赋值 → 浅拷贝 → 二次释放（ASan 已实测复现，见 legacy.md）。
void test_copy_is_deep() {
    dsa::ArrayStack<int> a(4);
    a.push(1);
    a.push(2);
    dsa::ArrayStack<int> b = a;      // 原书：b.data_ == a.data_
    b.push(3);
    check(a.size() == 2, "改副本不影响原栈的 size");
    check(a.top() == 2, "改副本不影响原栈的栈顶");
    check(b.size() == 3 && b.top() == 3, "副本自身正确");

    dsa::ArrayStack<int> c(1);
    c.push(99);
    c = a;
    check(c.size() == 2 && c.top() == 2, "拷贝赋值得到独立副本");
    dsa::ArrayStack<int>& copy_alias = c;
    c = copy_alias;                   // 自赋值（经别名避开 -Wself-assign-overloaded）
    check(c.size() == 2 && c.top() == 2, "自赋值后仍然完好");
    // 三个对象在这里各自析构一次。原书写法在此处 double-free。
}

void test_move_semantics() {
    dsa::ArrayStack<int> a(4);
    a.push(7);
    dsa::ArrayStack<int> b = std::move(a);
    check(b.size() == 1 && b.top() == 7, "移动后新对象持有数据");
    check(a.empty(), "被移动方处于有效的空状态");
    a.push(8);  // 有效状态意味着可以继续用
    check(a.size() == 1, "被移动方仍可复用");

    dsa::ArrayStack<int> c(2);
    c.push(1);
    c = std::move(b);
    check(c.size() == 1 && c.top() == 7, "移动赋值取得对方的数据");
    // 自移动赋值不得自毁。绕一层引用是因为直接写 `c = std::move(c)`
    // 会被 -Wself-move 拦下（-Werror 下即编译失败）——而运行期真正会撞上这种情况的，
    // 恰恰是「两个引用/指针指向同一对象」这种编译器看不出来的写法。
    dsa::ArrayStack<int>& alias = c;
    c = std::move(alias);
    check(c.size() == 1, "自移动赋值后仍然完好");
}

// 缺陷 8：原书 `bool push(const T item)` 按值传参，move-only 类型根本用不了。
void test_move_only_element() {
    dsa::ArrayStack<std::unique_ptr<int>> s;
    for (int i = 0; i < 10; ++i) {
        s.push(std::make_unique<int>(i));
    }
    check(s.size() == 10, "move-only 元素可以入栈");
    // top() 返回副本，move-only 元素用不了；peek() 零拷贝，可以。
    const std::unique_ptr<int>* seen = s.peek();
    check(seen != nullptr && **seen == 9, "peek 可以观望 move-only 的栈顶元素");
    auto item = s.pop();
    check(item.has_value() && **item == 9, "move-only 元素可以出栈且值正确");
}

// D-001 补充条款（2026-08-12 人拍板）：peek() 是 top() 的零拷贝补充。
struct Counted {
    int v{0};
    static int copies;
    Counted() = default;
    explicit Counted(int x) : v(x) {}
    Counted(const Counted& other) : v(other.v) { ++copies; }
    Counted& operator=(const Counted& other) {
        v = other.v;
        ++copies;
        return *this;
    }
};
int Counted::copies = 0;

void test_peek_does_not_copy() {
    dsa::ArrayStack<Counted> s(4);
    s.push(Counted(1));
    s.push(Counted(2));

    Counted::copies = 0;
    const Counted* p = s.peek();
    check(p != nullptr && p->v == 2, "peek 指向栈顶元素");
    check(Counted::copies == 0, "peek 一次拷贝都不做");

    Counted::copies = 0;
    auto copy = s.top();
    check(copy.has_value() && copy->v == 2, "top 返回等值的副本");
    check(Counted::copies >= 1, "top 确实拷贝了——这正是 peek 存在的理由");

    check(s.size() == 2, "peek/top 都不改变栈");
}

void test_peek_on_empty_is_nullptr() {
    dsa::ArrayStack<int> s;
    check(s.peek() == nullptr, "空栈 peek 返回 nullptr，不是未定义行为");
    s.push(7);
    check(s.peek() != nullptr && *s.peek() == 7, "非空栈 peek 可用");
    (void)s.pop();
    check(s.peek() == nullptr, "弹空后 peek 又回到 nullptr");
}

// 契约的另一半：扩容会换掉整块缓冲区，之前 peek 到的指针随之失效。
// 这里不去解引用旧指针（那是未定义行为），只验证「重新 peek 拿到的是对的」。
void test_peek_after_growth_is_refetched() {
    dsa::ArrayStack<int> s(1);
    s.push(1);
    check(*s.peek() == 1, "扩容前 peek 正确");
    for (int i = 2; i <= 100; ++i) {
        s.push(i);  // 中途必然发生多次扩容
    }
    check(s.peek() != nullptr && *s.peek() == 100, "扩容后重新 peek 仍然正确");
    check(s.at(0) == 1, "扩容后栈底元素完好");
}

// 算法3.3：扩容策略。原书那段按印刷原样根本编译不过（`i` 未声明）。
void test_growth_preserves_contents() {
    dsa::ArrayStack<std::string> s(1);
    const int n = 1000;
    for (int i = 0; i < n; ++i) {
        s.push("item-" + std::to_string(i));
    }
    check(s.size() == static_cast<std::size_t>(n), "扩容后元素个数正确");
    check(s.capacity() >= s.size(), "容量不小于长度");
    bool all_ok = true;
    for (int i = n - 1; i >= 0; --i) {
        auto v = s.pop();
        all_ok = all_ok && v.has_value() && *v == "item-" + std::to_string(i);
    }
    check(all_ok, "扩容过程中 1000 个元素逐个原样保留");

    dsa::ArrayStack<int> g(1);
    std::size_t reallocations = 0, last = g.capacity();
    for (int i = 0; i < 64; ++i) {
        g.push(i);
        if (g.capacity() != last) {
            ++reallocations;
            last = g.capacity();
        }
    }
    // 翻倍策略下 64 次 push 只该重新分配 O(log n) 次；线性增长会是几十次。
    check(reallocations <= 8, "扩容次数是对数级（翻倍策略生效）");
}

// 缺陷 10 的正面验证：扩容中途抛异常，原栈必须原封不动（强异常保证）。
// 这是**故障注入**，不是推理——第 N 次拷贝赋值必抛。
struct Fragile {
    int v{0};
    static int assignments;
    static int throw_at;  // 第几次拷贝赋值抛异常；0 表示不抛

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
};
int Fragile::assignments = 0;
int Fragile::throw_at = 0;

void test_strong_exception_guarantee_on_growth() {
    dsa::ArrayStack<Fragile> s(4);
    Fragile::throw_at = 0;
    Fragile::assignments = 0;
    for (int i = 0; i < 4; ++i) {
        s.push(Fragile(i));  // 填满，下一次 push 必然触发扩容
    }
    const auto size_before = s.size();
    const auto capacity_before = s.capacity();

    // 扩容要搬 4 个元素，让第 3 个搬迁失败
    Fragile::assignments = 0;
    Fragile::throw_at = 3;
    bool threw = false;
    try {
        s.push(Fragile(99));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Fragile::throw_at = 0;

    check(threw, "扩容中途的异常如实抛出，没有被吞掉");
    check(s.size() == size_before, "失败后长度不变");
    check(s.capacity() == capacity_before, "失败后容量不变（没有半途换掉缓冲区）");
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "失败后原有元素逐个完好——强异常保证成立");

    // 失败之后栈仍然可用
    s.push(Fragile(4));
    check(s.size() == size_before + 1, "异常之后栈仍可继续使用");
}

// 红队 T-002：分配本身抛 bad_alloc 时，fresh 尚未取得，旧栈也必须原样保留。
struct AllocationFailure {
    int v{0};
    static bool fail_next_array_allocation;

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
};
bool AllocationFailure::fail_next_array_allocation = false;

void test_bad_alloc_preserves_stack() {
    dsa::ArrayStack<AllocationFailure> s(2);
    s.push(AllocationFailure(1));
    s.push(AllocationFailure(2));
    const auto capacity_before = s.capacity();
    AllocationFailure::fail_next_array_allocation = true;
    bool threw = false;
    try {
        s.push(AllocationFailure(3));
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "new T[next] 的 bad_alloc 如实抛出");
    check(s.size() == 2 && s.capacity() == capacity_before, "bad_alloc 后长度与容量不变");
    check(s.at(0).v == 1 && s.at(1).v == 2, "bad_alloc 后原有元素完好");
}

// 红队 T-002：移动构造 noexcept 并不能证明移动赋值不抛；后者若可抛会破坏强保证。
struct ThrowingMoveAssignment {
    int v{0};
    static int moves;
    static int throw_at;

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
};
int ThrowingMoveAssignment::moves = 0;
int ThrowingMoveAssignment::throw_at = 0;

void test_throwing_move_assignment_preserves_stack() {
    dsa::ArrayStack<ThrowingMoveAssignment> s(4);
    for (int i = 0; i < 4; ++i) {
        s.push(ThrowingMoveAssignment(i));
    }
    ThrowingMoveAssignment::moves = 0;
    ThrowingMoveAssignment::throw_at = 3;
    s.push(ThrowingMoveAssignment(99));
    ThrowingMoveAssignment::throw_at = 0;
    check(s.size() == 5 && s.capacity() == 8, "可复制元素扩容时不走会抛的移动赋值");
    bool intact = true;
    for (std::size_t i = 0; i < 4; ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "移动赋值可抛时扩容后旧元素仍完整");
}

// 缺陷 7：容器不该做 I/O。原书 push/pop 失败时直接 cout 打中文提示。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    {
        dsa::ArrayStack<int> s(1);
        s.push(1);
        s.push(2);       // 触发扩容：原书这里打「栈满溢出」
        (void)s.pop();
        (void)s.pop();
        (void)s.pop();   // 空栈出栈：原书这里打「栈为空，不能执行出栈操作」
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "容器全程不向 cout/cerr 写任何东西");
}

// D-001 第 3 条：越界抛 std::out_of_range，不是未定义行为、也不是打印一行了事。
void test_at_throws_out_of_range() {
    dsa::ArrayStack<int> s(4);
    s.push(10);
    s.push(20);
    check(s.at(0) == 10 && s.at(1) == 20, "at 按栈底起的下标读取");
    bool threw = false;
    try {
        (void)s.at(2);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "越界读取抛 std::out_of_range");
}

void test_clear_keeps_capacity() {
    dsa::ArrayStack<int> s(8);
    s.push(1);
    s.push(2);
    const auto cap = s.capacity();
    s.clear();
    check(s.empty(), "clear 后为空");
    check(s.capacity() == cap, "clear 保留已分配容量");
}

}  // namespace

int main() {
    test_lifo_order();
    test_default_constructed_is_usable();
    test_copy_is_deep();
    test_move_semantics();
    test_move_only_element();
    test_peek_does_not_copy();
    test_peek_on_empty_is_nullptr();
    test_peek_after_growth_is_refetched();
    test_growth_preserves_contents();
    test_strong_exception_guarantee_on_growth();
    test_bad_alloc_preserves_stack();
    test_throwing_move_assignment_preserves_stack();
    test_no_console_output();
    test_at_throws_out_of_range();
    test_clear_keeps_capacity();

    std::printf("ArrayStack: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
