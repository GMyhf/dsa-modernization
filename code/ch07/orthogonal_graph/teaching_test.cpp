#include "teaching.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
int failures = 0;

void check(bool value, const char* name) {
    ++checks;
    if (!value) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

void test_two_chains() {
    dsa::OrthogonalGraphTeaching graph(4);
    graph.add_edge(0, 1, 7);
    graph.add_edge(0, 2, 9);
    graph.add_edge(3, 2, 5);
    check(graph.out_neighbors(0) == std::vector<std::size_t>({2, 1}),
          "表头插入后 tailnextarc 是 2、1");
    check(graph.in_neighbors(2) == std::vector<std::size_t>({3, 0}),
          "表头插入后 headnextarc 是 3、0");
    check(graph.out_neighbors(3) == std::vector<std::size_t>({2}),
          "一条弧也在尾点的出链");
    check(graph.in_neighbors(1) == std::vector<std::size_t>({0}),
          "同一条弧也在头点的入链");
    check(graph.out_neighbors(1).empty(), "没有出边时 firstout 是空");
    check(graph.in_neighbors(0).empty(), "没有入边时 firstin 是空");
}

void test_self_loop_and_lifetime() {
    for (int round = 0; round < 20; ++round) {
        dsa::OrthogonalGraphTeaching graph(2);
        graph.add_edge(1, 1);
        graph.add_edge(0, 1);
        check(graph.out_neighbors(1) == std::vector<std::size_t>({1}),
              "自环在自己的出链出现");
        check(graph.in_neighbors(1) == std::vector<std::size_t>({0, 1}),
              "自环与外来弧都在入链出现");
    }  // 析构沿每个顶点的出链删除；ASan 档会检查重复释放和泄漏。
    check(true, "反复构造和析构十字链表完成");
}

void test_bounds() {
    dsa::OrthogonalGraphTeaching graph(2);
    bool bad_tail = false;
    try {
        graph.add_edge(2, 0);
    } catch (const std::out_of_range&) {
        bad_tail = true;
    }
    check(bad_tail, "弧尾越界抛 out_of_range");

    bool bad_head = false;
    try {
        graph.add_edge(0, 2);
    } catch (const std::out_of_range&) {
        bad_head = true;
    }
    check(bad_head, "弧头越界抛 out_of_range");

    bool bad_query = false;
    try {
        (void)graph.out_neighbors(2);
    } catch (const std::out_of_range&) {
        bad_query = true;
    }
    check(bad_query, "查询越界抛 out_of_range");
}
}  // namespace

int main() {
    test_two_chains();
    test_self_loop_and_lifetime();
    test_bounds();
    std::printf("十字链表(原书式教学版)：%d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
