#include "modern.hpp"

// 与同章的邻接矩阵实现逐项对拍：换了存储方式，答案必须一模一样。
#include "../graph/modern.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <random>
#include <set>
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

using dsa::Graph;       // 邻接矩阵
using dsa::GraphList;   // 邻接表

void test_storage_shape() {
    GraphList graph(4);
    check(graph.vertices() == 4 && graph.edge_entries() == 0, "7.4 空图没有边表条目");

    graph.add_edge(0, 1, 5);
    graph.add_edge(0, 2, 3);
    graph.add_edge(3, 0, 7);
    check(graph.edge_entries() == 3, "7.4 三条有向边三个条目");
    check(graph.degree(0) == 2 && graph.degree(1) == 0 && graph.degree(3) == 1,
          "7.4 出度就是边表长度");
    check(graph.neighbors(0).size() == 2, "7.4 邻居直接来自边表");
    check(graph.weight(0, 1).has_value() && *graph.weight(0, 1) == 5, "7.4 取边权");
    check(!graph.weight(1, 0).has_value(), "7.4 反向边不存在返回 nullopt");
    check(!graph.weight(2, 3).has_value(), "7.4 不存在的边");

    // 无向边在两端各存一次——如实计数，不含糊。
    GraphList undirected(3);
    undirected.add_edge(0, 1, 4, false);
    check(undirected.edge_entries() == 2 && undirected.degree(0) == 1 && undirected.degree(1) == 1,
          "7.4 一条无向边在两端各存一次");

    // 重复加边覆盖权值，与邻接矩阵同语义，两种表示法才能对拍。
    GraphList repeated(2);
    repeated.add_edge(0, 1, 9);
    repeated.add_edge(0, 1, 2);
    check(repeated.edge_entries() == 1 && *repeated.weight(0, 1) == 2, "7.4 重复加边覆盖而非并存");
}

void test_storage_advantage_is_measurable() {
    // 稀疏图：1000 个顶点，每点一条出边（一条长链）。
    constexpr std::size_t n = 1000;
    GraphList sparse(n);
    for (std::size_t v = 0; v + 1 < n; ++v) {
        sparse.add_edge(v, v + 1, 1);
    }
    // 邻接表 O(V+E)；矩阵是 V² = 100 万格，差三个数量级。
    check(sparse.storage_cells() < n * 4, "7.4 邻接表存储量随 V+E 走");
    check(sparse.storage_cells() * 100 < n * n, "7.4 稀疏图上远小于矩阵的 V²");

    sparse.reset_scan_steps();
    const auto order = sparse.bfs(0);
    check(order.size() == n, "7.4 稀疏图 BFS 走遍全图");
    // 邻接表 BFS 只看实际存在的边：999 步。
    check(sparse.scan_steps() == n - 1, "7.4 BFS 的检查次数等于边数");
    // 邻接矩阵 BFS 每出队一个顶点就要扫过整行 V 格，合计 V² = 100 万格。
    check(sparse.scan_steps() * 1000 < n * n, "7.4 比矩阵的 V² 少三个数量级");
}

/// 两种表示法建同一张图。边按 `to` 升序加入，使邻接表的遍历次序与矩阵的
/// 「从 0 扫到 V-1」一致——否则 DFS/BFS 的**访问顺序**会不同（都对，但不可逐项比）。
struct Pair {
    Graph matrix;
    GraphList list;
};

Pair build(std::size_t n, const std::vector<std::array<int, 3>>& edges, bool directed = true) {
    Pair pair{Graph(n), GraphList(n)};
    for (const auto& e : edges) {
        pair.matrix.add_edge(static_cast<std::size_t>(e[0]), static_cast<std::size_t>(e[1]), e[2],
                             directed);
        pair.list.add_edge(static_cast<std::size_t>(e[0]), static_cast<std::size_t>(e[1]), e[2],
                           directed);
    }
    return pair;
}

void test_traversals_agree_with_the_matrix() {
    auto pair = build(6, {{0, 1, 1}, {0, 2, 1}, {1, 3, 1}, {2, 4, 1}, {3, 5, 1}, {4, 5, 1}});
    check(pair.list.dfs(0) == pair.matrix.dfs(0), "7.4 DFS 序列与矩阵版一致");
    check(pair.list.bfs(0) == pair.matrix.bfs(0), "7.4 BFS 序列与矩阵版一致");
    check(pair.list.dfs(5) == pair.matrix.dfs(5), "7.4 从汇点出发也一致");
    check(pair.list.bfs(3).size() == 2, "7.4 只到达可达的部分");

    const auto list_topo = pair.list.topological_sort();
    const auto matrix_topo = pair.matrix.topological_sort();
    check(list_topo.has_value() && matrix_topo.has_value(), "7.4 无环图有拓扑序");
    check(*list_topo == *matrix_topo, "7.4 拓扑序与矩阵版一致");

    // 有环图两边都必须报 nullopt。
    auto cyclic = build(3, {{0, 1, 1}, {1, 2, 1}, {2, 0, 1}});
    check(!cyclic.list.topological_sort().has_value(), "7.4 有环无拓扑序");
    check(!cyclic.matrix.topological_sort().has_value(), "7.4 矩阵版同样报无解");
}

void test_shortest_paths_agree_with_the_matrix() {
    auto pair = build(6, {{0, 1, 7}, {0, 2, 9}, {0, 5, 14}, {1, 2, 10},
                          {1, 3, 15}, {2, 3, 11}, {2, 5, 2}, {3, 4, 6}, {5, 4, 9}},
                      false);
    const auto list_dist = pair.list.dijkstra(0);
    const auto matrix_dist = pair.matrix.dijkstra(0);
    check(list_dist == matrix_dist, "7.4 Dijkstra 结果与矩阵版逐项一致");
    check(list_dist[4] == 20, "7.4 教科书例子：0 到 4 的最短距离是 20");
    check(list_dist[0] == 0, "7.4 源点到自己是 0");

    // 不可达顶点：两边都应给出 infinity。
    auto broken = build(3, {{0, 1, 5}});
    check(broken.list.dijkstra(0)[2] == GraphList::infinity, "7.4 不可达顶点是 infinity");
    check(broken.matrix.dijkstra(0)[2] == Graph::infinity, "7.4 矩阵版同样");
}

void test_prim_agrees_on_total_weight() {
    auto pair = build(5, {{0, 1, 2}, {0, 3, 6}, {1, 2, 3}, {1, 3, 8}, {1, 4, 5}, {2, 4, 7}},
                      false);
    const auto list_mst = pair.list.prim(0);
    const auto matrix_mst = pair.matrix.prim(0);
    check(list_mst.has_value() && matrix_mst.has_value(), "7.4 连通图有生成树");
    check(list_mst->size() == 4, "7.4 5 个顶点的生成树有 4 条边");

    const auto total = [](const auto& edges) {
        int sum = 0;
        for (const auto& edge : edges) {
            sum += edge.weight;
        }
        return sum;
    };
    // 最小生成树可能不唯一，但**总权必须相同**——这是能逐项断言的不变量。
    check(total(*list_mst) == total(*matrix_mst), "7.4 MST 总权与矩阵版一致");
    check(total(*list_mst) == 16, "7.4 教科书例子的 MST 总权是 16");

    GraphList disconnected(4);
    disconnected.add_edge(0, 1, 1, false);
    check(!disconnected.prim(0).has_value(), "7.4 不连通图没有生成树");
}

void test_argument_checks() {
    GraphList graph(3);
    for (auto call : {+[](GraphList& g) { g.add_edge(3, 0, 1); },
                      +[](GraphList& g) { g.add_edge(0, 3, 1); }}) {
        bool threw = false;
        try {
            call(graph);
        } catch (const std::out_of_range&) {
            threw = true;
        }
        check(threw, "7.4 顶点越界抛 out_of_range");
    }
    bool threw = false;
    try {
        graph.add_edge(0, 1, -1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "7.4 负权边被拒绝（Dijkstra 的前提）");

    threw = false;
    try {
        (void)graph.dfs(99);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "7.4 从不存在的顶点出发抛 out_of_range");
}

void test_random_graphs_match_the_matrix() {
    // 固定种子：失败可复现。随机图上两种表示法必须给出相同答案。
    std::mt19937 rng(20260814);
    for (int round = 0; round < 30; ++round) {
        const std::size_t n = 8;
        Graph matrix(n);
        GraphList list(n);
        std::uniform_int_distribution<int> weight(1, 20);
        std::uniform_int_distribution<int> present(0, 3);
        // 按 (from, to) 升序加边，两种表示法的遍历次序才一致。
        for (std::size_t from = 0; from < n; ++from) {
            for (std::size_t to = 0; to < n; ++to) {
                if (from != to && present(rng) == 0) {
                    const int w = weight(rng);
                    matrix.add_edge(from, to, w);
                    list.add_edge(from, to, w);
                }
            }
        }
        check(list.dfs(0) == matrix.dfs(0), "7.4 随机图 DFS 一致");
        check(list.bfs(0) == matrix.bfs(0), "7.4 随机图 BFS 一致");
        check(list.dijkstra(0) == matrix.dijkstra(0), "7.4 随机图 Dijkstra 一致");
        check(list.topological_sort().has_value() == matrix.topological_sort().has_value(),
              "7.4 随机图是否有拓扑序的判断一致");
    }
}
}  // namespace

int main() {
    test_storage_shape();
    test_storage_advantage_is_measurable();
    test_traversals_agree_with_the_matrix();
    test_shortest_paths_agree_with_the_matrix();
    test_prim_agrees_on_total_weight();
    test_argument_checks();
    test_random_graphs_match_the_matrix();
    std::printf("GraphList: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
