// ArrayList 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准同第 3 章：**如果实现退回原书的写法，这里必须有一条会红**。
// 探针类型来自 support/fault_injection.hpp（T-009），与第 3 章共用同一批形状，
// 免得两个单元对"强异常保证"各有一套判据。
#include "modern.hpp"

#include "support/fault_injection.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using dsa::testing::AllocationFailure;
using dsa::testing::CheapMove;
using dsa::testing::Fragile;
using dsa::testing::ThrowingMoveAssignment;

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what);
    }
}

// 缺陷 3：原书 arrList 有析构函数却没有拷贝构造/拷贝赋值 → 浅拷贝 → 二次释放。
void test_copy_is_deep() {
    dsa::ArrayList<int> a(4);
    a.append(1);
    a.append(2);
    dsa::ArrayList<int> b = a;
    b.append(3);
    check(a.size() == 2 && b.size() == 3, "副本独立增长");
    check(a.at(0) == 1 && a.at(1) == 2, "原表不受副本影响");

    dsa::ArrayList<int> c(1);
    c.append(99);
    c = a;
    check(c.size() == 2 && c.at(1) == 2, "拷贝赋值得到独立副本");
    dsa::ArrayList<int>& alias = c;
    c = alias;  // 自赋值（经别名避开 -Wself-assign-overloaded）
    check(c.size() == 2 && c.at(1) == 2, "自赋值后仍然完好");
    // 三个对象各自析构一次。原书写法在此处 double-free。
}

void test_move_semantics() {
    dsa::ArrayList<int> a(4);
    a.append(7);
    dsa::ArrayList<int> b = std::move(a);
    check(b.size() == 1 && b.at(0) == 7, "移动后新对象持有数据");
    check(a.empty(), "被移动方是有效的空表");
    a.append(8);
    check(a.size() == 1, "被移动方仍可复用");

    dsa::ArrayList<int> c(2);
    c.append(1);
    c = std::move(b);
    check(c.size() == 1 && c.at(0) == 7, "移动赋值取得对方的数据");
    dsa::ArrayList<int>& alias = c;
    c = std::move(alias);  // 自移动赋值不得自毁
    check(c.size() == 1, "自移动赋值后仍然完好");
}

// 【算法2.4】插入。原书按印刷原样有 OCR 损伤，逻辑本身还有「溢出就打印一行」的毛病。
void test_insert_positions() {
    dsa::ArrayList<std::string> s;
    s.append("b");
    s.insert(0, "a");            // 表头
    s.insert(2, "d");            // 表尾（pos == size 合法）
    s.insert(2, "c");            // 中间
    check(s.size() == 4, "四次插入后长度为 4");
    const char* expect[] = {"a", "b", "c", "d"};
    bool ordered = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        ordered = ordered && s.at(i) == expect[i];
    }
    check(ordered, "插入后次序为 a b c d");

    bool threw = false;
    try {
        s.insert(99, "x");
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "插入位置非法抛 out_of_range");
    check(s.size() == 4, "非法插入不改变表");
}

// 【算法2.5】删除。原书返回 bool + 打印；这里返回被删元素，位置非法抛异常。
void test_remove() {
    dsa::ArrayList<int> s;
    for (int i = 0; i < 5; ++i) {
        s.append(i * 10);  // 0 10 20 30 40
    }
    check(s.remove(0) == 0, "删表头返回被删元素");
    check(s.remove(3) == 40, "删表尾返回被删元素");
    check(s.size() == 3 && s.at(0) == 10 && s.at(2) == 30, "删除后剩余元素左移就位");

    bool threw = false;
    try {
        (void)s.remove(3);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "删除位置非法抛 out_of_range");

    dsa::ArrayList<int> empty_list;
    threw = false;
    try {
        (void)empty_list.remove(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "空表删除抛 out_of_range（空表是下标非法的特例）");
}

// 【算法2.3】查找。原书用 `int i; for (i = 0; i < n; ...)`——`n` 根本没声明，编译不过。
void test_find() {
    dsa::ArrayList<int> s;
    for (int v : {5, 7, 5, 9}) {
        s.append(v);
    }
    auto first = s.find(5);
    check(first.has_value() && *first == 0, "find 返回第一次出现的下标");
    check(s.find(9).has_value() && *s.find(9) == 3, "find 能找到表尾元素");
    check(!s.find(1234).has_value(), "找不到时返回 nullopt");

    dsa::ArrayList<int> empty_list;
    check(!empty_list.find(0).has_value(), "空表 find 返回 nullopt");
}

// 缺陷 5：原书 getValue/setValue 越界时打印一行再返回 false。
void test_bounds_are_exceptions() {
    dsa::ArrayList<int> s(4);
    s.append(1);
    check(s.at(0) == 1, "at 读到正确的值");
    s.set(0, 42);
    check(s.at(0) == 42, "set 改到正确的位置");

    int thrown = 0;
    try { (void)s.at(1); } catch (const std::out_of_range&) { ++thrown; }
    try { s.set(1, 0); } catch (const std::out_of_range&) { ++thrown; }
    const dsa::ArrayList<int>& const_ref = s;
    try { (void)const_ref.at(9); } catch (const std::out_of_range&) { ++thrown; }
    check(thrown == 3, "at/set 越界一律抛 out_of_range（含 const 重载）");
}

// 原书的 position 游标住在容器里；这里把遍历状态放到容器外。
void test_range_for_and_const_iteration() {
    dsa::ArrayList<int> s;
    for (int i = 1; i <= 4; ++i) {
        s.append(i);
    }
    int sum = 0;
    for (int v : s) {
        sum += v;
    }
    check(sum == 10, "range-for 能遍历");

    const dsa::ArrayList<int>& const_ref = s;
    int const_sum = 0;
    for (int v : const_ref) {
        const_sum += v;
    }
    check(const_sum == 10, "const 表也能遍历——原书的 position 游标做不到");

    int nested = 0;
    for (int a : const_ref) {
        for (int b : const_ref) {
            nested += a * b;
        }
    }
    check(nested == 100, "嵌套遍历互不干扰——共享游标会在这里踩坏");
}

// 容量：原书固定 maxSize，满了打印 "The list is overflow" 然后拒绝插入。
void test_growth() {
    dsa::ArrayList<int> s(1);
    for (int i = 0; i < 100; ++i) {
        s.append(i);
    }
    check(s.size() == 100, "自动扩容后长度正确");
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        intact = intact && s.at(i) == static_cast<int>(i);
    }
    check(intact, "扩容过程中元素逐个保留");

    dsa::ArrayList<int> g(1);
    std::size_t reallocations = 0, last = g.capacity();
    for (int i = 0; i < 64; ++i) {
        g.append(i);
        if (g.capacity() != last) {
            ++reallocations;
            last = g.capacity();
        }
    }
    check(reallocations <= 8, "扩容次数是对数级（翻倍策略生效）");

    const auto cap = s.capacity();
    s.clear();
    check(s.empty() && s.capacity() == cap, "clear 保留已分配容量，不重新分配");
}

// D-005：扩容搬迁的判据是「移动赋值抛不抛」，不是 move_if_noexcept 看的移动构造。
void test_strong_exception_guarantee_on_growth() {
    dsa::ArrayList<Fragile> s(4);
    Fragile::reset();
    for (int i = 0; i < 4; ++i) {
        s.append(Fragile(i));
    }
    const auto size_before = s.size();
    const auto capacity_before = s.capacity();

    Fragile::reset(3);  // 搬第 3 个元素时抛
    bool threw = false;
    try {
        s.append(Fragile(99));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Fragile::reset();

    check(threw, "扩容中途的异常如实抛出");
    check(s.size() == size_before && s.capacity() == capacity_before, "失败后长度与容量不变");
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "失败后原有元素逐个完好——强异常保证成立");
}

void test_throwing_move_assignment_preserves_list() {
    dsa::ArrayList<ThrowingMoveAssignment> s(4);
    for (int i = 0; i < 4; ++i) {
        s.append(ThrowingMoveAssignment(i));
    }
    ThrowingMoveAssignment::reset(3);
    s.append(ThrowingMoveAssignment(99));  // 可复制 → 搬迁走拷贝，不该踩到会抛的移动赋值
    ThrowingMoveAssignment::reset();
    check(s.size() == 5, "扩容成功");
    bool intact = true;
    for (std::size_t i = 0; i < 4; ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "移动赋值可抛时扩容不走它，旧元素完整");
}

void test_growth_moves_when_move_assignment_is_noexcept() {
    dsa::ArrayList<CheapMove> s(1);
    CheapMove::reset();
    for (int i = 0; i < 64; ++i) {
        s.append(CheapMove(i));
    }
    check(CheapMove::copies == 0, "移动赋值 noexcept 的元素，扩容时一次都不该拷贝");
    check(CheapMove::moves > 64, "扩容搬迁确实走了移动");
}

void test_bad_alloc_preserves_list() {
    dsa::ArrayList<AllocationFailure> s(2);
    s.append(AllocationFailure(1));
    s.append(AllocationFailure(2));
    const auto capacity_before = s.capacity();
    AllocationFailure::arm();
    bool threw = false;
    try {
        s.append(AllocationFailure(3));
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "new T[next] 的 bad_alloc 如实抛出");
    check(s.size() == 2 && s.capacity() == capacity_before, "bad_alloc 后长度与容量不变");
    check(s.at(0).v == 1 && s.at(1).v == 2, "bad_alloc 后原有元素完好");
}

// 缺陷 4：原书 insert/delete/getValue 失败时直接往 cout 打英文提示。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    {
        dsa::ArrayList<int> s(1);
        s.append(1);
        s.append(2);  // 触发扩容：原书这里打 "The list is overflow"
        try { s.insert(99, 0); } catch (const std::out_of_range&) {}
        try { (void)s.remove(99); } catch (const std::out_of_range&) {}
        try { (void)s.at(99); } catch (const std::out_of_range&) {}
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "容器全程不向 cout/cerr 写任何东西");
}

// 缺陷 6：原书 `bool append(const T value)` 按值传参，move-only 类型用不了。
void test_move_only_element() {
    dsa::ArrayList<std::unique_ptr<int>> s;
    for (int i = 0; i < 8; ++i) {
        s.append(std::make_unique<int>(i));
    }
    check(s.size() == 8, "move-only 元素可以入表");
    check(*s.at(3) == 3, "move-only 元素可以按下标访问");
    auto removed = s.remove(0);
    check(removed != nullptr && *removed == 0, "move-only 元素可以被删除并取回");
    check(s.size() == 7 && *s.at(0) == 1, "删除后其余元素左移就位");
}

}  // namespace

int main() {
    test_copy_is_deep();
    test_move_semantics();
    test_insert_positions();
    test_remove();
    test_find();
    test_bounds_are_exceptions();
    test_range_for_and_const_iteration();
    test_growth();
    test_strong_exception_guarantee_on_growth();
    test_throwing_move_assignment_preserves_list();
    test_growth_moves_when_move_assignment_is_noexcept();
    test_bad_alloc_preserves_list();
    test_no_console_output();
    test_move_only_element();

    std::printf("ArrayList: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
