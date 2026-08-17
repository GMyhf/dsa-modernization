// 教学版 LinkedStack 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

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

void test_lifo_order() {
    LinkedStack<int> s;
    s.push(1);
    s.push(2);
    s.push(3);
    check(s.size() == 3, "push 三次后 size==3");
    check(s.top() == 3, "top 是最后压入的");
    check(s.pop() == 3, "第一次 pop 得到 3");
    check(s.pop() == 2, "第二次 pop 得到 2");
    check(s.pop() == 1, "第三次 pop 得到 1");
    check(s.empty(), "全部弹出后为空");
}

void test_empty_returns_nullopt() {
    LinkedStack<int> s;
    check(s.empty(), "新建的链式栈是空的");
    check(s.size() == 0, "空栈 size 为 0");
    check(!s.pop().has_value(), "空栈 pop 返回 nullopt");
    check(!s.top().has_value(), "空栈 top 返回 nullopt");
}

// 链式栈没有「栈满」这回事：结点分散在堆上，压多少个都不需要扩容。
// 这一条是 3.3.1 拿它与顺序栈对比的核心依据。
void test_no_overflow() {
    LinkedStack<int> s;
    for (int i = 0; i < 10000; ++i) {
        s.push(i);
    }
    check(s.size() == 10000, "压入一万个结点，一个都没丢");
    bool all_ok = true;
    for (int i = 9999; i >= 0; --i) {
        if (s.pop() != i) {
            all_ok = false;
        }
    }
    check(all_ok, "一万个结点逆序原样弹出");
    check(s.empty(), "弹完之后为空");
}

// 原书缺陷：有 ~lnkStack(){clear();} 却没有拷贝构造 → 两个栈共享结点链。
// 变异实测：删掉拷贝构造，`LinkedStack<int> b = a;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    LinkedStack<int> a;
    a.push(1);
    a.push(2);
    a.push(3);
    LinkedStack<int> b = a;
    check(b.size() == 3, "副本长度一致");
    check(b.pop() == 3, "副本栈顶一致");
    check(b.pop() == 2, "副本次序一致（不是被拷反了）");
    check(a.size() == 3, "改动副本不影响原栈");
    check(a.top() == 3, "原栈的栈顶没被动过");
}

void test_copy_assignment_is_deep() {
    LinkedStack<std::string> a;
    a.push("x");
    a.push("y");
    LinkedStack<std::string> b;
    b.push("old1");
    b.push("old2");
    b = a;                       // b 原来那两个结点必须被释放掉
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.pop() == std::string("y"), "赋值后栈顶取自右边");
    check(a.size() == 2, "赋值不改动右边");
}

void test_self_assignment_is_safe() {
    LinkedStack<int> s;
    s.push(7);
    s.push(8);
    // 自赋值写成「先取引用别名再赋值」，而不是 `s = s;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = s;
    s = same;
    check(s.size() == 2, "自赋值后长度不变");
    check(s.pop() == 8, "自赋值后栈顶不变");
    check(s.pop() == 7, "自赋值后栈底不变");
}

// clear 之后栈必须还能用（不是「清空 = 报废」）。
void test_clear_then_reuse() {
    LinkedStack<int> s;
    for (int i = 0; i < 5; ++i) {
        s.push(i);
    }
    s.clear();
    check(s.empty(), "clear 之后为空");
    check(s.size() == 0, "clear 之后 size 归零");
    s.push(42);
    check(s.top() == 42, "clear 之后还能继续 push");
    check(s.size() == 1, "clear 之后 size 从 1 重新数起");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        LinkedStack<int> s;
        s.push(1);
        s.pop();
        s.pop();                 // 空栈出栈：原书在这里打印
        LinkedStack<int> copy = s;
        copy = s;
        s.clear();
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

// 深链的释放必须是迭代的。规模不是随手写的：本机（Linux/gcc 13.3/8MB 栈，
// Debug+ASan/UBSan 档）实测，把 clear() 换成递归释放后 40 万结点仍能过、
// 80 万结点 ASan 报 `stack-overflow ... in LinkedStack<int>::drop`；
// 而这里的迭代版在 200 万结点下依然无恙。所以门槛取 80 万——
// 变异：把 clear() 改成递归释放，这条必红。
void test_deep_chain_destructs_without_stack_overflow() {
    const int kDeep = 800000;
    {
        LinkedStack<int> s;
        for (int i = 0; i < kDeep; ++i) {
            s.push(i);
        }
        check(s.size() == static_cast<std::size_t>(kDeep), "八十万个结点建得起来");
    }   // 这里析构：迭代释放，不该崩
    check(true, "八十万结点的链析构完成，没有爆栈");
}

}  // namespace

int main() {
    test_lifo_order();
    test_empty_returns_nullopt();
    test_no_overflow();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_clear_then_reuse();
    test_no_console_output();
    test_deep_chain_destructs_without_stack_overflow();

    std::printf("LinkedStack(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
