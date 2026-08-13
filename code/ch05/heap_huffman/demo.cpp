#include "modern.hpp"

#include <iostream>

int main() {
    dsa::MinHeap<int> heap;
    for (int value : {5, 1, 4, 2}) {
        heap.insert(value);
    }
    std::cout << "依次取出最小元:";
    while (auto value = heap.remove_min()) {
        std::cout << ' ' << *value;
    }
    std::cout << '\n';

    const int weights[] = {2, 3, 4, 7};
    const dsa::HuffmanTree tree(weights, 4);
    std::cout << "权 2,3,4,7 的 Huffman 树根权 = " << tree.total_weight() << '\n';
}
