// 第 2 章「先跑一遍」：用教学版 ArrayList 走一遍 append / insert / find / remove。
// 编译运行：
//   g++ -std=c++17 -I code/ch02/array_list code/ch02/array_list/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    ArrayList<int> values;
    values.append(10);
    values.append(30);
    values.insert(1, 20);

    std::cout << "顺序表:";
    for (int value : values) {   // 有 begin()/end()，range-for 直接可用
        std::cout << ' ' << value;
    }

    // find 返回 optional：有值才解引用
    if (auto pos = values.find(20)) {
        std::cout << "\n查找 20 的下标: " << *pos << '\n';
    }

    std::cout << "删除位置 1 得到 " << values.remove(1) << "，剩余:";
    for (int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << '\n';
}
