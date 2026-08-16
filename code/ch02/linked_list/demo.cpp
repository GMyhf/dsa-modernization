// 第 2 章「先跑一遍」：用教学版 LinkedList 走一遍 append / insert / remove。
// 编译运行：
//   g++ -std=c++17 -I code/ch02/linked_list code/ch02/linked_list/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    LinkedList<int> values;
    values.append(10);
    values.append(30);
    values.insert(1, 20);

    std::cout << "链表:";
    for (int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << "\n删除位置 0 得到 " << values.remove(0) << "，剩余:";
    for (int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << "\nappend 之后尾元素是 " << values.at(values.size() - 1) << '\n';
}
