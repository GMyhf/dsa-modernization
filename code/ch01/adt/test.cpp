#include "modern.hpp"

#include <cstdio>
#include <stdexcept>

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

void test_rumor_network() {
    // 图1.2 的相邻矩阵。**注意 OCR 把 ∞ 大量认成了 8**（见 legacy.md），
    // 这里按原书正文还原：正文明写「从顶点 B1、B2、B4、B5 出发……最大值均为 ∞，
    // 而从 B3 出发的最长的最短路径为 10」。∞ 表示没有这条边，因此不 add_route。
    dsa::adt::RumorNetwork network(5);
    network.add_route(0, 3, 4);   // B1 → B4
    network.add_route(0, 4, 3);   // B1 → B5
    // B2 一条出边也没有（该行全为 ∞）
    network.add_route(2, 0, 6);   // B3 → B1
    network.add_route(2, 1, 7);   // B3 → B2
    network.add_route(2, 3, 10);  // B3 → B4
    network.add_route(2, 4, 2);   // B3 → B5
    // B4 一条出边也没有
    network.add_route(4, 0, 5);   // B5 → B1
    network.add_route(4, 1, 5);   // B5 → B2

    // 正文的旁证：「B5 到 B4 需经过 B1 作为中介，顶点序列 B5B1B4」——
    // 5 + 4 = 9，与上面的边完全吻合。
    check(network.best_source() == 2,
          "算法1.1 选出 B3（下标 2）：唯一能到达全部顶点者，最大最短路 10——与原书正文一致");

    dsa::adt::RumorNetwork duplicate(2);
    duplicate.add_route(0, 1, 9);
    duplicate.add_route(0, 1, 3);
    duplicate.add_route(1, 0, 2);
    check(duplicate.best_source() == 1, "算法1.1 retains lower duplicate edge");

    dsa::adt::RumorNetwork disconnected(3);
    disconnected.add_route(0, 1, 1);
    check(!disconnected.best_source(), "算法1.1 disconnected graph is nullopt");
    dsa::adt::RumorNetwork empty(0);
    check(!empty.best_source(), "算法1.1 empty graph is nullopt");
    dsa::adt::RumorNetwork singleton(1);
    check(singleton.best_source() == 0, "算法1.1 singleton source");

    bool bad_cost = false;
    try {
        singleton.add_route(0, 0, -1);
    } catch (const std::invalid_argument&) {
        bad_cost = true;
    }
    check(bad_cost, "算法1.1 rejects negative cost");
    bool bad_vertex = false;
    try {
        singleton.add_route(1, 0, 1);
    } catch (const std::invalid_argument&) {
        bad_vertex = true;
    }
    check(bad_vertex, "算法1.1 rejects invalid vertex");
}
}  // namespace

int main() {
    test_rumor_network();
    std::printf("ADT: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
