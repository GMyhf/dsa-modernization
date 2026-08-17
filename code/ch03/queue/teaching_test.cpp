// 教学版队列的自带断言测试。零框架：断言失败就返回非零退出码。
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

// ---- 顺序队列（循环队列）-------------------------------------------------

void test_array_fifo_order() {
    ArrayQueue<int> q(4);
    check(q.empty(), "新建的队列是空的");
    check(q.enqueue(1), "入队 1 成功");
    check(q.enqueue(2), "入队 2 成功");
    check(q.enqueue(3), "入队 3 成功");
    check(q.size() == 3, "三次入队后 size==3");
    check(q.front() == 1, "队头是最先入队的");
    check(q.dequeue() == 1, "先进先出：先出 1");
    check(q.dequeue() == 2, "再出 2");
    check(q.dequeue() == 3, "最后出 3");
    check(q.empty(), "全部出队后为空");
}

void test_array_empty_returns_nullopt() {
    ArrayQueue<int> q(2);
    check(!q.dequeue().has_value(), "空队列 dequeue 返回 nullopt");
    check(!q.front().has_value(), "空队列 front 返回 nullopt");
    check(q.size() == 0, "空队列 size 为 0");
}

// 循环队列的看家本领：下标绕回数组开头之后照样正确。
// 变异：把 `(rear_ + 1) % slots_` 的取模去掉 → 越界，ASan 当场报。
void test_array_wraps_around() {
    ArrayQueue<int> q(3);
    check(q.enqueue(1) && q.enqueue(2) && q.enqueue(3), "先填满 3 个");
    check(q.dequeue() == 1, "出队 1，队头往后走一格");
    check(q.dequeue() == 2, "出队 2，队头再走一格");
    check(q.enqueue(4), "这两格空出来了，入队 4 会绕回数组开头");
    check(q.enqueue(5), "再入队 5");
    check(q.size() == 3, "绕回之后 size 仍然算得对");
    check(q.dequeue() == 3, "绕回之后出队次序仍是 3");
    check(q.dequeue() == 4, "然后是 4");
    check(q.dequeue() == 5, "然后是 5");
    check(q.empty(), "绕了一圈之后能正确判空");
}

// 牺牲一个槽位换「空 / 满」可分辨：容量 n 就真的只能装 n 个，装不下第 n+1 个。
// 变异：把满的判据写成 `rear_ == front_` → 满时被当成空，这里会红。
void test_array_full_is_distinguishable_from_empty() {
    ArrayQueue<int> q(3);
    check(!q.full(), "空队列不是满的");
    check(q.enqueue(1) && q.enqueue(2) && q.enqueue(3), "装满 3 个");
    check(q.full(), "装到容量就是满的");
    check(!q.empty(), "满队列不是空的——这正是牺牲一个槽位换来的");
    check(!q.enqueue(4), "满了之后入队返回 false，不是覆盖也不是越界");
    check(q.size() == 3, "入队失败不改变长度");
    check(q.dequeue() == 1, "出队一个之后");
    check(!q.full(), "就不再是满的了");
    check(q.enqueue(4), "腾出位置后能继续入队");
}

// 原书 arrQueue 有析构却没有拷贝构造 → 二次释放。
void test_array_copy_is_deep() {
    ArrayQueue<int> a(4);
    check(a.enqueue(1) && a.enqueue(2), "原队列装两个");
    ArrayQueue<int> b = a;
    check(b.size() == 2, "副本长度一致");
    check(b.dequeue() == 1, "副本内容一致");
    check(a.size() == 2, "改动副本不影响原队列");
    check(a.dequeue() == 1, "原队列内容没被动过");
}

void test_array_copy_assignment_is_deep() {
    ArrayQueue<std::string> a(4);
    check(a.enqueue("x") && a.enqueue("y"), "右边装两个");
    ArrayQueue<std::string> b(2);
    check(b.enqueue("old"), "左边先装一个");
    b = a;
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.dequeue() == std::string("x"), "赋值后内容取自右边");
    check(a.size() == 2, "赋值不改动右边");
}

void test_array_self_assignment_is_safe() {
    ArrayQueue<int> q(4);
    check(q.enqueue(7) && q.enqueue(8), "装两个");
    // 自赋值写成「先取引用别名再赋值」，而不是 `q = q;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = q;
    q = same;
    check(q.size() == 2, "自赋值后长度不变");
    check(q.dequeue() == 7, "自赋值后内容不变");
}

void test_array_clear_then_reuse() {
    ArrayQueue<int> q(3);
    check(q.enqueue(1) && q.enqueue(2), "先装两个");
    q.clear();
    check(q.empty(), "clear 之后为空");
    check(q.enqueue(9), "clear 之后还能继续用");
    check(q.dequeue() == 9, "clear 之后取到的是新元素");
}

// ---- 链式队列 -------------------------------------------------------------

void test_linked_fifo_order() {
    LinkedQueue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    check(q.size() == 3, "三次入队后 size==3");
    check(q.front() == 1, "队头是最先入队的");
    check(q.dequeue() == 1, "先进先出：先出 1");
    check(q.dequeue() == 2, "再出 2");
    check(q.dequeue() == 3, "最后出 3");
    check(q.empty(), "全部出队后为空");
}

void test_linked_empty_returns_nullopt() {
    LinkedQueue<int> q;
    check(q.empty(), "新建的链式队列是空的");
    check(!q.dequeue().has_value(), "空队列 dequeue 返回 nullopt");
    check(!q.front().has_value(), "空队列 front 返回 nullopt");
}

// 链式队列没有「队满」：入多少个都行。
void test_linked_has_no_capacity_limit() {
    LinkedQueue<int> q;
    for (int i = 0; i < 10000; ++i) {
        q.enqueue(i);
    }
    check(q.size() == 10000, "一万次入队一个都没丢");
    bool all_ok = true;
    for (int i = 0; i < 10000; ++i) {
        if (q.dequeue() != i) {
            all_ok = false;
        }
    }
    check(all_ok, "一万个元素按入队次序出来");
    check(q.empty(), "全部出队后为空");
}

// 队列空掉时队尾指针必须一起置空，否则它指向已释放的结点。
// 变异：删掉 dequeue 里的 `if (front_ == nullptr) rear_ = nullptr;`
//       → 下一次 enqueue 会写进已释放的结点，ASan 报 heap-use-after-free。
void test_linked_rear_is_reset_when_emptied() {
    LinkedQueue<int> q;
    q.enqueue(1);
    check(q.dequeue() == 1, "出队唯一的元素");
    check(q.empty(), "队列现在是空的");
    q.enqueue(2);                      // 这里会踩到那根野指针
    check(q.size() == 1, "空掉之后重新入队，长度是 1");
    check(q.dequeue() == 2, "取到的是新入队的元素");
    check(q.empty(), "又空了");
}

// 队尾指针的意义：入队是 O(1)，不是每次从头走到尾。
// 这条测的是行为不是耗时——反复「入队一个、出队一个」十万次，
// 若实现每次都从头遍历，闸门的 120 秒超时会把它拦下来。
void test_linked_enqueue_is_constant_time() {
    LinkedQueue<int> q;
    for (int i = 0; i < 200000; ++i) {
        q.enqueue(i);
    }
    check(q.size() == 200000, "二十万次入队");
    check(q.front() == 0, "队头仍是第一个入队的");
    q.clear();
    check(q.empty(), "clear 之后为空");
}

void test_linked_copy_is_deep() {
    LinkedQueue<int> a;
    a.enqueue(1);
    a.enqueue(2);
    a.enqueue(3);
    LinkedQueue<int> b = a;
    check(b.size() == 3, "副本长度一致");
    check(b.dequeue() == 1, "副本次序一致（不是被拷反了）");
    check(b.dequeue() == 2, "副本第二个也对");
    check(a.size() == 3, "改动副本不影响原队列");
    check(a.front() == 1, "原队列队头没被动过");
}

void test_linked_copy_assignment_is_deep() {
    LinkedQueue<std::string> a;
    a.enqueue("x");
    a.enqueue("y");
    LinkedQueue<std::string> b;
    b.enqueue("old1");
    b.enqueue("old2");
    b = a;                             // b 原来那两个结点必须被释放掉
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.dequeue() == std::string("x"), "赋值后内容取自右边");
    check(a.size() == 2, "赋值不改动右边");
}

void test_linked_self_assignment_is_safe() {
    LinkedQueue<int> q;
    q.enqueue(7);
    q.enqueue(8);
    // 自赋值写成「先取引用别名再赋值」，而不是 `q = q;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = q;
    q = same;
    check(q.size() == 2, "自赋值后长度不变");
    check(q.dequeue() == 7, "自赋值后队头不变");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        ArrayQueue<int> a(2);
        (void)a.enqueue(1);
        (void)a.enqueue(2);
        (void)a.enqueue(3);            // 队满：原书在这里打印
        (void)a.dequeue();
        (void)a.dequeue();
        (void)a.dequeue();             // 队空：原书也在这里打印
        ArrayQueue<int> ac = a;
        ac = a;

        LinkedQueue<int> l;
        l.enqueue(1);
        (void)l.dequeue();
        (void)l.dequeue();
        LinkedQueue<int> lc = l;
        lc = l;
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_array_fifo_order();
    test_array_empty_returns_nullopt();
    test_array_wraps_around();
    test_array_full_is_distinguishable_from_empty();
    test_array_copy_is_deep();
    test_array_copy_assignment_is_deep();
    test_array_self_assignment_is_safe();
    test_array_clear_then_reuse();

    test_linked_fifo_order();
    test_linked_empty_returns_nullopt();
    test_linked_has_no_capacity_limit();
    test_linked_rear_is_reset_when_emptied();
    test_linked_enqueue_is_constant_time();
    test_linked_copy_is_deep();
    test_linked_copy_assignment_is_deep();
    test_linked_self_assignment_is_safe();

    test_no_console_output();

    std::printf("Queue(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
