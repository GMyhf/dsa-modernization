// 教学版 ArrayStack 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
// 教学版比工程版少了移动语义与强异常保证，那两块由 test.cpp 守；
// 这里守的是教学版自己承诺的东西——LIFO、翻倍扩容、深拷贝、空栈返回空 optional、
// 以及「容器里一个字都不许往控制台打」。
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

// 原书缺陷 6/7：pop 用 `bool pop(T&)` 出参 + cout 提示；这里是 optional。
void test_lifo_order() {
    ArrayStack<int> s(2);
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

// 空栈是可预期状态，不是错误：返回空 optional，不抛异常、不打印。
void test_empty_returns_nullopt() {
    ArrayStack<int> s;
    check(!s.pop().has_value(), "空栈 pop 返回 nullopt");
    check(!s.top().has_value(), "空栈 top 返回 nullopt");
    check(s.size() == 0, "空栈 size 为 0");
    check(s.empty(), "空栈 empty 为真");
}

// 算法3.3 的翻倍扩容：内容必须一个不少、次序一个不乱。
// 变异：把 grow() 里的 delete[] 挪到拷贝循环之前 → ASan heap-use-after-free。
void test_growth_preserves_contents() {
    ArrayStack<int> s(1);
    for (int i = 0; i < 100; ++i) {
        s.push(i);
    }
    check(s.size() == 100, "连续 push 100 次后 size==100");
    check(s.capacity() >= 100, "容量已经涨到至少 100");
    bool all_ok = true;
    for (int i = 99; i >= 0; --i) {
        if (s.pop() != i) {
            all_ok = false;
        }
    }
    check(all_ok, "扩容后 100 个元素逐个原样弹出");
    check(s.empty(), "弹完之后为空");
}

// 摊还 O(1) 的前提是「翻倍」而不是「每次加一」。
// 变异：把 capacity_ * 2 改成 capacity_ + 1 → 这条会红。
void test_growth_is_doubling() {
    ArrayStack<int> s(4);
    for (int i = 0; i < 5; ++i) {
        s.push(i);
    }
    check(s.capacity() == 8, "容量从 4 翻倍到 8，不是加一");
    for (int i = 5; i < 9; ++i) {
        s.push(i);
    }
    check(s.capacity() == 16, "第二次翻倍到 16");
}

// 原书缺陷 4：有析构函数却没有拷贝构造 → `arrStack<int> b = a;` 二次释放。
// 变异实测：删掉拷贝构造，本行 `ArrayStack<int> b = a;` 在 -Werror 下先撞
// `-Wdeprecated-copy`（用户声明了析构就不该再依赖隐式拷贝），编译即红；
// 加 -Wno-deprecated-copy 放行后，ASan 报 `attempting double-free`，
// 回溯正好指到 teaching.hpp 的析构函数。两道防线，两次都红。
void test_copy_is_deep() {
    ArrayStack<int> a(2);
    a.push(1);
    a.push(2);
    ArrayStack<int> b = a;          // 拷贝构造
    check(b.size() == 2, "副本长度一致");
    check(b.pop() == 2, "副本内容一致");
    a.push(3);
    check(b.size() == 1, "改动原栈不影响副本");
    check(a.size() == 3, "改动副本不影响原栈");
}

// 变异实测：删掉拷贝赋值，`b = a;` 同样先在 -Werror 下被 -Wdeprecated-copy 拦下；
// 放行后是二次释放。
void test_copy_assignment_is_deep() {
    ArrayStack<std::string> a;
    a.push("x");
    a.push("y");
    ArrayStack<std::string> b;
    b.push("zzz");
    b = a;                          // 拷贝赋值：b 原来的内容要被释放掉
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.pop() == std::string("y"), "赋值后内容取自右边");
    check(a.size() == 2, "赋值不改动右边");
}

// 自赋值：`s = s` 必须什么都不坏。
// 变异：删掉 `if (this == &other) return *this;` 这一句本身不会红，
// 但把拷贝赋值改成「先 delete[] 再拷贝」就会——那正是这一条要挡的写法。
void test_self_assignment_is_safe() {
    ArrayStack<int> s;
    s.push(7);
    s.push(8);
    s = s;
    check(s.size() == 2, "自赋值后长度不变");
    check(s.pop() == 8, "自赋值后栈顶不变");
    check(s.pop() == 7, "自赋值后栈底不变");
}

// clear 只归零长度，容量留着复用——与原书 clear() 语义一致。
void test_clear_keeps_capacity() {
    ArrayStack<int> s(4);
    for (int i = 0; i < 10; ++i) {
        s.push(i);
    }
    std::size_t before = s.capacity();
    s.clear();
    check(s.empty(), "clear 之后为空");
    check(s.capacity() == before, "clear 不释放已分配的容量");
    s.push(42);
    check(s.top() == 42, "clear 之后还能继续用");
}

// D-001 第 3 条红线：数据结构内部零 I/O。原书在空栈出栈时直接 cout 打中文提示。
// 变异：在 pop() 的空栈分支里加一句 std::cout → 这条会红。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        ArrayStack<int> s(1);
        s.push(1);
        s.push(2);          // 触发扩容
        s.pop();
        s.pop();
        s.pop();            // 空栈出栈：原书在这里打印
        ArrayStack<int> copy = s;
        copy = s;
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

// 默认构造出来的栈就能用（原书无参构造只设 top=-1，mSize 与 st 都没初始化）。
void test_default_constructed_is_usable() {
    ArrayStack<int> s;
    check(s.empty(), "默认构造是空栈");
    check(s.capacity() > 0, "默认构造已经有容量");
    s.push(1);
    check(s.top() == 1, "默认构造的栈可以直接 push");
}

}  // namespace

int main() {
    test_lifo_order();
    test_empty_returns_nullopt();
    test_growth_preserves_contents();
    test_growth_is_doubling();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_clear_keeps_capacity();
    test_no_console_output();
    test_default_constructed_is_usable();

    std::printf("ArrayStack(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
