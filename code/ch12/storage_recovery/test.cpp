#include "modern.hpp"

#include <cstdio>
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

using dsa::advanced::MarkSweepHeap;
using Node = MarkSweepHeap::Node;

void test_unreachable_objects_are_reclaimed() {
    MarkSweepHeap heap;
    Node* root = heap.allocate(1);
    Node* kept = heap.allocate(2);
    Node* dropped = heap.allocate(3);
    MarkSweepHeap::link(root, kept);
    check(heap.live() == 3, "12.2.4 分配了三个对象");
    (void)dropped;  // 谁也不指着它

    const std::size_t reclaimed = heap.collect({root});
    check(reclaimed == 1 && heap.live() == 2, "12.2.4 从根走不到的对象被回收");
    check(root->value == 1 && kept->value == 2, "12.2.4 可达对象原封不动");
    check(heap.collect({root}) == 0, "12.2.4 再收一次没有可回收的——标记已复位");
}

/// 本节的正题：**引用计数收不回环，标记–清除收得回**。
void test_unrooted_cycle_is_reclaimed() {
    MarkSweepHeap heap;
    Node* root = heap.allocate(1);

    // 两个对象互相指着：引用计数各为 1，永远掉不到零。
    Node* a = heap.allocate(10);
    Node* b = heap.allocate(11);
    MarkSweepHeap::link(a, b);
    MarkSweepHeap::link(b, a);
    check(heap.live() == 3, "12.2.4 环上的两个对象都在堆里");

    check(heap.collect({root}) == 2, "12.2.4 无根环整个被回收——引用计数做不到这件事");
    check(heap.live() == 1, "12.2.4 只剩根");

    // 有根的环必须留下：标记时先标记再入栈，否则这里会转不出来。
    Node* x = heap.allocate(20);
    Node* y = heap.allocate(21);
    MarkSweepHeap::link(x, y);
    MarkSweepHeap::link(y, x);
    MarkSweepHeap::link(root, x);
    check(heap.collect({root}) == 0, "12.2.4 从根能走到的环全部保留");
    check(heap.live() == 3, "12.2.4 根加环上两个对象");
}

/// 可达性是**从根走出来的**，与「有多少人指着我」无关。
void test_reachability_not_reference_count() {
    MarkSweepHeap heap;
    Node* root = heap.allocate(0);

    // 这个对象被三个对象指着，但那三个都够不着根——一起回收。
    Node* popular = heap.allocate(99);
    for (int i = 0; i < 3; ++i) {
        Node* fan = heap.allocate(i);
        MarkSweepHeap::link(fan, popular);
    }
    check(heap.live() == 5, "12.2.4 一个被三处引用的对象");
    check(heap.collect({root}) == 4, "12.2.4 引用计数为 3，照样回收——它够不着根");
    check(heap.live() == 1, "12.2.4 只剩根");
}

void test_multiple_roots_and_shared_subgraph() {
    MarkSweepHeap heap;
    Node* first = heap.allocate(1);
    Node* second = heap.allocate(2);
    Node* shared = heap.allocate(3);
    Node* only_second = heap.allocate(4);
    Node* orphan = heap.allocate(5);
    MarkSweepHeap::link(first, shared);
    MarkSweepHeap::link(second, shared);
    MarkSweepHeap::link(second, only_second);
    (void)orphan;

    // 两个根：共享子图从任一根都能到，只能算一次、不能删。
    check(heap.collect({first, second}) == 1, "12.2.4 两个根，只回收那个孤儿");
    check(heap.live() == 4, "12.2.4 共享子图保留");

    // 去掉一个根：只被它指着的那部分成了垃圾，共享的那个仍然活着。
    check(heap.collect({first}) == 2, "12.2.4 少一个根，它独占的部分成为垃圾");
    check(heap.live() == 2 && first->value == 1 && shared->value == 3,
          "12.2.4 共享子图仍从剩下的根可达");

    check(heap.collect({}) == 2, "12.2.4 根集为空则全部回收");
    check(heap.live() == 0, "12.2.4 堆清空");
    check(heap.collect({}) == 0, "12.2.4 空堆再收一次是 0");
}

/// 对象图可以很深。标记用显式栈，不能靠调用栈——
/// GC 恰恰在内存吃紧时跑，那时最不该再去吃栈。
void test_deep_and_wide_graphs() {
    MarkSweepHeap heap;
    constexpr int depth = 200000;
    Node* root = heap.allocate(0);
    Node* tail = root;
    for (int i = 1; i < depth; ++i) {
        Node* next = heap.allocate(i);
        MarkSweepHeap::link(tail, next);
        tail = next;
    }
    check(heap.live() == depth, "12.2.4 20 万个对象的深链");
    check(heap.collect({root}) == 0, "12.2.4 深链全部可达，一个都不回收");

    // 把根丢掉，整条链都是垃圾。
    check(heap.collect({}) == depth, "12.2.4 根一撤，20 万个对象全部回收");
    check(heap.live() == 0, "12.2.4 深链回收干净");

    // 宽图：一个根指向很多对象。
    Node* hub = heap.allocate(-1);
    for (int i = 0; i < 50000; ++i) {
        MarkSweepHeap::link(hub, heap.allocate(i));
    }
    check(heap.live() == 50001, "12.2.4 一个根指向 5 万个对象");
    check(heap.collect({hub}) == 0, "12.2.4 宽图全部可达");
}
}  // namespace

int main() {
    test_unreachable_objects_are_reclaimed();
    test_unrooted_cycle_is_reclaimed();
    test_reachability_not_reference_count();
    test_multiple_roots_and_shared_subgraph();
    test_deep_and_wide_graphs();
    std::printf("StorageRecovery: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
