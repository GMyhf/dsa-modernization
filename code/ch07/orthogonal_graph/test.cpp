#include "modern.hpp"
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

template <typename Graph>
void add_example(Graph& graph) {
    graph.add_edge(0, 1);
    graph.add_edge(0, 2);
    graph.add_edge(3, 2);
}

void test_two_chains() {
    dsa::OrthogonalGraphTeaching teaching(4);
    add_example(teaching);
    check(teaching.out_neighbors(0) == std::vector<std::size_t>({2, 1}),
          "原书式 tailnextarc 沿 0 的出链走");
    check(teaching.in_neighbors(2) == std::vector<std::size_t>({3, 0}),
          "原书式 headnextarc 沿 2 的入链走");

    dsa::OrthogonalGraph graph(4);
    add_example(graph);
    check(graph.edges() == 3, "十字链表每条弧只拥有一个结点");
    check(graph.out_neighbors(0) == std::vector<std::size_t>({2, 1}),
          "现代版 tailnextarc 沿 0 的出链走");
    check(graph.in_neighbors(2) == std::vector<std::size_t>({3, 0}),
          "现代版 headnextarc 沿 2 的入链走");
}

void test_remove_unlinks_both_chains() {
    dsa::OrthogonalGraph graph(4);
    add_example(graph);
    check(graph.remove_edge(0, 2), "删除已有弧返回 true");
    check(graph.edges() == 2, "删除后唯一所有者释放弧结点");
    check(graph.out_neighbors(0) == std::vector<std::size_t>({1}),
          "删除从 tail 的出链摘除");
    check(graph.in_neighbors(2) == std::vector<std::size_t>({3}),
          "删除从 head 的入链摘除");
    check(!graph.remove_edge(0, 2), "重复删除不会碰另一条链");
}

void test_rejections() {
    dsa::OrthogonalGraph graph(2);
    graph.add_edge(0, 1);
    bool duplicate = false;
    try {
        graph.add_edge(0, 1);
    } catch (const std::invalid_argument&) {
        duplicate = true;
    }
    check(duplicate, "重复弧被拒绝");

    bool out_of_range = false;
    try {
        (void)graph.in_neighbors(2);
    } catch (const std::out_of_range&) {
        out_of_range = true;
    }
    check(out_of_range, "顶点越界抛 out_of_range");
}
}  // namespace

int main() {
    test_two_chains();
    test_remove_unlinks_both_chains();
    test_rejections();
    std::printf("十字链表：%d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
