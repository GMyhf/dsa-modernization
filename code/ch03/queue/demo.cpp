// 第 3 章「先跑一遍」：用教学版 ArrayQueue 走一遍 enqueue / dequeue，
// 顺便看看「牺牲一个槽位」的效果——逻辑容量 3 就真的只装得下 3 个。
// 编译运行：
//   g++ -std=c++17 -I code/ch03/queue code/ch03/queue/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    ArrayQueue<int> queue(3);
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
