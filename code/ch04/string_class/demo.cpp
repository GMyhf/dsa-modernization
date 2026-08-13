#include "modern.hpp"

#include <iostream>

int main() {
    dsa::String text = "Hello";
    text.append(' ').append('C').append('+').append('+');
    std::cout << "拼接后: " << text.c_str() << '\n';
    const auto slice = text.substr(6, 3);
    std::cout << "子串: " << slice.c_str() << '\n';
    const auto found = text.find('C');
    std::cout << "首次出现 C 的下标: ";
    if (found) {
        std::cout << *found << '\n';
    } else {
        std::cout << "无\n";
    }
}
