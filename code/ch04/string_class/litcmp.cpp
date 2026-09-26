// 字符串字面量比较陷阱：const char* 比较的是指针地址，不是内容。
// 编译运行：
//   g++ -std=c++17 -Wall -Wextra litcmp.cpp -o litcmp && ./litcmp
#include <iostream>
#include <string>

int main() {
    // 字面量比较：比较的是两个 const char* 指针地址，结果未定义
    std::cout << std::boolalpha;
    std::cout << "字面量:   " << ("123" < "1234") << ' ' << ("1234" < "23") << '\n';

    // std::string 比较：按字典序，符合直觉
    std::cout << "std::string: "
              << (std::string("123") < std::string("1234")) << ' '
              << (std::string("1234") < std::string("23")) << '\n';
}
