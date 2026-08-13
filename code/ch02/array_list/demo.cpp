#include "modern.hpp"

#include <iostream>

int main() {
    dsa::ArrayList<int> values;
    values.append(10);
    values.append(30);
    values.insert(1, 20);
    std::cout << "顺序表:";
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::cout << ' ' << values.at(index);
    }
    std::cout << "\n查找 20 的下标: " << *values.find(20) << '\n';
    std::cout << "删除位置 1 得到 " << values.remove(1) << "，剩余:";
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::cout << ' ' << values.at(index);
    }
    std::cout << '\n';
}
