#include "modern.hpp"

#include <iostream>

int main() {
    dsa::ArrayStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);
    std::cout << "栈顶是 " << *stack.top() << '\n';
    std::cout << "依次弹出:";
    while (auto value = stack.pop()) {
        std::cout << ' ' << *value;
    }
    std::cout << "\n空栈再弹? " << (stack.pop() ? "有值" : "空") << '\n';
}
