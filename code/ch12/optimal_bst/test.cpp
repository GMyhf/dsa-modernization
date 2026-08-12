#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

void test_pool() {
    dsa::advanced::ReusableNodePool<int> pool(2);
    check(pool.available() == 2, "算法12.1 initial free list");
    const auto first = pool.acquire(11);
    const auto second = pool.acquire(22);
    check(first == 0 && second == 1, "算法12.1 allocates fixed slots");
    check(pool.get(*first) != nullptr && *pool.get(*first) == 11,
          "算法12.1 retrieves allocated value");
    check(!pool.acquire(33), "算法12.1 exhausted pool is nullopt");
    check(pool.release(*first), "算法12.1 returns node to free list");
    check(pool.get(*first) == nullptr && pool.available() == 1,
          "算法12.1 released node is unavailable");
    const auto reused = pool.acquire(44);
    check(reused == first && *pool.get(*reused) == 44, "算法12.1 reuses released slot");
    check(!pool.release(*first + 3), "算法12.1 invalid release is false");
    check(pool.release(*first), "算法12.1 releases reused slot");
    check(!pool.release(*first), "算法12.1 double release is false");
}

void test_optimal_bst() {
    const auto result = dsa::advanced::optimal_bst({1, 5, 4, 3}, {5, 4, 3, 2, 1});
    check(result.cost[0][4] == 57, "算法12.2 textbook total cost");
    check(result.root[0][4] == 2, "算法12.2 textbook root");
    check(result.cost[0][1] == 10 && result.root[0][1] == 1,
          "算法12.2 one-key base case");
    const auto empty = dsa::advanced::optimal_bst({}, {7});
    check(empty.cost[0][0] == 0 && empty.root[0][0] == 0, "算法12.2 empty tree");
    bool bad = false;
    try {
        (void)dsa::advanced::optimal_bst({1, 2}, {3, 4});
    } catch (const std::invalid_argument&) {
        bad = true;
    }
    check(bad, "算法12.2 mismatched weights");
}
}  // namespace

int main() {
    test_pool();
    test_optimal_bst();
    std::printf("OptimalBST: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
