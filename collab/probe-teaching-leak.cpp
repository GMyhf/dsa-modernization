#include "ch03/array_stack/teaching.hpp"
#include "ch03/linked_stack/teaching.hpp"
#include "ch05/binary_tree/teaching.hpp"
#include <cstdio>
#include <stdexcept>

// 第 N 次拷贝赋值 / 拷贝构造抛异常
struct Throwing {
    int v = 0;
    static inline int budget = 1000000;
    Throwing() = default;
    Throwing(int x) : v(x) {}
    Throwing(const Throwing& o) : v(o.v) { if (--budget < 0) throw std::runtime_error("copy ctor"); }
    Throwing& operator=(const Throwing& o) { if (--budget < 0) throw std::runtime_error("copy assign"); v = o.v; return *this; }
};

int main(int argc, char**) {
    (void)argc;
    std::printf("--- 1. ArrayStack::grow 搬迁中途抛 ---\n");
    try {
        ArrayStack<Throwing> s(2);
        Throwing::budget = 1000000;
        s.push(Throwing(1)); s.push(Throwing(2));
        Throwing::budget = 1;          // 扩容搬第 2 个元素时抛
        s.push(Throwing(3));
    } catch (const std::exception& e) { std::printf("    抛出: %s\n", e.what()); }

    std::printf("--- 2. LinkedStack 拷贝构造中途抛 ---\n");
    try {
        LinkedStack<Throwing> a;
        Throwing::budget = 1000000;
        for (int i = 0; i < 5; ++i) a.push(Throwing(i));
        Throwing::budget = 2;          // 拷到第 3 个结点时抛
        LinkedStack<Throwing> b = a;
        (void)b;
    } catch (const std::exception& e) { std::printf("    抛出: %s\n", e.what()); }

    std::printf("--- 3. BinaryTree::clone 中途抛 ---\n");
    try {
        BinaryTree<Throwing> l, r, t;
        Throwing::budget = 1000000;
        l.create_leaf(Throwing(1)); r.create_leaf(Throwing(2));
        t.create_tree(Throwing(0), l, r);
        Throwing::budget = 2;          // 克隆到第 3 个结点时抛
        BinaryTree<Throwing> copy = t;
        (void)copy;
    } catch (const std::exception& e) { std::printf("    抛出: %s\n", e.what()); }
    Throwing::budget = 1000000;
    return 0;
}
