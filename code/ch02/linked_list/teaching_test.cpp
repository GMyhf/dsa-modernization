// 教学版 LinkedList 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
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

void test_append_and_at() {
    LinkedList<int> list;
    check(list.empty(), "新建的链表是空的");
    list.append(10);
    list.append(20);
    list.append(30);
    check(list.size() == 3, "append 三次后 size==3");
    check(list.at(0) == 10, "位置 0 是最先追加的");
    check(list.at(2) == 30, "位置 2 是最后追加的");
    check(!list.empty(), "非空链表 empty 为假");
}

// 头结点的意义：表头插入不再是特例，和中间插入走同一套代码。
// 变异：去掉头结点、让 head_ 直接指向第一个元素 → 表头插入会走丢，这里会红。
void test_insert_at_head_is_not_special() {
    LinkedList<int> list;
    list.append(2);
    list.append(3);
    list.insert(0, 1);                 // 插在表头
    check(list.size() == 3, "表头插入后长度加一");
    check(list.at(0) == 1, "新元素在位置 0");
    check(list.at(1) == 2, "原来的第一个退到位置 1");
    check(list.at(2) == 3, "其余不变");
}

void test_insert_in_middle_and_tail() {
    LinkedList<int> list;
    list.append(1);
    list.append(3);
    list.insert(1, 2);                 // 插在中间
    check(list.at(1) == 2, "中间插入落在位置 1");
    list.insert(list.size(), 4);       // pos == size() 合法，等于追加
    check(list.size() == 4, "表尾插入后长度为 4");
    check(list.at(3) == 4, "表尾插入落在最后");
    list.append(5);                    // 追加必须仍然接在真正的尾巴后面
    check(list.at(4) == 5, "insert 到表尾之后，append 仍接在最后");
}

void test_remove_head_middle_tail() {
    LinkedList<int> list;
    for (int i = 0; i < 5; ++i) {
        list.append(i);
    }
    check(list.remove(0) == 0, "删表头返回被删的值");
    check(list.at(0) == 1, "删表头之后新表头是 1");
    check(list.remove(1) == 2, "删中间返回被删的值");
    check(list.at(1) == 3, "删中间之后链接接上了");
    check(list.remove(list.size() - 1) == 4, "删表尾返回被删的值");
    check(list.size() == 2, "删了三个之后剩 2 个");
    list.append(9);                    // 删表尾之后 tail_ 必须已经退回前驱
    check(list.at(list.size() - 1) == 9, "删过表尾之后 append 仍接得对");
}

// 删到空表之后，尾指针必须退回头结点，否则它指向已释放的结点。
// 变异：删掉 remove 里的 `if (dying == tail_) tail_ = predecessor;`
//       → 下一次 append 写进已释放的内存，ASan 报 heap-use-after-free。
void test_remove_until_empty_then_append() {
    LinkedList<int> list;
    list.append(1);
    check(list.remove(0) == 1, "删掉唯一的元素");
    check(list.empty(), "链表现在是空的");
    list.append(2);                    // 这里会踩到那根野指针
    check(list.size() == 1, "空掉之后重新 append，长度是 1");
    check(list.at(0) == 2, "取到的是新元素");
}

void test_find_returns_optional() {
    LinkedList<std::string> list;
    list.append("a");
    list.append("b");
    list.append("c");
    auto hit = list.find("b");
    check(hit.has_value(), "找得到时 optional 有值");
    check(hit.value() == 1, "找到的位置是 1");
    check(!list.find("zzz").has_value(), "找不到时返回 nullopt");
}

void test_out_of_range_throws() {
    LinkedList<int> list;
    list.append(1);

    bool thrown = false;
    try {
        (void)list.at(5);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "at 越界抛 out_of_range");

    thrown = false;
    try {
        list.insert(99, 7);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "insert 位置非法抛 out_of_range");

    thrown = false;
    try {
        LinkedList<int> empty_list;
        (void)empty_list.remove(0);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "空表上 remove 抛 out_of_range");
}

// 原书 lnkList 有析构却没有拷贝构造 → 两个链表共享同一串结点。
// 变异实测：删掉拷贝构造，`LinkedList<int> b = a;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    LinkedList<int> a;
    a.append(1);
    a.append(2);
    LinkedList<int> b = a;
    check(b.size() == 2, "副本长度一致");
    check(b.at(0) == 1 && b.at(1) == 2, "副本次序一致");
    a.append(3);
    check(b.size() == 2, "改动原表不影响副本");
    b.append(9);
    check(a.size() == 3, "改动副本不影响原表");
    check(a.at(2) == 3, "原表最后一个仍是自己 append 的那个");
}

void test_copy_assignment_is_deep() {
    LinkedList<std::string> a;
    a.append("x");
    a.append("y");
    LinkedList<std::string> b;
    b.append("old1");
    b.append("old2");
    b.append("old3");
    b = a;                             // b 原来那三个结点必须被释放掉
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.at(0) == std::string("x"), "赋值后内容取自右边");
    b.append("z");                     // 赋值后 tail_ 必须指向新链的尾巴
    check(b.at(2) == std::string("z"), "赋值之后 append 仍接得对");
    check(a.size() == 2, "赋值不改动右边");
}

void test_self_assignment_is_safe() {
    LinkedList<int> list;
    list.append(7);
    list.append(8);
    // 自赋值写成「先取引用别名再赋值」，而不是 `list = list;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = list;
    list = same;
    check(list.size() == 2, "自赋值后长度不变");
    check(list.at(0) == 7 && list.at(1) == 8, "自赋值后内容不变");
}

void test_range_for() {
    LinkedList<int> list;
    for (int i = 1; i <= 4; ++i) {
        list.append(i);
    }
    int sum = 0;
    int count = 0;
    for (int x : list) {
        sum += x;
        ++count;
    }
    check(count == 4, "range-for 走过 4 个元素（不多不少，不含头结点）");
    check(sum == 10, "range-for 取到的值是 1+2+3+4");
}

void test_clear_then_reuse() {
    LinkedList<int> list;
    for (int i = 0; i < 5; ++i) {
        list.append(i);
    }
    list.clear();
    check(list.empty(), "clear 之后为空");
    check(list.size() == 0, "clear 之后 size 归零");
    list.append(42);                   // clear 之后 tail_ 必须已退回头结点
    check(list.size() == 1, "clear 之后还能继续用");
    check(list.at(0) == 42, "clear 之后 append 的元素取得到");
}

// append 走尾指针，是 O(1)。这条测的是行为不是耗时——若实现每次从头走到尾，
// 二十万次 append 会是 O(n²)，闸门的 120 秒超时会把它拦下来。
void test_append_is_constant_time() {
    LinkedList<int> list;
    for (int i = 0; i < 200000; ++i) {
        list.append(i);
    }
    check(list.size() == 200000, "二十万次 append");
    check(list.at(0) == 0, "第一个仍是 0");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        LinkedList<int> list;
        list.append(1);
        list.insert(0, 0);
        (void)list.remove(0);
        (void)list.find(999);
        try {
            (void)list.at(99);         // 越界：原书在这里打印
        } catch (const std::out_of_range&) {
        }
        LinkedList<int> copy = list;
        copy = list;
        list.clear();
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_append_and_at();
    test_insert_at_head_is_not_special();
    test_insert_in_middle_and_tail();
    test_remove_head_middle_tail();
    test_remove_until_empty_then_append();
    test_find_returns_optional();
    test_out_of_range_throws();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_range_for();
    test_clear_then_reuse();
    test_append_is_constant_time();
    test_no_console_output();

    std::printf("LinkedList(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
