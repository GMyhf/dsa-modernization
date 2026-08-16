// 教学版检索/集合/散列的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

// ---- 检索 -----------------------------------------------------------------

void test_sequential_search() {
    std::vector<int> values{5, 3, 9, 1};
    check(sequential_search(values, 5) == 0u, "第一个就是要找的，返回 0");
    check(sequential_search(values, 1) == 3u, "最后一个，返回 3");
    check(!sequential_search(values, 42).has_value(), "找不到返回 nullopt");

    std::vector<int> empty_values;
    check(!sequential_search(empty_values, 1).has_value(), "空表上找不到，也不崩");
}

// 二分检索的半开区间写法：`last = middle`，永远不写 `middle - 1`。
// 变异：改成闭区间 [first, last] 且 `last = middle - 1` → middle==0 时下标下溢，
//       ASan/UBSan 当场报越界。下面「查比最小值还小的键」那一条专为触发它而写。
void test_binary_search() {
    std::vector<int> sorted{1, 3, 5, 7, 9, 11};
    check(binary_search(sorted, 1) == 0u, "找最小的 1");
    check(binary_search(sorted, 11) == 5u, "找最大的 11");
    check(binary_search(sorted, 7) == 3u, "找中间的 7");
    check(!binary_search(sorted, 0).has_value(), "比最小值还小：找不到，且不下溢");
    check(!binary_search(sorted, 12).has_value(), "比最大值还大：找不到");
    check(!binary_search(sorted, 4).has_value(), "落在两个元素之间：找不到");

    std::vector<int> empty_values;
    check(!binary_search(empty_values, 1).has_value(), "空表上二分，不崩");

    std::vector<int> single{7};
    check(binary_search(single, 7) == 0u, "单元素表找得到");
    check(!binary_search(single, 6).has_value(), "单元素表找不到别的");
}

// 二分与顺序在同一份有序数据上必须给出同样的答案。
void test_binary_and_sequential_agree() {
    std::vector<int> sorted;
    for (int i = 0; i < 200; i += 2) {
        sorted.push_back(i);            // 0 2 4 ... 198
    }
    bool agree = true;
    for (int key = -1; key <= 200; ++key) {
        if (binary_search(sorted, key) != sequential_search(sorted, key)) {
            agree = false;
        }
    }
    check(agree, "202 个查询上二分与顺序检索逐个一致");
}

// ---- 集合 -----------------------------------------------------------------

void test_int_set_basics() {
    IntSet s;
    check(s.size() == 0, "新集合是空的");
    check(s.insert(1), "插入 1 成功");
    check(s.insert(2), "插入 2 成功");
    check(!s.insert(1), "重复元素返回 false，集合里仍然只有一个 1");
    check(s.size() == 2, "两个不同元素");
    check(s.contains(1) && s.contains(2), "两个都在");
    check(!s.contains(3), "3 不在");
    check(s.erase(1), "删掉 1");
    check(!s.contains(1), "1 确实没了");
    check(s.size() == 1, "删除后只剩 1 个");
    check(!s.erase(99), "删不存在的元素返回 false");
}

void test_int_set_operations() {
    IntSet a;
    for (int x : {1, 2, 3, 4}) {
        (void)a.insert(x);
    }
    IntSet b;
    for (int x : {3, 4, 5}) {
        (void)b.insert(x);
    }

    IntSet both = a.intersection(b);
    check(both.size() == 2, "交集有 2 个元素");
    check(both.contains(3) && both.contains(4), "交集是 {3,4}");
    check(!both.contains(1) && !both.contains(5), "交集不含单边独有的元素");

    IntSet sub;
    (void)sub.insert(2);
    (void)sub.insert(3);
    check(a.includes(sub), "{1,2,3,4} 包含 {2,3}");
    check(!a.includes(b), "{1,2,3,4} 不包含 {3,4,5}");
    check(a.includes(IntSet{}), "任何集合都包含空集");
}

// ---- 散列函数 --------------------------------------------------------------

// 逐字节读的是 unsigned char。变异：改成 char → 非 ASCII 字节变成负数，
// 散列值随之改变，下面「同一个串两次结果相同」仍能过，但这一条会红。
void test_elf_hash() {
    check(elf_hash("hello") == elf_hash("hello"), "同一个串两次散列结果相同");
    check(elf_hash("hello") != elf_hash("world"), "不同的串散列到不同的值");
    check(elf_hash("") == 0, "空串散列到 0");

    // 非 ASCII 字节（UTF-8 的「中」= E4 B8 AD）：按 unsigned char 读得到 0xF02D。
    // 若把它当有符号 char 读，负值会被符号扩展成 0xFFFFFF... 一路带进后续运算，
    // 本机实测结果是 0xFFFFFF000FFF00DD——两者天差地别，所以这一条能分辨两种写法。
    check(elf_hash("\xE4\xB8\xAD") == 0xF02Du, "非 ASCII 字节按无符号处理");
}

// ---- 闭散列表 --------------------------------------------------------------

void test_hash_table_basics() {
    HashTable table(7);
    check(table.capacity() == 7, "容量是 7");
    check(table.size() == 0, "新表是空的");
    check(table.insert(3), "插入 3");
    check(table.insert(10), "插入 10（与 3 同基地址，触发线性探测）");
    check(table.size() == 2, "两个元素");
    check(table.contains(3) && table.contains(10), "两个都找得到");
    check(!table.contains(4), "没插过的找不到");
    check(!table.insert(3), "重复键返回 false");
    check(table.size() == 2, "重复插入不改变长度");
}

void test_hash_table_rejects_zero_capacity() {
    bool thrown = false;
    try {
        HashTable table(0);
        (void)table;
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "容量 0 抛 invalid_argument（否则 %0 是除零）");
}

// 线性探测：同基地址的键依次往后落。
void test_hash_table_linear_probing() {
    HashTable table(7);
    check(table.insert(3), "3 落在基地址 3");
    check(table.insert(10), "10 的基地址也是 3，被挤到 4");
    check(table.insert(17), "17 的基地址还是 3，被挤到 5");
    check(table.slot_at(3).key == 3 && table.slot_at(3).state == HashTable::SlotState::used,
          "槽 3 是 3");
    check(table.slot_at(4).key == 10, "槽 4 是 10");
    check(table.slot_at(5).key == 17, "槽 5 是 17");
    check(table.slot_at(6).state == HashTable::SlotState::empty, "槽 6 还空着");
}

// **这一条是本节的核心**：删除必须标墓碑而不是标空。
// 变异：erase 里把状态改成 SlotState::empty → 探测链被截断，
//       删掉 3 之后 10 就查不到了，这里立刻红。
void test_erase_leaves_a_tombstone_not_a_hole() {
    HashTable table(7);
    check(table.insert(3), "3 落在槽 3");
    check(table.insert(10), "10 被挤到槽 4");

    check(table.erase(3), "删掉 3");
    check(!table.contains(3), "3 确实没了");
    check(table.slot_at(3).state == HashTable::SlotState::tombstone,
          "槽 3 是墓碑，不是空——探测链不能在这里断掉");
    check(table.contains(10), "**10 仍然查得到**（查找路过墓碑要继续走）");
    check(table.size() == 1, "长度减一");
}

// 插入要能回收墓碑，否则表用久了探测链只会越来越长。
// 变异：insertion_slot 不记录 first_tombstone、直接返回第一个 empty
//       → 新键落到槽 4 之后，这里会红。
void test_insert_reuses_a_tombstone() {
    HashTable table(7);
    check(table.insert(3), "3 落在槽 3");
    check(table.insert(10), "10 被挤到槽 4");
    check(table.erase(3), "删掉 3，槽 3 成为墓碑");

    check(table.insert(24), "插入 24（基地址也是 3）");
    check(table.slot_at(3).key == 24 && table.slot_at(3).state == HashTable::SlotState::used,
          "24 回收了槽 3 的墓碑，而不是跑到后面去");
    check(table.slot_at(4).key == 10, "10 没有被动过");
    check(table.contains(10) && table.contains(24), "两个键都在");
}

// 回收墓碑时**不能提前停**：必须先确认这个键没在后面出现过。
// 变异：insertion_slot 一碰到墓碑就返回 → 同一个键会被插两遍，这里红。
void test_tombstone_reuse_does_not_duplicate_keys() {
    HashTable table(7);
    check(table.insert(3), "3 落在槽 3");
    check(table.insert(10), "10 被挤到槽 4");
    check(table.erase(3), "删掉 3，槽 3 成为墓碑");

    check(!table.insert(10), "再插 10 必须返回 false——它就在槽 4，不能因为前面有墓碑就再插一遍");
    check(table.size() == 1, "长度仍是 1");
}

void test_hash_table_full() {
    HashTable table(3);
    check(table.insert(0) && table.insert(1) && table.insert(2), "装满 3 格");
    check(table.size() == 3, "满了");
    check(!table.insert(9), "表满时插入返回 false，不是死循环也不是越界");
    check(table.erase(1), "删掉一个");
    check(table.insert(9), "腾出墓碑后能插进去");
}

void test_negative_keys() {
    HashTable table(7);
    check(table.insert(-3), "负数键也能插");
    check(table.contains(-3), "负数键找得到");
    check(!table.contains(3), "-3 和 3 是不同的键");
    check(table.erase(-3), "负数键能删");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        HashTable table(5);
        (void)table.insert(1);
        (void)table.insert(1);
        (void)table.erase(99);
        (void)table.contains(99);
        try {
            (void)table.slot_at(99);
        } catch (const std::out_of_range&) {
        }
        IntSet s;
        (void)s.insert(1);
        (void)s.insert(1);
        (void)s.erase(99);
        std::vector<int> v{1, 2, 3};
        (void)binary_search(v, 99);
        (void)elf_hash("x");
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "没有往 stdout 打任何东西");
    check(err.str().empty(), "没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_sequential_search();
    test_binary_search();
    test_binary_and_sequential_agree();
    test_int_set_basics();
    test_int_set_operations();
    test_elf_hash();
    test_hash_table_basics();
    test_hash_table_rejects_zero_capacity();
    test_hash_table_linear_probing();
    test_erase_leaves_a_tombstone_not_a_hole();
    test_insert_reuses_a_tombstone();
    test_tombstone_reuse_does_not_duplicate_keys();
    test_hash_table_full();
    test_negative_keys();
    test_no_console_output();

    std::printf("SearchHash(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
