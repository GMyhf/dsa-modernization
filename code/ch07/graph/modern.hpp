#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa {

// >>> graph
class Graph {
public:
    static constexpr int infinity = std::numeric_limits<int>::max() / 4;

    struct Edge {
        std::size_t from;
        std::size_t to;
        int weight;

        bool operator<(const Edge& other) const noexcept { return weight < other.weight; }
    };

    // >>> graph-build
    explicit Graph(std::size_t count) : adjacency_(count, std::vector<int>(count, infinity)) {
        for (std::size_t vertex = 0; vertex < count; ++vertex) {
            adjacency_[vertex][vertex] = 0;
        }
    }

    [[nodiscard]] std::size_t vertices() const noexcept { return adjacency_.size(); }

    void add_edge(std::size_t from, std::size_t to, int weight, bool directed = true) {
        check_vertex(from);
        check_vertex(to);
        if (weight < 0) {
            throw std::invalid_argument("negative edge");
        }
        if (weight >= infinity) {  // infinity 表示「无边」，这么大的权存进去就被当成没有边
            throw std::invalid_argument("edge weight must be below Graph::infinity");
        }
        adjacency_[from][to] = weight;
        if (!directed) {
            adjacency_[to][from] = weight;
        }
    }
    // <<< graph-build

    // >>> dfs
    [[nodiscard]] std::vector<std::size_t> dfs(std::size_t source) const {
        check_vertex(source);
        std::vector<bool> seen(vertices());
        std::vector<std::size_t> result;
        visit_depth_first(source, seen, result);
        return result;
    }
    // <<< dfs

    // >>> bfs
    [[nodiscard]] std::vector<std::size_t> bfs(std::size_t source) const {
        check_vertex(source);
        std::vector<bool> seen(vertices());
        std::queue<std::size_t> queue;
        std::vector<std::size_t> result;
        queue.push(source);
        seen[source] = true;
        while (!queue.empty()) {
            const std::size_t from = queue.front();
            queue.pop();
            result.push_back(from);
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (adjacency_[from][to] < infinity && !seen[to]) {
                    seen[to] = true;
                    queue.push(to);
                }
            }
        }
        return result;
    }
    // <<< bfs

    // >>> topological
    [[nodiscard]] std::optional<std::vector<std::size_t>> topological_sort() const {
        std::vector<std::size_t> indegree(vertices());
        for (std::size_t from = 0; from < vertices(); ++from) {
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (from != to && adjacency_[from][to] < infinity) {
                    ++indegree[to];
                }
            }
        }
        std::queue<std::size_t> queue;
        for (std::size_t vertex = 0; vertex < vertices(); ++vertex) {
            if (indegree[vertex] == 0) {
                queue.push(vertex);
            }
        }
        std::vector<std::size_t> result;
        while (!queue.empty()) {
            const std::size_t from = queue.front();
            queue.pop();
            result.push_back(from);
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (from != to && adjacency_[from][to] < infinity && --indegree[to] == 0) {
                    queue.push(to);
                }
            }
        }
        return result.size() == vertices() ? std::optional<std::vector<std::size_t>>(result)
                                           : std::nullopt;
    }
    // <<< topological

    // >>> dijkstra
    [[nodiscard]] std::vector<int> dijkstra(std::size_t source) const {
        check_vertex(source);
        std::vector<int> distance(vertices(), infinity);
        std::vector<bool> used(vertices());
        distance[source] = 0;
        for (std::size_t count = 0; count < vertices(); ++count) {
            const std::size_t from = nearest_unvisited(distance, used);
            if (from == vertices() || distance[from] == infinity) {
                break;
            }
            used[from] = true;
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (adjacency_[from][to] < infinity &&
                    distance[to] > distance[from] + adjacency_[from][to]) {
                    distance[to] = distance[from] + adjacency_[from][to];
                }
            }
        }
        return distance;
    }
    // <<< dijkstra

    // >>> dijkstra-path
    /// 原书 Dist 类的 `length` 与 `pre` 两个域：到各点的距离，和最短路径上的前一个顶点。
    /// 源点自己、以及到不了的顶点，没有前一个顶点（`std::nullopt`）。
    struct ShortestPathTree {
        std::size_t source;
        std::vector<int> distance;
        std::vector<std::optional<std::size_t>> predecessor;
    };

    /// 与 `dijkstra` 同一个算法，多记一个 `pre`：**只在松弛成功时**改前驱。
    /// 溢出：两个加数都小于 infinity = INT_MAX/4，和小于 INT_MAX/2，不会溢出；
    /// 代价是总长 >= infinity 的路径会被当成「到不了」。
    [[nodiscard]] ShortestPathTree dijkstra_tree(std::size_t source) const {
        check_vertex(source);
        ShortestPathTree tree{source, std::vector<int>(vertices(), infinity),
                              std::vector<std::optional<std::size_t>>(vertices())};
        std::vector<bool> used(vertices());
        tree.distance[source] = 0;
        for (std::size_t count = 0; count < vertices(); ++count) {
            const std::size_t from = nearest_unvisited(tree.distance, used);
            if (from == vertices() || tree.distance[from] == infinity) {
                break;
            }
            used[from] = true;
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (adjacency_[from][to] < infinity &&
                    tree.distance[to] > tree.distance[from] + adjacency_[from][to]) {
                    tree.distance[to] = tree.distance[from] + adjacency_[from][to];
                    tree.predecessor[to] = from;
                }
            }
        }
        return tree;
    }

    /// 顺着 `pre` 从汇点倒着走回源点，再把序列翻过来。到不了返回 `std::nullopt`；
    /// 汇点就是源点时路径是 `{source}`。前驱链断了或成环，说明传进来的树是坏的。
    [[nodiscard]] static std::optional<std::vector<std::size_t>> shortest_path(
        const ShortestPathTree& tree, std::size_t target) {
        const std::size_t count = tree.distance.size();
        if (target >= count || tree.source >= count || tree.predecessor.size() != count) {
            throw std::out_of_range("vertex");
        }
        if (tree.distance[target] == infinity) {
            return std::nullopt;
        }
        std::vector<std::size_t> path{target};
        for (std::size_t vertex = target; vertex != tree.source;) {
            if (!tree.predecessor[vertex] || path.size() > count) {
                throw std::invalid_argument("broken predecessor chain");
            }
            vertex = *tree.predecessor[vertex];
            path.push_back(vertex);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
    // <<< dijkstra-path

    // >>> floyd
    [[nodiscard]] std::vector<std::vector<int>> floyd() const {
        auto distance = adjacency_;
        for (std::size_t via = 0; via < vertices(); ++via) {
            for (std::size_t from = 0; from < vertices(); ++from) {
                for (std::size_t to = 0; to < vertices(); ++to) {
                    if (distance[from][via] < infinity && distance[via][to] < infinity) {
                        distance[from][to] = std::min(
                            distance[from][to], distance[from][via] + distance[via][to]);
                    }
                }
            }
        }
        return distance;
    }
    // <<< floyd

    // >>> prim
    [[nodiscard]] std::optional<std::vector<Edge>> prim(std::size_t source) const {
        check_vertex(source);
        std::vector<int> distance(vertices(), infinity);
        std::vector<std::size_t> predecessor(vertices());
        std::vector<bool> used(vertices());
        std::vector<Edge> result;
        distance[source] = 0;
        for (std::size_t count = 0; count < vertices(); ++count) {
            const std::size_t from = nearest_unvisited(distance, used);
            if (from == vertices() || distance[from] == infinity) {
                return std::nullopt;
            }
            used[from] = true;
            if (from != source) {
                result.push_back({predecessor[from], from, distance[from]});
            }
            for (std::size_t to = 0; to < vertices(); ++to) {
                if (!used[to] && adjacency_[from][to] < distance[to]) {
                    distance[to] = adjacency_[from][to];
                    predecessor[to] = from;
                }
            }
        }
        return result;
    }
    // <<< prim

    // >>> kruskal
    [[nodiscard]] std::optional<std::vector<Edge>> kruskal() const {
        std::vector<Edge> edges;
        for (std::size_t from = 0; from < vertices(); ++from) {
            for (std::size_t to = from + 1; to < vertices(); ++to) {
                if (adjacency_[from][to] < infinity) {
                    edges.push_back({from, to, adjacency_[from][to]});
                }
            }
        }
        std::sort(edges.begin(), edges.end());
        std::vector<std::size_t> parent(vertices());
        for (std::size_t vertex = 0; vertex < vertices(); ++vertex) {
            parent[vertex] = vertex;
        }
        std::vector<Edge> result;
        for (const Edge& edge : edges) {
            const std::size_t from_root = find_root(parent, edge.from);
            const std::size_t to_root = find_root(parent, edge.to);
            if (from_root != to_root) {
                parent[from_root] = to_root;
                result.push_back(edge);
            }
        }
        return result.size() + 1 == vertices() ? std::optional<std::vector<Edge>>(result)
                                                : std::nullopt;
    }
    // <<< kruskal

private:
    // Kept recursive to match the textbook DFS; deep graphs have Stack Overflow Risk.
    void visit_depth_first(std::size_t from, std::vector<bool>& seen,
                           std::vector<std::size_t>& result) const {
        seen[from] = true;
        result.push_back(from);
        for (std::size_t to = 0; to < vertices(); ++to) {
            if (adjacency_[from][to] < infinity && !seen[to]) {
                visit_depth_first(to, seen, result);
            }
        }
    }

    [[nodiscard]] std::size_t nearest_unvisited(const std::vector<int>& distance,
                                                const std::vector<bool>& used) const {
        std::size_t nearest = vertices();
        for (std::size_t vertex = 0; vertex < vertices(); ++vertex) {
            if (!used[vertex] &&
                (nearest == vertices() || distance[vertex] < distance[nearest])) {
                nearest = vertex;
            }
        }
        return nearest;
    }

    static std::size_t find_root(std::vector<std::size_t>& parent, std::size_t vertex) {
        if (parent[vertex] != vertex) {
            parent[vertex] = find_root(parent, parent[vertex]);
        }
        return parent[vertex];
    }

    void check_vertex(std::size_t vertex) const {
        if (vertex >= vertices()) {
            throw std::out_of_range("vertex");
        }
    }

    std::vector<std::vector<int>> adjacency_;
};
// <<< graph

}  // namespace dsa
