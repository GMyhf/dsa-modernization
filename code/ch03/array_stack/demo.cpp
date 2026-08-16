// 第 3 章「先跑一遍」：用教学版 ArrayStack 走一遍 push / top / pop。
// 编译运行：
//   g++ -std=c++17 -I code/ch03/array_stack code/ch03/array_stack/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    ArrayStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);

    // top() 返回 optional：有值才解引用，空栈不会崩
    if (auto value = stack.top()) {
        std::cout << "栈顶是 " << *value << '\n';
    }

    std::cout << "依次弹出:";
    while (auto value = stack.pop()) {
        std::cout << ' ' << *value;
    }
    std::cout << "\n空栈再弹? " << (stack.pop() ? "有值" : "空") << '\n';
}
