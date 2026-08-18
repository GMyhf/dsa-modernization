#include "modern.hpp"
#include "support/shared_cases.hpp"

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
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

using dsa::index::IndexKind;
using dsa::index::MultiLevelIndex;

std::vector<std::pair<int, std::string>> sorted_records(int count) {
    std::vector<std::pair<int, std::string>> out;
    for (int i = 0; i < count; ++i) {
        // 变长记录：正是「索引 + 位置」比「按下标算地址」更合适的场合。
        out.emplace_back(i * 10, std::string(static_cast<std::size_t>(i % 7) + 1, 'x'));
    }
    return out;
}

void test_sparse_index_needs_a_sorted_file() {
    MultiLevelIndex index(IndexKind::Sparse, 4, 4);
    bool threw = false;
    try {
        index.load({{5, "a"}, {3, "b"}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.1 稀疏索引要求主文件按 key 有序");

    // 稠密索引不要求主文件有序——这正是它的用处。
    MultiLevelIndex dense(IndexKind::Dense, 4, 4);
    dense.load({{5, "a"}, {3, "b"}, {9, "c"}});
    check(dense.find(3).has_value() && *dense.find(3) == "b", "11.1 稠密索引可以查无序主文件");
    check(dense.find(9).has_value() && *dense.find(9) == "c", "11.1 无序主文件的另一条");
    check(!dense.find(4).has_value(), "11.1 不存在的 key");
}

void test_one_entry_per_record_versus_per_page() {
    const auto records = sorted_records(40);
    MultiLevelIndex dense(IndexKind::Dense, 4, 8);
    MultiLevelIndex sparse(IndexKind::Sparse, 4, 8);
    dense.load(records);
    sparse.load(records);

    check(dense.entries() == 40, "11.1 稠密索引每条记录一项");
    check(sparse.entries() == 10, "11.1 稀疏索引每个数据页一项");
    check(sparse.index_pages() < dense.index_pages(), "11.1 稀疏索引更省页");
    check(dense.data_pages() == 10 && sparse.data_pages() == 10, "11.1 数据页数一样");
}

void test_page_reads_are_the_metric() {
    const auto records = sorted_records(40);
    MultiLevelIndex sparse(IndexKind::Sparse, 4, 16);
    sparse.load(records);
    check(sparse.levels() == 1, "11.1 10 个索引项一页放得下，只有顶层");

    sparse.reset_counters();
    check(sparse.find(250).has_value(), "11.1 稀疏索引查得到");
    check(sparse.page_reads() == 1, "11.1 顶层常驻内存，只读一个数据页");

    sparse.reset_counters();
    check(!sparse.find(255).has_value(), "11.1 稀疏索引查不到");
    check(sparse.page_reads() == 1, "11.1 稀疏索引查不到也得先读那一页");

    MultiLevelIndex dense(IndexKind::Dense, 4, 64);
    dense.load(records);
    dense.reset_counters();
    check(!dense.find(255).has_value(), "11.1 稠密索引查不到");
    check(dense.page_reads() == 0, "11.1 稠密索引查不到时一个数据页都不读");
    dense.reset_counters();
    check(dense.find(250).has_value(), "11.1 稠密索引查得到");
    check(dense.page_reads() == 1, "11.1 稠密索引命中时读一个数据页");
}

void test_multi_level_grows_and_costs_one_read_per_level() {
    const auto records = sorted_records(1000);
    // 每个索引页 4 项：250 个底层项 → 63 → 16 → 4，第 4 层正好装满一页，收敛。
    MultiLevelIndex index(IndexKind::Sparse, 4, 4);
    index.load(records);
    check(index.entries() == 250, "11.1 250 个数据页对应 250 个索引项");
    check(index.levels() == 4, "11.1 索引逐层收敛到一页");

    index.reset_counters();
    check(index.find(5000).has_value(), "11.1 多级索引查得到");
    // 顶层不算，下面 4 层各读一页，最后读一个数据页。
    check(index.page_reads() == index.levels(), "11.1 一次查询 = 层数-1 个索引页 + 1 个数据页");

    // 索引页放得多，层数就少，访外次数随之下降。这就是 11.2 做多分树的理由。
    MultiLevelIndex flat(IndexKind::Sparse, 4, 64);
    flat.load(records);
    check(flat.levels() == 2, "11.1 每页 64 项时只要两层");
    flat.reset_counters();
    (void)flat.find(5000);
    check(flat.page_reads() == 2 && flat.page_reads() < index.page_reads(),
          "11.1 页装得多，层数少，访外次数随之下降");
}

void test_every_record_is_reachable() {
    const auto records = sorted_records(500);
    for (const IndexKind kind : {IndexKind::Dense, IndexKind::Sparse}) {
        MultiLevelIndex index(kind, 6, 5);
        index.load(records);
        bool all_found = true;
        bool values_match = true;
        for (const auto& record : records) {
            const auto found = index.find(record.first);
            all_found = all_found && found.has_value();
            values_match = values_match && found.has_value() && *found == record.second;
        }
        check(all_found, "11.1 每条记录都查得到");
        check(values_match, "11.1 取回的记录内容正确");
        check(!index.find(-1).has_value(), "11.1 小于最小 key");
        check(!index.find(999999).has_value(), "11.1 大于最大 key");
        check(!index.find(5).has_value(), "11.1 落在两条记录之间");
    }
}

void test_edge_cases() {
    MultiLevelIndex empty(IndexKind::Sparse, 4, 4);
    empty.load({});
    check(empty.levels() == 0 && empty.entries() == 0, "11.1 空文件没有索引");
    check(!empty.find(1).has_value(), "11.1 空文件查不到");
    check(empty.data_pages() == 0, "11.1 空文件没有数据页");

    MultiLevelIndex single(IndexKind::Sparse, 4, 4);
    single.load({{7, "only"}});
    check(single.levels() == 1 && *single.find(7) == "only", "11.1 只有一条记录");

    bool threw = false;
    try {
        MultiLevelIndex bad(IndexKind::Sparse, 4, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.1 索引页放不下两项就收敛不了，直接拒绝");

    threw = false;
    try {
        MultiLevelIndex dup(IndexKind::Dense, 4, 4);
        dup.load({{1, "a"}, {1, "b"}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.1 重复 key 被拒绝");
}
}  // namespace

int main() {
    test_sparse_index_needs_a_sorted_file();
    test_one_entry_per_record_versus_per_page();
    test_page_reads_are_the_metric();
    test_multi_level_grows_and_costs_one_read_per_level();
    test_every_record_is_reachable();
    test_edge_cases();
    const auto shared = dsa::shared_cases::load();
    for (const auto& item : shared) {
        const bool sparse = item.operation == "sparse";
        dsa::index::MultiLevelIndex index(sparse ? dsa::index::IndexKind::Sparse : dsa::index::IndexKind::Dense, 2, 2);
        std::vector<std::pair<int, std::string>> records;
        const auto split = item.input.find('|');
        std::istringstream rows(item.input.substr(0, split)); std::string row;
        while (std::getline(rows, row, ',')) { const auto colon = row.find(':'); records.push_back({std::stoi(row.substr(0, colon)), row.substr(colon + 1)}); }
        if (item.expected_error.empty()) { index.load(records); check(index.find(std::stoi(item.input.substr(split + 1))) == item.expected, "T-047 linear index"); }
        else { bool raised = false; try { index.load(records); } catch (const std::invalid_argument&) { raised = true; } check(raised, "T-047 linear exception"); }
    }
    std::printf("共享用例: %zu\n", shared.size());
    std::printf("LinearIndex: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
