#include "modern.hpp"

#include <cstdio>
#include <utility>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

using dsa::ownership::IterativeChain;
using dsa::ownership::RecursiveChain;

/// 小规模下两种写法完全等价——这正是它的危险之处：写完跑一遍，看不出任何问题。
void test_both_designs_agree_on_small_input() {
    RecursiveChain recursive;
    IterativeChain iterative;
    for (int i = 0; i < 1000; ++i) {
        recursive.push_front(i);
        iterative.push_front(i);
    }
    check(recursive.size() == 1000 && iterative.size() == 1000, "所有权 两种写法长度一致");
    check(recursive.to_vector() == iterative.to_vector(), "所有权 两种写法内容一致");
    check(iterative.to_vector().front() == 999, "所有权 头插顺序");
}

/// 迭代释放：链有多长都不压栈。50 万个结点在 -O0 也过得去。
///
/// 这里**不测** RecursiveChain 的同等规模——它会段错误，而段错误没法写成断言。
/// 崩溃阈值、复现命令和真实输出在 legacy.md，那才是这一节的证据。
void test_iterative_release_survives_a_long_chain() {
    IterativeChain chain;
    for (int i = 0; i < 500000; ++i) {
        chain.push_front(i);
    }
    check(chain.size() == 500000, "所有权 50 万结点建链");
    chain.clear();  // 走循环，栈深度恒定
    check(chain.size() == 0 && chain.to_vector().empty(), "所有权 50 万结点迭代释放");

    // 析构路径同样要走一遍：作用域结束时不能崩。
    {
        IterativeChain scoped;
        for (int i = 0; i < 500000; ++i) {
            scoped.push_front(i);
        }
        check(scoped.size() == 500000, "所有权 作用域内的长链");
    }
    check(true, "所有权 长链析构没有压穿栈");
}

/// 手写所有权就得手写五法则——这是代价那一侧，如实测出来。
void test_value_semantics_are_hand_written() {
    IterativeChain original;
    for (int i = 0; i < 5; ++i) {
        original.push_front(i);
    }

    IterativeChain copy = original;
    check(copy.to_vector() == original.to_vector(), "所有权 拷贝构造是深拷贝");
    copy.push_front(99);
    check(copy.size() == 6 && original.size() == 5, "所有权 拷贝之后互不影响");

    IterativeChain assigned;
    assigned = original;
    check(assigned.to_vector() == original.to_vector(), "所有权 拷贝赋值");

    IterativeChain moved = std::move(copy);
    check(moved.size() == 6, "所有权 移动构造");
    check(copy.size() == 0 && copy.to_vector().empty(), "所有权 被移动方留在空状态");

    IterativeChain target;
    target.push_front(7);
    target = std::move(moved);
    check(target.size() == 6, "所有权 移动赋值会先释放原有的链");

    IterativeChain& alias = target;
    target = alias;
    check(target.size() == 6, "所有权 自赋值不炸");

    IterativeChain empty_copy = IterativeChain();
    check(empty_copy.size() == 0, "所有权 空链可拷贝");
}
}  // namespace

int main() {
    test_both_designs_agree_on_small_input();
    test_iterative_release_survives_a_long_chain();
    test_value_semantics_are_hand_written();
    std::printf("Ownership: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
