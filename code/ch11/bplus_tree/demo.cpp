#include "modern.hpp"

#include <cstdio>

namespace {
using dsa::index::BPlusTree;

std::vector<BPlusTree::Row> page(std::initializer_list<int> keys) {
    std::vector<BPlusTree::Row> out;
    for (const int key : keys) {
        out.emplace_back(key, "记录" + std::to_string(key));
    }
    return out;
}

/// 原书图 11.9：叶 e～l 自左向右，上面两层各结点的孩子数自底向上给出。
BPlusTree fig_11_9() {
    return BPlusTree::assemble(3,
                               {page({5, 10, 20}), page({25, 30}), page({35, 40}),
                                page({45, 48, 50}), page({55, 60}), page({65, 70}),
                                page({75, 80}), page({85, 90})},
                               {{3, 3, 2}, {3}});
}
}  // namespace

int main() {
    BPlusTree tree = fig_11_9();
    std::printf("图 11.9  : %s\n", tree.to_string().c_str());

    // 先取结果再取计数：printf 的实参求值顺序没有保证。
    tree.reset_counters();
    const auto found = tree.find(48);
    const std::size_t point_reads = tree.page_reads();
    std::printf("查找 48  : %s，读了 %zu 页\n", found->c_str(), point_reads);

    tree.reset_counters();
    const auto scan = tree.range(33, 62);
    const std::size_t scan_reads = tree.page_reads();
    std::printf("范围 33..62 :");
    for (const auto& row : scan) {
        std::printf(" %d", row.first);
    }
    std::printf("，读了 %zu 页\n", scan_reads);

    // 图 11.10：插入 15，叶 e → 内部结点 b → 根 a 相继分裂，树增高一层。
    tree.reset_counters();
    tree.insert(15, "记录15");
    std::printf("插入 15  : %s（读 %zu 写 %zu）\n", tree.to_string().c_str(), tree.page_reads(),
                tree.page_writes());

    // 图 11.11：回到图 11.9 删除 75，叶合并、内部结点借位。
    BPlusTree other = fig_11_9();
    other.reset_counters();
    other.erase(75);
    std::printf("删除 75  : %s（读 %zu 写 %zu）\n", other.to_string().c_str(), other.page_reads(),
                other.page_writes());
    return 0;
}
