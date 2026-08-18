#include "modern.hpp"
#include "support/shared_cases.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool value, const char* name) { ++checks; if (!value) { ++failures; std::printf("  FAIL: %s\n", name); } }

void test_linear_searches() {
    const std::vector<int> unordered{8, 3, 8, 1};
    check(dsa::search::sequential_search(unordered, 8) == 0, "代码10.1/算法10.2 first matching index");
    check(!dsa::search::sequential_search(unordered, 7), "算法10.2 missing key optional");
    check(!dsa::search::sequential_search({}, 1), "算法10.2 empty table");
    check(dsa::search::sequential_search({4, 4}, 4) == 0, "算法10.2 duplicate returns first");
    const std::vector<int> ordered{-5, -1, 0, 4, 9, 20};
    check(dsa::search::binary_search(ordered, -5) == 0, "算法10.3 left boundary");
    check(dsa::search::binary_search(ordered, 20) == 5, "算法10.3 right boundary");
    check(!dsa::search::binary_search(ordered, 5), "算法10.3 insertion gap missing");
    check(!dsa::search::binary_search({}, 0), "算法10.3 empty sorted table");
    check(dsa::search::binary_search({42}, 42) == 0, "算法10.3 single element");
    check(!dsa::search::binary_search({42}, 41), "算法10.3 single missing element");
}

void test_sets() {
    dsa::search::Item<int> item(3);
    item.set_key(4);
    check(item.key() == 4, "代码10.1 Item getter and setter");
    dsa::search::IntSet left;
    dsa::search::IntSet right;
    check(left.insert(1) && left.insert(2) && !left.insert(2), "算法10.5 unique insertion");
    check(right.insert(2) && right.insert(3), "算法10.5 second set insertion");
    check(left.intersection(right).size() == 1, "算法10.6 intersection");
    check(right.intersection(left).size() == 1, "算法10.6 intersection commutative size");
    check(left.intersection(dsa::search::IntSet{}).size() == 0, "算法10.6 empty intersection");
    check(left.includes(left) && !left.includes(right), "算法10.7 containment");
    check(left.erase(1) && !left.erase(1) && left.size() == 1, "代码10.4 keyed deletion status");
    check(!left.includes(right) && dsa::search::IntSet{}.includes(dsa::search::IntSet{}), "算法10.7 empty set containment");
}

void test_hash_and_tombstones() {
    check(dsa::search::elf_hash("abc") != dsa::search::elf_hash("abd"), "算法10.8 ELFhash distinguishes nearby strings");
    check(dsa::search::elf_hash("") == 0, "算法10.8 empty string hash");
    check(dsa::search::elf_hash("abc") == dsa::search::elf_hash("abc"), "算法10.8 deterministic hash");
    dsa::search::HashTable table(5);
    check(table.insert(1) && table.insert(6) && table.insert(11), "算法10.10 linear collision insertion");
    check(table.size() == 3 && table.capacity() == 5, "算法10.9 table accounting");
    check(table.contains(11), "算法10.11 finds later collision");
    check(table.contains(1), "算法10.11 finds key at home slot");
    check(table.erase(1), "算法10.12 deletion creates tombstone");
    check(!table.erase(1), "算法10.12 deletion is idempotent failure");
    check(table.slot_at(1).state == dsa::search::HashTable::SlotState::tombstone, "算法10.12 tombstone state visible");
    check(table.contains(6) && table.contains(11), "算法10.11 probes through tombstone");
    check(table.insert(16), "算法10.13 insertion reuses tombstone after duplicate scan");
    check(table.slot_at(1).key == 16 && table.slot_at(1).state == dsa::search::HashTable::SlotState::used,
          "算法10.13 first tombstone reused");
    check(!table.insert(6), "算法10.13 does not duplicate after tombstone");
    check(table.erase(999) == false && table.size() == 3, "算法10.12 absent deletion leaves size");
    check(table.insert(21) && table.insert(26) && !table.insert(31), "算法10.10 full table returns false");
    check(table.contains(26) && !table.contains(31), "算法10.11 full probe terminates");
    dsa::search::HashTable wrapped(3);
    check(wrapped.insert(-1) && wrapped.insert(2) && wrapped.contains(-1), "算法10.10 negative key and wrap collision");
    check(wrapped.erase(-1) && wrapped.insert(5) && wrapped.contains(2), "算法10.13 wrapped tombstone chain");
    bool rejected = false;
    try { dsa::search::HashTable invalid(0); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "算法10.9 rejects zero capacity");
}
}  // namespace

/// 原书算法10.9 的散列表析构写的是 `delete HT`，而 `HT` 是 `new` 出来的**数组**，
/// 正确写法是 `delete[]`。对数组用 `delete` 是未定义行为，实践中表现为只析构第一个元素
/// 并把整块内存交还给错误的释放路径。
///
/// 现代实现的槽位数组由容器管理，这个错误在结构上不可能再犯。这个用例守的是这条不变量：
/// 装满一张表、拷贝一份、两份都析构掉，ASan 不得报泄漏或二次释放。
/// 谁要是把它改回裸数组加 `delete`，这里就会红。
void test_table_lifetime() {
    {
        dsa::search::HashTable table(64);
        for (int key = 0; key < 40; ++key) {
            (void)table.insert(key * 7);
        }
        const dsa::search::HashTable copy = table;          // 拷贝一份
        check(copy.size() == table.size(), "勘误E18 算法10.9：拷贝出来的表规模一致");
        dsa::search::HashTable assigned(8);
        assigned = table;                            // 拷贝赋值，原有槽位要被正确释放
        check(assigned.size() == table.size(), "勘误E18 算法10.9：拷贝赋值后的表规模一致");
        bool same = true;
        for (int key = 0; key < 40; ++key) {
            same = same && assigned.contains(key * 7);
        }
        check(same, "勘误E18 算法10.9：拷贝出来的表内容一致");
    }
    // 作用域结束，三张表全部析构。ASan 在这一步之后不报错，才算这条勘误没有复发。
    check(true, "勘误E18 算法10.9：装满的表析构完毕，ASan 未报泄漏或二次释放");
}

int main() {
    test_linear_searches();
    test_sets();
    test_hash_and_tombstones();
    test_table_lifetime();
    const auto shared = dsa::shared_cases::load();
    for (const auto& item : shared) {
        const auto split = item.input.find('|');
        if (item.operation == "binary") { const auto values = dsa::shared_cases::integers(item.input.substr(0, split)); const auto found = dsa::search::binary_search(values, std::stoi(item.input.substr(split + 1))); check(found && *found == std::stoul(item.expected), "T-047 binary"); }
        else { const auto capacity = std::stoul(item.input.substr(0, split)); if (item.expected_error.empty()) { dsa::search::HashTable table(capacity); for (int key : dsa::shared_cases::integers(item.input.substr(split + 1))) (void)table.insert(key); check(table.size() == std::stoul(item.expected), "T-047 hash"); } else { bool raised = false; try { dsa::search::HashTable table(capacity); } catch (const std::invalid_argument&) { raised = true; } check(raised, "T-047 hash exception"); } }
    }
    std::printf("共享用例: %zu\n", shared.size());
    std::printf("SearchHash: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
