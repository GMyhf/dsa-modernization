#include "modern.hpp"

#include <cstdio>
#include <list>
#include <random>
#include <stdexcept>
#include <string>
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

using dsa::DoublyLinkedList;

std::vector<int> forward_of(const DoublyLinkedList<int>& list) {
    std::vector<int> out;
    for (const int value : list) {
        out.push_back(value);
    }
    return out;
}

/// 靠 `prev` 链从尾往回走。**只有这个方向能验出 prev 接错**——
/// 正向遍历只用 next，prev 全错也照样通过。
std::vector<int> backward_of(DoublyLinkedList<int>& list) {
    std::vector<int> out;
    if (list.empty()) {
        return out;
    }
    auto it = list.begin();
    for (std::size_t i = 1; i < list.size(); ++i) {
        ++it;
    }
    for (std::size_t i = 0; i < list.size(); ++i) {
        out.push_back(*it);
        if (i + 1 < list.size()) {
            --it;
        }
    }
    return out;
}

DoublyLinkedList<int> make(const std::vector<int>& values) {
    DoublyLinkedList<int> list;
    for (const int value : values) {
        list.push_back(value);
    }
    return list;
}

void test_both_ends() {
    DoublyLinkedList<int> list;
    check(list.empty() && list.size() == 0, "2.3.2 空表");

    list.push_back(2);
    list.push_front(1);
    list.push_back(3);
    check(forward_of(list) == std::vector<int>({1, 2, 3}), "2.3.2 双端插入");
    check(list.size() == 3 && list.at(0) == 1 && list.at(2) == 3, "2.3.2 按位置读取");

    check(list.pop_front() == 1, "2.3.2 头删返回被删的值");
    check(list.pop_back() == 3, "2.3.2 尾删返回被删的值");
    check(forward_of(list) == std::vector<int>({2}), "2.3.2 两端各删一个之后");

    // 删到空再插入：head_/tail_ 都要归位，漏一个下次插入就会挂在野指针上。
    check(list.pop_back() == 2 && list.empty(), "2.3.2 删空");
    list.push_back(7);
    list.push_front(6);
    check(forward_of(list) == std::vector<int>({6, 7}), "2.3.2 删空之后还能继续用");

    bool threw = false;
    try {
        DoublyLinkedList<int> empty;
        (void)empty.pop_front();
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "2.3.2 空表出队抛 out_of_range");
}

void test_prev_links_are_wired() {
    DoublyLinkedList<int> list = make({1, 2, 3, 4, 5});
    check(backward_of(list) == std::vector<int>({5, 4, 3, 2, 1}), "2.3.2 反向遍历是正向的逆序");

    list.insert(0, 0);
    list.insert(6, 6);
    list.insert(3, 99);
    check(forward_of(list) == std::vector<int>({0, 1, 2, 99, 3, 4, 5, 6}), "2.3.2 头/中/尾插入");
    check(backward_of(list) == std::vector<int>({6, 5, 4, 3, 99, 2, 1, 0}),
          "2.3.2 中间插入之后 prev 链仍然正确");

    (void)list.erase(3);
    check(forward_of(list) == std::vector<int>({0, 1, 2, 3, 4, 5, 6}), "2.3.2 按位置删除");
    check(backward_of(list) == std::vector<int>({6, 5, 4, 3, 2, 1, 0}),
          "2.3.2 中间删除之后 prev 链仍然正确");

    for (const std::size_t bad : {std::size_t{9}, std::size_t{100}}) {
        bool threw = false;
        try {
            list.insert(bad, 0);
        } catch (const std::out_of_range&) {
            threw = true;
        }
        check(threw, "2.3.2 插入位置越界抛 out_of_range");
    }
    bool threw = false;
    try {
        (void)list.erase(std::size_t{99});
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "2.3.2 删除位置越界抛 out_of_range");
}

/// 双链表相对单链表的**唯一**实质好处：已知结点位置时删除是 O(1)，
/// 不必先循链找前驱。多存一个 `prev` 指针就是为了这个。
void test_erase_at_a_known_position_is_o1() {
    DoublyLinkedList<int> list = make({10, 20, 30, 40, 50});

    auto it = list.begin();
    ++it;
    ++it;  // 指向 30
    check(*it == 30, "2.3.2 迭代器定位");
    check(list.erase(it) == 30, "2.3.2 已知结点直接删除，不用找前驱");
    check(forward_of(list) == std::vector<int>({10, 20, 40, 50}), "2.3.2 删除后的正向序列");
    check(backward_of(list) == std::vector<int>({50, 40, 20, 10}), "2.3.2 删除后的反向序列");

    auto at_40 = list.begin();
    ++at_40;
    ++at_40;
    const auto inserted = list.insert(at_40, 35);
    check(*inserted == 35, "2.3.2 插入返回指向新结点的迭代器");
    check(forward_of(list) == std::vector<int>({10, 20, 35, 40, 50}), "2.3.2 已知位置插入");

    // 删头和删尾也走同一条路径。
    check(list.erase(list.begin()) == 10, "2.3.2 用迭代器删头");
    check(forward_of(list) == std::vector<int>({20, 35, 40, 50}), "2.3.2 删头之后");
}

void test_value_semantics() {
    DoublyLinkedList<int> original = make({1, 2, 3});

    DoublyLinkedList<int> copy = original;
    copy.at(0) = 99;
    check(forward_of(original) == std::vector<int>({1, 2, 3}), "2.3.2 拷贝是深拷贝，原表不受影响");
    check(forward_of(copy) == std::vector<int>({99, 2, 3}), "2.3.2 拷贝可独立修改");
    check(backward_of(copy) == std::vector<int>({3, 2, 99}), "2.3.2 拷贝出来的 prev 链也正确");

    DoublyLinkedList<int> assigned;
    assigned.push_back(1000);
    assigned = original;
    check(forward_of(assigned) == std::vector<int>({1, 2, 3}), "2.3.2 拷贝赋值会丢掉原有内容");

    DoublyLinkedList<int>& alias = assigned;
    assigned = alias;
    check(forward_of(assigned) == std::vector<int>({1, 2, 3}), "2.3.2 自赋值不炸");

    DoublyLinkedList<int> moved = std::move(copy);
    check(forward_of(moved) == std::vector<int>({99, 2, 3}), "2.3.2 移动构造");
    check(copy.empty() && copy.size() == 0, "2.3.2 被移动方留在空状态");

    DoublyLinkedList<int> target = make({7, 8});
    target = std::move(moved);
    check(forward_of(target) == std::vector<int>({99, 2, 3}), "2.3.2 移动赋值会释放原有结点");

    DoublyLinkedList<int> a = make({1, 2});
    DoublyLinkedList<int> b = make({8, 9, 10});
    a.swap(b);
    check(forward_of(a) == std::vector<int>({8, 9, 10}) && forward_of(b) == std::vector<int>({1, 2}),
          "2.3.2 交换两张表");
    check(backward_of(a) == std::vector<int>({10, 9, 8}), "2.3.2 交换之后 prev 链仍正确");

    a.clear();
    check(a.empty() && forward_of(a).empty(), "2.3.2 清空");
    a.push_back(5);
    check(forward_of(a) == std::vector<int>({5}), "2.3.2 清空之后还能继续用");
}

void test_non_int_element() {
    DoublyLinkedList<std::string> words;
    words.push_back("b");
    words.push_front("a");
    words.push_back("c");
    check(words.size() == 3 && words.at(0) == "a" && words.at(2) == "c", "2.3.2 非平凡元素类型");
    check(words.pop_back() == "c", "2.3.2 非平凡元素的删除返回值");
    const DoublyLinkedList<std::string> copy = words;
    check(copy.at(0) == "a" && copy.size() == 2, "2.3.2 非平凡元素的深拷贝");
}

void test_random_operations_match_std_list() {
    // 固定种子：失败可复现。与 std::list 对拍，正反两个方向都比。
    std::mt19937 rng(999);
    std::uniform_int_distribution<int> what(0, 4);
    std::uniform_int_distribution<int> value(0, 999);
    int mismatched = 0;
    for (int round = 0; round < 200; ++round) {
        DoublyLinkedList<int> list;
        std::list<int> mirror;
        for (int step = 0; step < 120; ++step) {
            const int v = value(rng);
            switch (what(rng)) {
                case 0: list.push_back(v); mirror.push_back(v); break;
                case 1: list.push_front(v); mirror.push_front(v); break;
                case 2: if (!mirror.empty()) { (void)list.pop_back(); mirror.pop_back(); } break;
                case 3: if (!mirror.empty()) { (void)list.pop_front(); mirror.pop_front(); } break;
                default: {
                    const std::size_t pos =
                        mirror.empty() ? 0 : static_cast<std::size_t>(value(rng)) % (mirror.size() + 1);
                    list.insert(pos, v);
                    auto it = mirror.begin();
                    std::advance(it, static_cast<long>(pos));
                    mirror.insert(it, v);
                } break;
            }
        }
        const std::vector<int> forward = forward_of(list);
        const std::vector<int> expected(mirror.begin(), mirror.end());
        const std::vector<int> reversed(expected.rbegin(), expected.rend());
        if (list.size() != mirror.size() || forward != expected || backward_of(list) != reversed) {
            ++mismatched;
        }
    }
    check(mismatched == 0, "2.3.2 200 轮随机操作与 std::list 一致（正向与反向都比）");
}
}  // namespace

int main() {
    test_both_ends();
    test_prev_links_are_wired();
    test_erase_at_a_known_position_is_o1();
    test_value_semantics();
    test_non_int_element();
    test_random_operations_match_std_list();
    std::printf("DoublyLinkedList: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
