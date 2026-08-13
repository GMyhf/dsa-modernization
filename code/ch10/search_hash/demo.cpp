#include "modern.hpp"

#include <iostream>

int main() {
    dsa::search::HashTable table(5);
    if (!table.insert(1) || !table.insert(6) || !table.insert(11)) {
        std::cout << "插入 1、6、11 失败\n";
        return 1;
    }

    std::cout << "插入 1、6、11 后:\n";
    for (std::size_t index = 0; index < table.capacity(); ++index) {
        const auto slot = table.slot_at(index);
        std::cout << "  槽 " << index << ": ";
        if (slot.state == dsa::search::HashTable::SlotState::used) {
            std::cout << slot.key << '\n';
        } else if (slot.state == dsa::search::HashTable::SlotState::tombstone) {
            std::cout << "墓碑\n";
        } else {
            std::cout << "空\n";
        }
    }

    if (!table.erase(1)) {
        std::cout << "删除 1 失败\n";
        return 1;
    }
    std::cout << "删除 1 后仍能找到 6? " << (table.contains(6) ? "是" : "否") << '\n';
    if (!table.insert(16)) {
        std::cout << "插入 16 失败\n";
        return 1;
    }
    std::cout << "插入 16 后槽 1 是 "
              << table.slot_at(1).key
              << "（复用了墓碑）\n";
}
