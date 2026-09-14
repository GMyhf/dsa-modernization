#include "modern.hpp"
#include "support/shared_cases.hpp"

#include <cstdio>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool value, const char* name) { ++checks; if (!value) { ++failures; std::printf("  FAIL: %s\n", name); } }
int weight(const std::vector<dsa::Graph::Edge>& edges) { int total = 0; for (const auto& edge : edges) total += edge.weight; return total; }

dsa::Graph directed() {
    dsa::Graph graph(5);
    graph.add_edge(0, 1, 2); graph.add_edge(0, 2, 7); graph.add_edge(1, 2, 1);
    graph.add_edge(1, 3, 5); graph.add_edge(2, 3, 1); graph.add_edge(3, 4, 3);
    return graph;
}
void test_traversals_and_topology() {
    const auto graph = directed();
    check(graph.vertices() == 5, "代码7.1 vertex count");
    check(graph.dfs(0) == std::vector<std::size_t>({0, 1, 2, 3, 4}), "算法7.5 DFS order");
    check(graph.bfs(0) == std::vector<std::size_t>({0, 1, 2, 3, 4}), "算法7.6 BFS order");
    check(graph.dfs(4) == std::vector<std::size_t>({4}), "算法7.5 isolated reachable set");
    const auto topological = graph.topological_sort();
    check(topological && topological->front() == 0 && topological->back() == 4, "算法7.7 topological endpoints");
    dsa::Graph cycle(3); cycle.add_edge(0, 1, 1); cycle.add_edge(1, 2, 1); cycle.add_edge(2, 0, 1);
    check(!cycle.topological_sort(), "算法7.7 cycle returns nullopt");
    bool rejected = false; try { (void)graph.bfs(5); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected, "代码7.1 rejects invalid vertex");
}
void test_shortest_paths() {
    const auto graph = directed();
    const auto floyd = graph.floyd();
    for (std::size_t source = 0; source < graph.vertices(); ++source) {
        const auto distances = graph.dijkstra(source);
        for (std::size_t target = 0; target < graph.vertices(); ++target) {
            check(distances[target] == floyd[source][target], "算法7.8 Dijkstra matches 算法7.9 Floyd");
        }
    }
    check(floyd[0][4] == 7, "算法7.9 uses intermediate vertices");
    dsa::Graph disconnected(3); disconnected.add_edge(0, 1, 4);
    check(disconnected.dijkstra(0)[2] == dsa::Graph::unreachable, "算法7.8 到不了是 unreachable");
    bool rejected = false; try { disconnected.add_edge(1, 2, -1); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "代码7.3 rejects negative Dijkstra weight");
    // T-078：infinity 兼任「无边」，权 >= infinity 的边若被收下，所有算法都会把它当成不存在。
    for (const int huge : {dsa::Graph::infinity, dsa::Graph::infinity + 1}) {
        dsa::Graph graph(2);
        rejected = false;
        try { graph.add_edge(0, 1, huge); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "T-078 权 >= infinity 被拒绝，而不是静默变成无边");
        check(graph.dijkstra(0)[1] == dsa::Graph::unreachable, "T-078 被拒绝的边没有写进矩阵");
    }
    dsa::Graph largest(2);
    largest.add_edge(0, 1, dsa::Graph::infinity - 1);
    check(largest.dijkstra(0)[1] == dsa::Graph::infinity - 1, "T-078 infinity-1 是合法的最大权，Dijkstra 看得见它");
    check(largest.dfs(0).size() == 2, "T-078 infinity-1 的边对周游也是边");

    // D-039：路径总长达到 infinity 不再被读成「到不了」。int 距离下这里是 infinity == 哨兵。
    dsa::Graph brink(3);
    brink.add_edge(0, 1, dsa::Graph::infinity - 1);
    brink.add_edge(1, 2, 1);
    const dsa::Graph::distance_type exactly = dsa::Graph::infinity;
    check(brink.dijkstra(0)[2] == exactly, "D-039 总长恰为 infinity 的路径 Dijkstra 算得出");
    check(brink.floyd()[0][2] == exactly, "D-039 总长恰为 infinity 的路径 Floyd 算得出");
    check(brink.bfs(0).size() == 3 && brink.shortest_path(brink.dijkstra_tree(0), 2).has_value(),
          "D-039 可达性与 BFS 一致，路径取得出来");
    // 6 顶点长链：总长 5*(infinity-1) = 2684354550 > INT_MAX，int 距离在这里溢出（UBSan 报 signed overflow）。
    dsa::Graph chain(6);
    for (std::size_t v = 0; v + 1 < 6; ++v) chain.add_edge(v, v + 1, dsa::Graph::infinity - 1);
    const dsa::Graph::distance_type total = 5 * static_cast<dsa::Graph::distance_type>(dsa::Graph::infinity - 1);
    check(total == 2684354550LL, "D-039 长链期望值自检");
    check(chain.dijkstra(0)[5] == total, "D-039 长链总长超过 INT_MAX，Dijkstra 仍精确");
    check(chain.floyd()[0][5] == total, "D-039 长链总长超过 INT_MAX，Floyd 仍精确");
    check(chain.dijkstra_tree(0).distance[5] == total, "D-039 长链 dijkstra_tree 距离同样精确");
}
void test_minimum_spanning_trees() {
    dsa::Graph graph(5);
    graph.add_edge(0, 1, 1, false); graph.add_edge(0, 2, 4, false); graph.add_edge(1, 2, 2, false);
    graph.add_edge(1, 3, 5, false); graph.add_edge(2, 3, 1, false); graph.add_edge(3, 4, 3, false);
    const auto prim = graph.prim(0); const auto kruskal = graph.kruskal();
    check(prim && prim->size() == 4, "算法7.10 Prim has n-1 edges");
    check(kruskal && kruskal->size() == 4, "算法7.11 Kruskal has n-1 edges");
    check(weight(*prim) == 7 && weight(*kruskal) == 7, "算法7.10/7.11 same MST weight");
    dsa::Graph disconnected(3); disconnected.add_edge(0, 1, 1, false);
    check(!disconnected.prim(0), "算法7.10 disconnected nullopt");
    check(!disconnected.kruskal(), "算法7.11 disconnected nullopt");
    dsa::Graph one(1); check(one.prim(0)->empty() && one.kruskal()->empty(), "MST singleton empty");
}

// 路径重建：把实现改回「前驱乱记 / 不记 / 不翻转」，下面必有一条红。
// 平局允许任选一条，所以不断言具体路径，只断言路径合法：首尾对、每条边真实存在、边权和等于距离。
using Weights = std::vector<std::vector<int>>;
bool path_is_valid(const std::vector<std::size_t>& path, std::size_t source, std::size_t target,
                   const Weights& weights, dsa::Graph::distance_type distance) {
    if (path.empty() || path.front() != source || path.back() != target) return false;
    long long total = 0;
    for (std::size_t index = 1; index < path.size(); ++index) {
        const int edge = weights[path[index - 1]][path[index]];
        if (edge < 0) return false;
        total += edge;
    }
    return total == distance;
}
// 前驱乱记时 shortest_path 会抛——折成「没取到路径」，让具名断言去红，而不是整个测试 abort。
std::optional<std::vector<std::size_t>> path_or_empty(const dsa::Graph::ShortestPathTree& tree,
                                                      std::size_t target) {
    try {
        return dsa::Graph::shortest_path(tree, target);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}
void test_shortest_path_reconstruction() {
    const auto graph = directed();
    const auto tree = graph.dijkstra_tree(0);
    check(tree.distance == graph.dijkstra(0), "算法7.8 dijkstra_tree 距离与 dijkstra 相同");
    check(tree.predecessor[0] == std::nullopt, "算法7.8 源点没有前驱");
    const auto path = path_or_empty(tree, 4);
    check(path == std::vector<std::size_t>({0, 1, 2, 3, 4}), "算法7.8 唯一最短路 0→1→2→3→4");
    check(path_or_empty(tree, 0) == std::vector<std::size_t>({0}), "算法7.8 汇点即源点");
    dsa::Graph lonely(3); lonely.add_edge(0, 1, 4);
    const auto lonely_tree = lonely.dijkstra_tree(0);
    check(!dsa::Graph::shortest_path(lonely_tree, 2), "算法7.8 到不了返回 nullopt");
    check(!lonely_tree.predecessor[2], "算法7.8 到不了的顶点没有前驱");
    bool rejected = false;
    try { (void)dsa::Graph::shortest_path(lonely_tree, 3); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected, "算法7.8 shortest_path 越界汇点");
    auto broken = tree; broken.predecessor[4] = std::nullopt;
    rejected = false;
    try { (void)dsa::Graph::shortest_path(broken, 4); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "算法7.8 断掉的前驱链被拒绝");
    auto cyclic = tree; cyclic.predecessor[3] = 4;
    rejected = false;
    try { (void)dsa::Graph::shortest_path(cyclic, 4); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "算法7.8 成环的前驱链被拒绝");

    // 随机小图对拍 Floyd。权取 0..4，平局与零权边都会大量出现。
    std::mt19937 random(20260914);
    bool reachability = true, validity = true, distances = true;
    int paths = 0;
    for (int round = 0; round < 300; ++round) {
        const std::size_t count = 1 + random() % 8;
        const bool undirected = round % 2 == 1;
        dsa::Graph sample(count);
        Weights weights(count, std::vector<int>(count, -1));
        for (std::size_t vertex = 0; vertex < count; ++vertex) weights[vertex][vertex] = 0;
        for (std::size_t from = 0; from < count; ++from) {
            for (std::size_t to = 0; to < count; ++to) {
                if (from == to || random() % 100 >= 35) continue;
                const int w = static_cast<int>(random() % 5);
                sample.add_edge(from, to, w, !undirected);
                weights[from][to] = w;
                if (undirected) weights[to][from] = w;
            }
        }
        const auto floyd = sample.floyd();
        for (std::size_t source = 0; source < count; ++source) {
            const auto sample_tree = sample.dijkstra_tree(source);
            distances = distances && sample_tree.distance == floyd[source];
            for (std::size_t target = 0; target < count; ++target) {
                const auto found = path_or_empty(sample_tree, target);
                const bool reachable = floyd[source][target] != dsa::Graph::unreachable;
                reachability = reachability && found.has_value() == reachable;
                if (found && reachable) {
                    ++paths;
                    validity = validity &&
                               path_is_valid(*found, source, target, weights, floyd[source][target]);
                }
            }
        }
    }
    check(distances, "算法7.8 随机图 dijkstra_tree 距离等于 Floyd");
    check(reachability, "算法7.8 随机图 有路径当且仅当 Floyd 可达");
    check(validity, "算法7.8 随机图 路径首尾正确、边都存在、边权和等于最短距离");
    check(paths > 1000, "算法7.8 随机图 确实重建了足够多条路径");
}
}  // namespace
int main() {
    test_traversals_and_topology();
    test_shortest_paths();
    test_minimum_spanning_trees();
    test_shortest_path_reconstruction();
    const auto shared = dsa::shared_cases::load();
    for (const auto& item : shared) {
        if (item.expected_error == "invalid_argument") {
            const auto edge = dsa::shared_cases::integers(item.input);
            dsa::Graph graph(2);
            bool raised = false;
            try {
                graph.add_edge(edge[0], edge[1], edge[2]);
            } catch (const std::invalid_argument&) {
                raised = true;
            }
            check(raised, "T-047 graph exception");
            continue;
        }
        const auto parts = dsa::shared_cases::strings(item.input, '|');
        dsa::Graph graph(std::stoi(parts.at(0)));
        for (const auto& encoded : dsa::shared_cases::strings(parts.at(1), ';')) {
            const auto edge = dsa::shared_cases::integers(encoded);
            graph.add_edge(edge[0], edge[1], edge[2]);
        }
        if (item.operation == "dijkstra_path") {
            const auto path = path_or_empty(
                graph.dijkstra_tree(0), static_cast<std::size_t>(std::stoi(parts.at(2))));
            if (item.expected == "none") {
                check(!path, "T-047 graph path unreachable");
            } else {
                std::vector<std::size_t> expected;
                for (int vertex : dsa::shared_cases::integers(item.expected)) {
                    expected.push_back(static_cast<std::size_t>(vertex));
                }
                check(path == expected, "T-047 graph path");
            }
            continue;
        }
        // 期望距离可能超过 int（长链用例），按 64 位读，不走 integers() 的 stoi。
        std::vector<dsa::Graph::distance_type> expected;
        for (const auto& token : dsa::shared_cases::strings(item.expected)) {
            expected.push_back(std::stoll(token));
        }
        check(graph.dijkstra(0) == expected, "T-047 graph distances");
    }
    std::printf("共享用例: %zu\n", shared.size());
    std::printf("Graph: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
