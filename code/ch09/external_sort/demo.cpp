#include "modern.hpp"

#include <iostream>

int main() {
    const std::vector<int> input{50, 49, 35, 45, 30, 25, 15, 60, 16, 27, 1};
    const auto runs = dsa::external_sort::replacement_selection(input, 7);

    std::cout << "工作区 M=7，得到 " << runs.size() << " 个顺串\n";
    for (std::size_t index = 0; index < runs.size(); ++index) {
        std::cout << "顺串 " << index + 1 << "（长度 " << runs[index].size() << "）:";
        for (int value : runs[index]) {
            std::cout << ' ' << value;
        }
        std::cout << '\n';
    }

    dsa::external_sort::LoserTree tree({20, 6, 8, 9, 11});
    std::cout << "败者树当前冠军: " << *tree.winner() << '\n';
    tree.replace(1, 15);
    std::cout << "替换后冠军: " << *tree.winner() << '\n';
}
