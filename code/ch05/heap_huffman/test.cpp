#include "modern.hpp"

#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

int main() {
    int failures = 0;
    int checks = 0;
    auto check = [&](bool value) { ++checks; if (!value) ++failures; };

    dsa::MinHeap<int> heap;
    for (int value : {5, 1, 4, 2, 3}) heap.insert(value);
    dsa::MinHeap<int> copy = heap;
    dsa::MinHeap<int>& alias = copy;
    copy = alias;
    for (int expected = 1; expected <= 5; ++expected) {
        check(heap.remove_min() == expected);
        check(copy.remove_min() == expected);
    }
    check(!heap.remove_min());
    dsa::MinHeap<int> moved = std::move(copy);
    check(copy.empty() && moved.empty());

    int weights[] = {5, 7, 10, 15, 20, 45};
    dsa::HuffmanTree tree(weights, 6);
    check(tree.total_weight() == 102);
    dsa::HuffmanTree single(weights, 1);
    check(single.total_weight() == 5);
    dsa::HuffmanTree empty(nullptr, 0);
    check(empty.total_weight() == 0);
    bool rejected_null = false;
    try { dsa::HuffmanTree invalid(nullptr, 1); }
    catch (const std::invalid_argument&) { rejected_null = true; }
    check(rejected_null);
    int negative[] = {-1};
    bool rejected_negative = false;
    try { dsa::HuffmanTree invalid(negative, 1); }
    catch (const std::invalid_argument&) { rejected_negative = true; }
    check(rejected_negative);
    int overflowing[] = {std::numeric_limits<int>::max(), 1};
    bool rejected_overflow = false;
    try { dsa::HuffmanTree invalid(overflowing, 2); }
    catch (const std::overflow_error&) { rejected_overflow = true; }
    check(rejected_overflow);

    std::printf("HeapHuffman: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
