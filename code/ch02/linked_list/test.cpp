#include "modern.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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

void test_node_types() {
    dsa::SinglyLink<int> one(1);
    dsa::SinglyLink<int> two(2, &one);
    check(two.next == &one && two.data == 2, "代码2.6 单链结点保存数据和后继");

    dsa::DoublyLink<int> left(1);
    dsa::DoublyLink<int> right(2, &left);
    left.next = &right;
    check(right.prev == &left && left.next == &right, "代码2.12 双链结点维护前驱和后继");
}

void test_insert_and_tail() {
    dsa::LinkedList<std::string> list;
    list.append("b");
    list.insert(0, "a");
    list.insert(2, "d");
    list.insert(2, "c");
    check(list.size() == 4, "四次插入后长度正确");
    check(list.at(0) == "a" && list.at(1) == "b" && list.at(2) == "c" && list.at(3) == "d",
          "头部、中间、尾部插入均保持顺序");
    list.append("e");
    check(list.at(4) == "e", "尾指针在尾插后仍正确");

    bool threw = false;
    try { list.insert(99, "x"); } catch (const std::out_of_range&) { threw = true; }
    check(threw && list.size() == 5, "非法插入抛 out_of_range 且不改变链表");
}

void test_remove_boundaries_and_tail_repair() {
    dsa::LinkedList<int> list;
    list.append(1);
    list.append(2);
    list.append(3);
    check(list.remove(0) == 1, "删除首结点");
    check(list.remove(1) == 3, "删除尾结点");
    list.append(4);
    list.append(5);
    check(list.size() == 3 && list.at(0) == 2 && list.at(1) == 4 && list.at(2) == 5,
          "删尾后连续 append 仍接在真尾部");
    check(list.remove(2) == 5 && list.remove(1) == 4 && list.remove(0) == 2 && list.empty(), "删到空表");
    list.append(9);
    check(list.size() == 1 && list.at(0) == 9, "空表后的 append 修复尾指针");

    bool threw = false;
    try { (void)list.remove(1); } catch (const std::out_of_range&) { threw = true; }
    check(threw, "非法删除抛 out_of_range");
}

void test_access_find_and_iteration() {
    dsa::LinkedList<int> list;
    for (int value : {5, 7, 5, 9}) {
        list.append(value);
    }
    check(list.find(5) == 0 && list.find(9) == 3 && !list.find(88).has_value(),
          "循链查找返回第一次位置或 nullopt");
    list.at(1) = 8;
    check(list.at(1) == 8, "at 非 const 重载可修改元素");
    const dsa::LinkedList<int>& const_list = list;
    int sum = 0;
    for (int value : const_list) {
        sum += value;
    }
    check(sum == 27, "const 链表可独立迭代，不在容器中保存游标");

    bool threw = false;
    try { (void)const_list.at(4); } catch (const std::out_of_range&) { threw = true; }
    check(threw, "访问越界抛 out_of_range");
}

void test_rule_of_five() {
    dsa::LinkedList<int> source;
    source.append(1);
    source.append(2);
    dsa::LinkedList<int> copy = source;
    copy.append(3);
    check(source.size() == 2 && copy.size() == 3, "深拷贝不共享结点");
    dsa::LinkedList<int> assigned;
    assigned = source;
    dsa::LinkedList<int>& copy_alias = assigned;
    assigned = copy_alias;
    check(assigned.size() == 2 && assigned.at(1) == 2, "拷贝赋值和自赋值安全");

    dsa::LinkedList<int> moved = std::move(copy);
    check(moved.size() == 3 && copy.empty(), "移动构造转移结点所有权");
    assigned = std::move(moved);
    check(assigned.size() == 3 && moved.empty(), "移动赋值释放旧链并转移所有权");

    dsa::LinkedList<int> empty;
    swap(assigned, empty);
    check(assigned.empty() && empty.size() == 3 && empty.at(2) == 3,
          "与空链表交换后两侧头尾哨兵均正确");
    assigned.append(8);
    empty.append(4);
    check(assigned.at(0) == 8 && empty.at(3) == 4, "交换后两侧仍可尾插");
}

struct CopyConstructionFailure {
    int value{0};
    inline static bool throw_on_copy = false;

    CopyConstructionFailure() = default;
    explicit CopyConstructionFailure(int v) : value(v) {}
    CopyConstructionFailure(const CopyConstructionFailure& other) : value(other.value) {
        if (throw_on_copy) {
            throw std::runtime_error("injected copy construction failure");
        }
    }
    CopyConstructionFailure(CopyConstructionFailure&&) noexcept = default;
    CopyConstructionFailure& operator=(const CopyConstructionFailure&) = default;
    CopyConstructionFailure& operator=(CopyConstructionFailure&&) noexcept = default;
};

void test_insert_strong_exception_guarantee() {
    dsa::LinkedList<CopyConstructionFailure> list;
    list.append(CopyConstructionFailure(1));
    list.append(CopyConstructionFailure(2));
    CopyConstructionFailure value(99);
    CopyConstructionFailure::throw_on_copy = true;
    bool threw = false;
    try { list.insert(1, value); } catch (const std::runtime_error&) { threw = true; }
    CopyConstructionFailure::throw_on_copy = false;
    check(threw, "新结点构造异常如实抛出");
    check(list.size() == 2 && list.at(0).value == 1 && list.at(1).value == 2,
          "构造失败前未接链，链表保持完整");
}

struct Lifetime {
    int value{0};
    inline static int live = 0;
    inline static int copies = 0;
    inline static int throw_at = 0;

    Lifetime() { ++live; }
    explicit Lifetime(int v) : value(v) { ++live; }
    Lifetime(const Lifetime& other) : value(other.value) {
        if (throw_at != 0 && ++copies == throw_at) {
            throw std::runtime_error("injected copy failure");
        }
        ++live;
    }
    Lifetime(Lifetime&& other) noexcept : value(other.value) { ++live; }
    Lifetime& operator=(const Lifetime&) = default;
    Lifetime& operator=(Lifetime&&) noexcept = default;
    ~Lifetime() { --live; }

    static void reset(int at = 0) {
        copies = 0;
        throw_at = at;
    }
};

void test_copy_constructor_cleans_partial_chain() {
    Lifetime::reset();
    {
        dsa::LinkedList<Lifetime> source;
        source.append(Lifetime(1));
        source.append(Lifetime(2));
        source.append(Lifetime(3));
        const int before = Lifetime::live;
        Lifetime::reset(3);
        bool threw = false;
        try { dsa::LinkedList<Lifetime> copy(source); } catch (const std::runtime_error&) { threw = true; }
        Lifetime::reset();
        check(threw, "复制构造中途异常如实抛出");
        check(Lifetime::live == before, "复制构造失败时已接入结点全部回收");
    }
    check(Lifetime::live == 0, "链表离开作用域后不遗留元素对象");
}

void test_move_only_and_clear() {
    dsa::LinkedList<std::unique_ptr<int>> list;
    list.append(std::make_unique<int>(1));
    list.insert(0, std::make_unique<int>(0));
    check(list.size() == 2 && *list.at(0) == 0 && *list.at(1) == 1, "move-only 元素可插入链表");
    auto removed = list.remove(0);
    check(removed != nullptr && *removed == 0 && *list.at(0) == 1, "move-only 元素可删除并取回");
    list.clear();
    check(list.empty(), "clear 释放所有动态结点并成为空表");
}

}  // namespace

int main() {
    test_node_types();
    test_insert_and_tail();
    test_remove_boundaries_and_tail_repair();
    test_access_find_and_iteration();
    test_rule_of_five();
    test_insert_strong_exception_guarantee();
    test_copy_constructor_cleans_partial_chain();
    test_move_only_and_clear();
    std::printf("LinkedList: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
