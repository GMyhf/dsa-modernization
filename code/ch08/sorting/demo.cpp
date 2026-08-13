#include "modern.hpp"

#include <iostream>
#include <vector>

namespace {
void print(const char* name, const std::vector<int>& values) {
    std::cout << name;
    for (int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << '\n';
}
}  // namespace

int main() {
    const std::vector<int> raw{3, -2, 7, 3, 0, -2, 9, 1};

    auto insertion = raw;
    dsa::sorting::insertion_sort(insertion);
    print("插入:", insertion);

    auto heap = raw;
    dsa::sorting::heap_sort(heap);
    print("堆排:", heap);

    auto quick = raw;
    dsa::sorting::quick_sort(quick);
    print("快排:", quick);

    auto radix = raw;
    dsa::sorting::radix_sort(radix);
    print("基数:", radix);
}
