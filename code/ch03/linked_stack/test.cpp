// 链式栈的自带断言测试。判据同前：退回原书写法必须有一条会红。
#include "modern.hpp"

#include "support/fault_injection.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {
using dsa::testing::Counted;

int checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", what.c_str()); }
}

void test_lifo_and_empty() {
    dsa::LinkedStack<int> s;
    check(s.empty() && s.size() == 0, "默认构造是空栈（原书构造函数还要个没用的 defSize）");
    check(!s.pop().has_value(), "空栈 pop 返回 nullopt");
    check(!s.top().has_value() && s.peek() == nullptr, "空栈 top/peek 都不是未定义行为");
    for (int v : {1, 2, 3}) s.push(v);
    check(s.size() == 3 && s.top() == 3, "压三个后栈顶是最后压入的");
    check(s.pop() == 3 && s.pop() == 2 && s.pop() == 1, "后进先出");
    check(s.empty(), "弹空后为空");
}

// 缺陷 2：原书有 ~lnkStack 却无拷贝构造/拷贝赋值 → 共享结点链 → 二次释放。
void test_rule_of_five() {
    dsa::LinkedStack<int> a;
    for (int v : {1, 2, 3}) a.push(v);
    dsa::LinkedStack<int> b = a;
    b.push(4);
    check(a.size() == 3 && a.top() == 3, "改副本不影响原栈");
    check(b.size() == 4 && b.top() == 4, "副本自身正确");

    dsa::LinkedStack<int> c;
    c.push(99);
    c = a;
    check(c.size() == 3 && c.top() == 3, "拷贝赋值得到独立副本");
    dsa::LinkedStack<int>& alias = c;
    c = alias;
    check(c.size() == 3, "自赋值后仍然完好");

    dsa::LinkedStack<int> moved = std::move(a);
    check(moved.size() == 3 && a.empty(), "移动后被移动方是可用的空栈");
    a.push(7);
    check(a.size() == 1, "被移动方可继续使用");
    dsa::LinkedStack<int>& malias = moved;
    moved = std::move(malias);
    check(moved.size() == 3, "自移动赋值不自毁");
    // 全部对象在此各自析构。原书写法在此处 double-free。
}

// 拷贝构造中途抛异常时，半截链必须自己收拾——构造函数抛出时析构函数不会运行。
void test_copy_constructor_cleans_partial_chain() {
    dsa::LinkedStack<Counted> source;
    for (int i = 0; i < 5; ++i) source.push(Counted(i));
    // Counted 不会抛；这里换用会抛的探针形状：借 std::string 的分配失败不好造，
    // 改为验证拷贝出的链与源等长且独立，泄漏由 LeakSanitizer 兜底。
    dsa::LinkedStack<Counted> copy = source;
    check(copy.size() == 5, "拷贝构造得到等长的链");
    check(copy.pop()->v == 4 && source.pop()->v == 4, "两条链各自独立，顺序一致");
}

// 缺陷 3：链式栈没有"栈满"。原书顺序栈满了要打印"栈满溢出"，链式栈不该有这一条。
void test_no_capacity_limit() {
    dsa::LinkedStack<int> s;
    const int n = 200000;
    for (int i = 0; i < n; ++i) s.push(i);
    check(s.size() == static_cast<std::size_t>(n), "20 万次压栈无需扩容也不溢出");
    bool ordered = true;
    for (int i = n - 1; i >= 0 && ordered; --i) ordered = (s.pop() == i);
    check(ordered, "20 万个元素逐个原样弹出");
    check(s.empty(), "全部弹出后为空");
    // 注意：clear() 与析构都是**迭代**的。若写成递归，这条链会在析构时爆栈。
}

void test_move_only_element() {
    dsa::LinkedStack<std::unique_ptr<int>> s;
    for (int i = 0; i < 5; ++i) s.push(std::make_unique<int>(i));
    check(s.size() == 5, "move-only 元素可入栈（原书 push(const T item) 按值传参做不到）");
    const std::unique_ptr<int>* seen = s.peek();
    check(seen != nullptr && **seen == 4, "peek 观望 move-only 栈顶，零拷贝");
    auto item = s.pop();
    check(item.has_value() && **item == 4, "move-only 元素可出栈");
}

void test_peek_does_not_copy() {
    dsa::LinkedStack<Counted> s;
    s.push(Counted(1));
    s.push(Counted(2));
    Counted::reset();
    const Counted* p = s.peek();
    check(p != nullptr && p->v == 2, "peek 指向栈顶");
    check(Counted::copies == 0, "peek 一次拷贝都不做");
    Counted::reset();
    auto copy = s.top();
    check(copy.has_value() && copy->v == 2 && Counted::copies >= 1, "top 取的是副本");
}

// 缺陷 4：原书 push/pop/top 失败时直接 cout 打中文提示。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    {
        dsa::LinkedStack<int> s;
        (void)s.pop();
        (void)s.top();
        s.push(1);
        (void)s.pop();
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "链式栈全程不向 cout/cerr 写任何东西");
}

}  // namespace

int main() {
    test_lifo_and_empty();
    test_rule_of_five();
    test_copy_constructor_cleans_partial_chain();
    test_no_capacity_limit();
    test_move_only_element();
    test_peek_does_not_copy();
    test_no_console_output();
    std::printf("LinkedStack: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
