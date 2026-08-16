// 第 4 章「先跑一遍」：用教学版 String 走一遍 append / substr / find。
// 编译运行：
//   g++ -std=c++17 -I code/ch04/string_class code/ch04/string_class/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    String text = "Hello";                       // 隐式转换：String(const char*)
    text.append(' ').append('C').append('+').append('+');
    std::cout << "拼接后: " << text.c_str() << '\n';

    const String slice = text.substr(6, 3);
    std::cout << "子串: " << slice.c_str() << '\n';

    // find 返回 optional：有值才解引用
    std::cout << "首次出现 C 的下标: ";
    if (const auto found = text.find('C')) {
        std::cout << *found << '\n';
    } else {
        std::cout << "无\n";
    }
}
