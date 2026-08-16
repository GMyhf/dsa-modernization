// 第 5 章「先跑一遍」：用教学版 MinHeap 与 HuffmanTree。
// 编译运行：
//   g++ -std=c++17 -I code/ch05/heap_huffman code/ch05/heap_huffman/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    MinHeap<int> heap;
    for (int value : {5, 1, 4, 2}) {
        heap.insert(value);
    }
    std::cout << "依次取出最小元:";
    while (auto value = heap.remove_min()) {   // 空堆返回 nullopt，循环自然结束
        std::cout << ' ' << *value;
    }
    std::cout << '\n';

    const int weights[] = {2, 3, 4, 7};
    const HuffmanTree tree(weights, 4);
    std::cout << "权 2,3,4,7 的 Huffman 树根权 = " << tree.total_weight() << '\n';
    std::cout << "带权路径长度 WPL = " << tree.weighted_path_length() << '\n';
}
