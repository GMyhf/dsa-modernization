#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa {

// >>> graph-list
/// 邻接表存图（原书【代码7.4】）：每个顶点挂一条边表，只存**实际存在**的边。
///
/// 与同章 `Graph`（邻接矩阵）的区别只有一处——存储方式——但这一处决定了全部代价：
///
/// | | 邻接矩阵 | 邻接表 |
/// | --- | --- | --- |
/// | 存储量 | $V^2$ | $V + E$ |
/// | 遍历某点的邻居 | $O(V)$，要扫过整行 | $O(\deg v)$，只走这条边表 |
/// | 判断 (u,v) 是否有边 | $O(1)$ | $O(\deg u)$ |
/// | DFS / BFS 全图 | $O(V^2)$ | $O(V + E)$ |
///
/// 稀疏图（$E \ll V^2$）上邻接表快得多；稠密图或频繁问「这两点之间有没有边」时，
/// 矩阵反而合适。**没有哪种表示法总是更好**，这正是本章要比较的东西。
class GraphList {
public:
    static constexpr int infinity = std::numeric_limits<int>::max() / 4;

    struct Edge {
        std::size_t from;
        std::size_t to;
        int weight;

        bool operator<(const Edge& other) const noexcept { return weight < other.weight; }
    };

    explicit GraphList(std::size_t count) : adjacency_(count) {}

    /// 加边。重复加同一条 `from→to` 时**覆盖权值**而不是并存两条——
    /// 与邻接矩阵的语义保持一致，两种表示法才可以逐项对拍。代价是每次加边 O(deg from)。
    void add_edge(std::size_t from, std::size_t to, int weight, bool directed = true) {
        check_vertex(from);
        check_vertex(to);
        if (weight < 0) {
            throw std::invalid_argument("negative edge");
        }
        put(from, to, weight);
        if (!directed) {
            put(to, from, weight);
        }
    }

    [[nodiscard]] std::size_t vertices() const noexcept { return adjacency_.size(); }

    /// 边表里的条目数。无向图一条边会在两端各存一次，这里如实返回条目数。
    [[nodiscard]] std::size_t edge_entries() const noexcept {
        std::size_t total = 0;
        for (const auto& list : adjacency_) {
            total += list.size();
        }
        return total;
    }

    [[nodiscard]] std::size_t degree(std::size_t vertex) const {
        check_vertex(vertex);
        return adjacency_[vertex].size();
    }

    /// 某个顶点的边表。邻接表的核心能力：拿到邻居不必扫过不存在的边。
    [[nodiscard]] const std::vector<Edge>& neighbors(std::size_t vertex) const {
        check_vertex(vertex);
        return adjacency_[vertex];
    }

    [[nodiscard]] std::optional<int> weight(std::size_t from, std::size_t to) const {
        check_vertex(from);
        check_vertex(to);
        for (const Edge& edge : adjacency_[from]) {
            ++scanned_;
            if (edge.to == to) {
                return edge.weight;
            }
        }
        return std::nullopt;  // 没有这条边是预期状态，不是错误
    }

    /// 存储量（按「一个整数算一格」计）。对照矩阵的 $V^2$，稀疏图上差距一目了然。
    [[nodiscard]] std::size_t storage_cells() const noexcept {
        return vertices() + edge_entries() * 2;  // 每条边存 to 和 weight
    }

    /// 已经检查过多少条边。教学计数器：用它把 $O(V+E)$ 和 $O(V^2)$ 的差别量出来。
    [[nodiscard]] std::size_t scan_steps() const noexcept { return scanned_; }
    void reset_scan_steps() const noexcept { scanned_ = 0; }
    // <<< graph-list

    // >>> graph-list-traversal
    /// 深度优先周游。只走边表，不看不存在的边——这是与矩阵版最直接的差别。
    [[nodiscard]] std::vector<std::size_t> dfs(std::size_t source) const {
        check_vertex(source);
        std::vector<bool> seen(vertices());
        std::vector<std::size_t> result;
        visit_depth_first(source, seen, result);
        return result;
    }

    [[nodiscard]] std::vector<std::size_t> bfs(std::size_t source) const {
        check_vertex(source);
        std::vector<bool> seen(vertices());
        std::queue<std::size_t> pending;
        std::vector<std::size_t> result;
        pending.push(source);
        seen[source] = true;
        while (!pending.empty()) {
            const std::size_t from = pending.front();
            pending.pop();
            result.push_back(from);
            for (const Edge& edge : adjacency_[from]) {  // 矩阵版这里要扫 V 格
                ++scanned_;
                if (!seen[edge.to]) {
                    seen[edge.to] = true;
                    pending.push(edge.to);
                }
            }
        }
        return result;
    }
    // <<< graph-list-traversal

    /// 拓扑排序。入度统计走边表，O(V+E)；矩阵版要 O(V²)。有环返回 nullopt。
    [[nodiscard]] std::optional<std::vector<std::size_t>> topological_sort() const {
        std::vector<std::size_t> indegree(vertices());
        for (const auto& list : adjacency_) {
            for (const Edge& edge : list) {
                ++scanned_;
                ++indegree[edge.to];
            }
        }
        std::queue<std::size_t> pending;
        for (std::size_t vertex = 0; vertex < vertices(); ++vertex) {
            if (indegree[vertex] == 0) {
                pending.push(vertex);
            }
        }
        std::vector<std::size_t> result;
        while (!pending.empty()) {
            const std::size_t from = pending.front();
            pending.pop();
            result.push_back(from);
            for (const Edge& edge : adjacency_[from]) {
                ++scanned_;
                if (--indegree[edge.to] == 0) {
                    pending.push(edge.to);
                }
            }
        }
        if (result.size() != vertices()) {
            return std::nullopt;  // 还有入度非零的顶点，说明有环
        }
        return result;
    }

    // >>> graph-list-dijkstra
    /// Dijkstra，最小堆版。代价 $O((V+E)\log V)$。
    ///
    /// 矩阵版每轮要扫一遍全部顶点找最近的那个，是 $O(V^2)$；换成邻接表 + 堆之后，
    /// 「找最近顶点」由堆负责、「松弛」只走实际存在的边。**稀疏图上这才是该用的组合**。
    /// 堆是第 5 章的教学内容，这里作为基础设施使用（见 unit.json 的 d001_exceptions）。
    [[nodiscard]] std::vector<int> dijkstra(std::size_t source) const {
        check_vertex(source);
        std::vector<int> distance(vertices(), infinity);
        distance[source] = 0;

        using Item = std::pair<int, std::size_t>;  // (当前距离, 顶点)
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap;
        heap.emplace(0, source);
        while (!heap.empty()) {
            const auto [dist, from] = heap.top();
            heap.pop();
            if (dist > distance[from]) {
                continue;  // 堆里的旧条目，已经被更短的路径取代
            }
            for (const Edge& edge : adjacency_[from]) {
                ++scanned_;
                const int relaxed = dist + edge.weight;
                if (relaxed < distance[edge.to]) {
                    distance[edge.to] = relaxed;
                    heap.emplace(relaxed, edge.to);
                }
            }
        }
        return distance;
    }
    // <<< graph-list-dijkstra

    /// Prim 最小生成树，同样用堆。图不连通时返回 nullopt。
    [[nodiscard]] std::optional<std::vector<Edge>> prim(std::size_t source) const {
        check_vertex(source);
        std::vector<bool> used(vertices());
        std::vector<Edge> result;
        using Item = std::pair<int, Edge>;
        const auto cheaper = [](const Item& a, const Item& b) { return a.first > b.first; };
        std::priority_queue<Item, std::vector<Item>, decltype(cheaper)> heap(cheaper);

        used[source] = true;
        for (const Edge& edge : adjacency_[source]) {
            heap.emplace(edge.weight, edge);
        }
        while (!heap.empty() && result.size() + 1 < vertices()) {
            const auto [weight, edge] = heap.top();
            heap.pop();
            ++scanned_;
            if (used[edge.to]) {
                continue;
            }
            used[edge.to] = true;
            result.push_back(edge);
            for (const Edge& next : adjacency_[edge.to]) {
                if (!used[next.to]) {
                    heap.emplace(next.weight, next);
                }
            }
        }
        if (result.size() + 1 != vertices()) {
            return std::nullopt;  // 没能连上所有顶点
        }
        return result;
    }

private:
    void check_vertex(std::size_t vertex) const {
        if (vertex >= adjacency_.size()) {
            throw std::out_of_range("GraphList: vertex out of range");
        }
    }

    void put(std::size_t from, std::size_t to, int weight) {
        for (Edge& edge : adjacency_[from]) {
            if (edge.to == to) {
                edge.weight = weight;  // 覆盖，保持与矩阵同语义
                return;
            }
        }
        adjacency_[from].push_back(Edge{from, to, weight});
    }

    void visit_depth_first(std::size_t from, std::vector<bool>& seen,
                           std::vector<std::size_t>& result) const {
        seen[from] = true;
        result.push_back(from);
        for (const Edge& edge : adjacency_[from]) {
            ++scanned_;
            if (!seen[edge.to]) {
                visit_depth_first(edge.to, seen, result);
            }
        }
    }

    std::vector<std::vector<Edge>> adjacency_;
    mutable std::size_t scanned_ = 0;
};

}  // namespace dsa
