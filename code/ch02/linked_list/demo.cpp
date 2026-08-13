#include "modern.hpp"

#include <iostream>

int main() {
    dsa::LinkedList<int> values;
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
