#include "modern.hpp"

#include <cstdio>

int main() {
    using dsa::index::BPlusTree;
    std::vector<std::pair<int, std::string>> rows;
    for (const int key : {10, 20, 30, 50, 70, 90}) {
        rows.emplace_back(key, "记录" + std::to_string(key));
    }
    // 11.2：批量装入，按页填满。
    BPlusTree tree = BPlusTree::bulk_load(3, rows, 2);
    std::printf("装入后   : %s\n", tree.to_string().c_str());

    // 11.4：插入 60，叶裂 → 根裂 → 树增高一层。
    tree.insert(60, "记录60");
    std::printf("插入 60  : %s（树高 %zu）\n", tree.to_string().c_str(), tree.height());
    tree.insert(65, "记录65");
    std::printf("插入 65  : %s（父结点没满，没裂到根）\n", tree.to_string().c_str());

    tree.reset_counters();
    // 先取结果再取计数：printf 的实参求值顺序没有保证。
    const auto found = tree.find(50);
    const std::size_t point_reads = tree.page_reads();
    std::printf("查找 50  : %s，读了 %zu 页\n", found->c_str(), point_reads);

    tree.reset_counters();
    const auto scan = tree.range(35, 80);
    const std::size_t scan_reads = tree.page_reads();
    std::printf("范围 35..80 :");
    for (const auto& row : scan) {
        std::printf(" %d", row.first);
    }
    std::printf("，读了 %zu 页\n", scan_reads);

    tree.erase(70);
    std::printf("删除 70  : %s\n", tree.to_string().c_str());
    return 0;
}
