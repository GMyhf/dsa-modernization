#include "modern.hpp"

#include <iostream>

int main() {
    dsa::ArrayQueue<int> queue(3);
    if (!queue.enqueue(1) || !queue.enqueue(2) || !queue.enqueue(3)) {
        std::cout << "入队失败\n";
        return 1;
    }
    std::cout << "逻辑容量 3 时再入队? " << (queue.enqueue(4) ? "成功" : "已满") << '\n';
    std::cout << "依次出队:";
    while (auto value = queue.dequeue()) {
        std::cout << ' ' << *value;
    }
    std::cout << '\n';
}
