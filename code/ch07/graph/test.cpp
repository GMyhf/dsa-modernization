#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
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
    check(disconnected.dijkstra(0)[2] == dsa::Graph::infinity, "算法7.8 unreachable stays infinity");
    bool rejected = false; try { disconnected.add_edge(1, 2, -1); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "代码7.3 rejects negative Dijkstra weight");
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
}  // namespace
int main() { test_traversals_and_topology(); test_shortest_paths(); test_minimum_spanning_trees(); std::printf("Graph: %d 项断言，%d 失败\n", checks, failures); return failures == 0 ? 0 : 1; }
