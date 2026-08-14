#include "modern.hpp"

#include <cstdio>

int main() {
    // 小规模下两种写法完全等价——看不出任何区别，这正是它的危险之处。
    dsa::ownership::RecursiveChain recursive;
    dsa::ownership::IterativeChain iterative;
    for (int i = 0; i < 5; ++i) {
        recursive.push_front(i);
        iterative.push_front(i);
    }
    std::printf("unique_ptr 串链 :");
    for (const int value : recursive.to_vector()) {
        std::printf(" %d", value);
    }
    std::printf("\n自管所有权     :");
    for (const int value : iterative.to_vector()) {
        std::printf(" %d", value);
    }
    std::printf("\n两者内容一致   : %s\n",
                recursive.to_vector() == iterative.to_vector() ? "是" : "否");

    // 规模一大，区别就出来了：迭代释放与链长无关。
    dsa::ownership::IterativeChain big;
    for (int i = 0; i < 500000; ++i) {
        big.push_front(i);
    }
    std::printf("\n自管所有权建了 %zu 个结点，析构走循环，栈深度恒定。\n", big.size());
    std::printf("换成 unique_ptr 串链，同样规模在 -O0 下会段错误"
                "（实测阈值约 5.7 万个结点，见 legacy.md）。\n");
    return 0;
}
