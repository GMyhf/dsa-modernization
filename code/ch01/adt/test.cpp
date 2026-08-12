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
    dsa::adt::RumorNetwork network(5);
    network.add_route(0, 1, 8);
    network.add_route(0, 2, 8);
    network.add_route(0, 3, 4);
    network.add_route(0, 4, 3);
    network.add_route(1, 0, 8);
    network.add_route(1, 2, 8);
    network.add_route(1, 4, 8);
    network.add_route(2, 0, 6);
    network.add_route(2, 1, 7);
    network.add_route(2, 3, 10);
    network.add_route(2, 4, 2);
    network.add_route(3, 0, 8);
    network.add_route(3, 1, 8);
    network.add_route(3, 4, 8);
    network.add_route(4, 0, 5);
    network.add_route(4, 1, 5);
    network.add_route(4, 2, 8);
    network.add_route(4, 3, 8);
    check(network.best_source() == 0, "算法1.1 chooses first minimum from printed matrix");

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
