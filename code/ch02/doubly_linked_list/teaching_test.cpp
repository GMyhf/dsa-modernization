// 教学版 DoublyLinkedList 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
// 双链表的每一条链接都要有断言盯着——漏改一根 prev/next，表在正向遍历时
// 看起来完全正常，只有反向走或者删中间结点时才炸。所以这里两个方向都验。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
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

void test_push_back_order() {
    DoublyLinkedList<int> list;
    check(list.empty(), "新建的双链表是空的");
    list.push_back(1);
    list.push_back(2);
    list.push_back(3);
    check(list.size() == 3, "push_back 三次后 size==3");
    check(list.at(0) == 1 && list.at(1) == 2 && list.at(2) == 3, "次序是 1 2 3");
}

void test_push_front_order() {
    DoublyLinkedList<int> list;
    list.push_front(1);
    list.push_front(2);
    list.push_front(3);
    check(list.size() == 3, "push_front 三次后 size==3");
    check(list.at(0) == 3 && list.at(1) == 2 && list.at(2) == 1, "次序倒过来是 3 2 1");
}

// 空表上第一次插入：新结点同时是表头和表尾，两根指针都要设。
// 变异：push_back 里去掉 `else head_ = fresh;` → 表头是空指针，正向遍历直接崩。
void test_first_insert_sets_both_ends() {
    DoublyLinkedList<int> a;
    a.push_back(7);
    check(a.at(0) == 7, "空表 push_back 之后取得到");
    check(a.pop_front() == 7, "它同时也是表头");

    DoublyLinkedList<int> b;
    b.push_front(8);
    check(b.at(0) == 8, "空表 push_front 之后取得到");
    check(b.pop_back() == 8, "它同时也是表尾");
}

// 双向链接必须两根都对：正着走一遍、倒着走一遍，结果要互为逆序。
// 变异：insert 里漏掉 `successor->prev = fresh;` → 倒着走会跳过新结点，这里红。
void test_links_are_consistent_in_both_directions() {
    DoublyLinkedList<int> list;
    for (int i = 1; i <= 5; ++i) {
        list.push_back(i);
    }
    list.insert(2, 99);                   // 插到中间

    int forward[6];
    int n = 0;
    for (int x : list) {
        forward[n++] = x;
    }
    check(n == 6, "正向遍历走过 6 个结点");
    check(forward[2] == 99, "正向看，新结点在位置 2");

    // 倒着走：从最后一个结点开始，靠 prev 往回
    auto it = list.begin();
    for (int i = 0; i < 5; ++i) {
        ++it;
    }
    int backward[6];
    for (int i = 0; i < 6; ++i) {
        backward[i] = *it;
        if (i < 5) {
            --it;
        }
    }
    bool mirrored = true;
    for (int i = 0; i < 6; ++i) {
        if (backward[i] != forward[5 - i]) {
            mirrored = false;
        }
    }
    check(mirrored, "反向遍历与正向遍历互为逆序——两根链接都对");
}

void test_insert_at_both_ends_and_middle() {
    DoublyLinkedList<int> list;
    list.push_back(1);
    list.push_back(3);
    list.insert(1, 2);                    // 中间
    check(list.at(1) == 2, "中间插入落在位置 1");
    list.insert(0, 0);                    // 表头
    check(list.at(0) == 0, "表头插入落在位置 0");
    list.insert(list.size(), 4);          // 表尾
    check(list.at(4) == 4, "表尾插入落在最后");
    check(list.size() == 5, "一共 5 个元素");
    check(list.pop_back() == 4, "pop_back 拿到的正是刚插到表尾的那个");
    check(list.pop_front() == 0, "pop_front 拿到的正是刚插到表头的那个");
}

// 删中间结点是双链表的看家本领：prev 现成，不必循链找前驱。
// 变异：erase_node 里漏掉 `node->next->prev = node->prev;`
//       → 后继的 prev 指向已释放的结点，反向走时 ASan 报 heap-use-after-free。
void test_erase_middle_relinks_both_sides() {
    DoublyLinkedList<int> list;
    for (int i = 0; i < 5; ++i) {
        list.push_back(i);
    }
    check(list.erase(2) == 2, "erase 返回被删的值");
    check(list.size() == 4, "删除后长度减一");
    check(list.at(1) == 1 && list.at(2) == 3, "两侧链接接上了");

    // 反向走一遍，确认后继的 prev 也被改对了
    auto it = list.begin();
    for (int i = 0; i < 3; ++i) {
        ++it;
    }
    check(*it == 4, "正向走到最后一个是 4");
    --it;
    check(*it == 3, "往回一步是 3（被删的 2 已经不在链上）");
    --it;
    check(*it == 1, "再往回一步是 1");
}

void test_erase_head_and_tail() {
    DoublyLinkedList<int> list;
    for (int i = 0; i < 3; ++i) {
        list.push_back(i);
    }
    check(list.pop_front() == 0, "pop_front 拿到表头");
    check(list.at(0) == 1, "新表头是 1");
    check(list.pop_back() == 2, "pop_back 拿到表尾");
    check(list.size() == 1, "只剩一个");
    check(list.pop_back() == 1, "删掉最后一个");
    check(list.empty(), "表空了");
    list.push_back(9);                    // 空掉之后两根指针都必须已置空
    check(list.size() == 1 && list.at(0) == 9, "空掉之后还能继续用");
}

void test_empty_and_out_of_range_throw() {
    DoublyLinkedList<int> list;

    bool thrown = false;
    try {
        (void)list.pop_front();
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "空表 pop_front 抛 out_of_range");

    thrown = false;
    try {
        (void)list.pop_back();
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "空表 pop_back 抛 out_of_range");

    list.push_back(1);
    thrown = false;
    try {
        (void)list.at(9);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "at 越界抛 out_of_range");

    thrown = false;
    try {
        list.insert(9, 1);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "insert 位置非法抛 out_of_range");
}

// 变异实测：删掉拷贝构造，`DoublyLinkedList<int> b = a;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    DoublyLinkedList<int> a;
    a.push_back(1);
    a.push_back(2);
    DoublyLinkedList<int> b = a;
    check(b.size() == 2, "副本长度一致");
    check(b.at(0) == 1 && b.at(1) == 2, "副本次序一致");
    a.push_back(3);
    check(b.size() == 2, "改动原表不影响副本");
    b.push_back(9);
    check(a.size() == 3, "改动副本不影响原表");
}

void test_copy_assignment_is_deep() {
    DoublyLinkedList<std::string> a;
    a.push_back("x");
    a.push_back("y");
    DoublyLinkedList<std::string> b;
    b.push_back("old1");
    b.push_back("old2");
    b.push_back("old3");
    b = a;                                // b 原来那三个结点必须被释放掉
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.at(0) == std::string("x"), "赋值后内容取自右边");
    b.push_back("z");                     // 赋值后 tail_ 必须指向新链的尾巴
    check(b.at(2) == std::string("z"), "赋值之后 push_back 仍接得对");
    check(a.size() == 2, "赋值不改动右边");
}

void test_self_assignment_is_safe() {
    DoublyLinkedList<int> list;
    list.push_back(7);
    list.push_back(8);
    list = list;
    check(list.size() == 2, "自赋值后长度不变");
    check(list.at(0) == 7 && list.at(1) == 8, "自赋值后内容不变");
}

void test_clear_then_reuse() {
    DoublyLinkedList<int> list;
    for (int i = 0; i < 5; ++i) {
        list.push_back(i);
    }
    list.clear();
    check(list.empty(), "clear 之后为空");
    list.push_back(42);
    list.push_front(41);
    check(list.size() == 2, "clear 之后两头都还能接");
    check(list.at(0) == 41 && list.at(1) == 42, "clear 之后内容正确");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        DoublyLinkedList<int> list;
        list.push_back(1);
        list.push_front(0);
        list.insert(1, 9);
        (void)list.erase(1);
        (void)list.pop_front();
        (void)list.pop_back();
        try {
            (void)list.pop_back();       // 空表：原书在这里打印
        } catch (const std::out_of_range&) {
        }
        DoublyLinkedList<int> copy = list;
        copy = list;
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_push_back_order();
    test_push_front_order();
    test_first_insert_sets_both_ends();
    test_links_are_consistent_in_both_directions();
    test_insert_at_both_ends_and_middle();
    test_erase_middle_relinks_both_sides();
    test_erase_head_and_tail();
    test_empty_and_out_of_range_throw();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_clear_then_reuse();
    test_no_console_output();

    std::printf("DoublyLinkedList(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
