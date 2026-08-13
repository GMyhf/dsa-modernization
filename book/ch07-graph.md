# 第7章 图

图由顶点和边组成。有向边表示单向关系，无向边表示双向关系，带权边表示代价。先弄清题目属于哪一种，再选算法。

源码：[图与七个算法](../code/ch07/graph/modern.hpp)、
[可运行示例](../code/ch07/graph/demo.cpp)、
[交叉验证测试](../code/ch07/graph/test.cpp)。

## 7.1 先把题目说清楚

| 题目 | 前提 | 结果 |
| --- | --- | --- |
| 「从一个点能走到哪些点」 | 任意图 | DFS 或 BFS 的访问序列 |
| 「课程/任务的可行顺序」 | 有向无环图 | 拓扑序；有环则无解 |
| 「一个源点到各点最短路」 | 边权非负 | Dijkstra 距离数组 |
| 「任意两点最短路」 | 顶点数较小 | Floyd 距离矩阵 |
| 「连通所有点且总权最小」 | 无向连通图 | Prim 或 Kruskal 的边集 |

最短路的「最短」是边权总和最小，不一定是经过边数最少。最小生成树的目标也不是任意两点最短，而是在保证所有顶点连通的前提下让**选中的全部边**总权最小。

本单元用邻接矩阵。没有边记为 `infinity`（`int` 最大值的四分之一，给加法留余量）。环、非连通图等正常失败状态用 `optional` 表达。DFS 仍是递归，深图有栈溢出风险。

## 7.2 如何调用

下面这张有向图：`0→1(2)`、`0→2(7)`、`1→2(1)`、`1→3(5)`、`2→3(1)`、`3→4(3)`。从 0 到 4 的最短路是 `0→1→2→3→4`，总权 7，不是那条权为 7 的直达边 `0→2` 再往后走。

```cpp file=code/ch07/graph/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::Graph graph(5);
    graph.add_edge(0, 1, 2);
    graph.add_edge(0, 2, 7);
    graph.add_edge(1, 2, 1);
    graph.add_edge(1, 3, 5);
    graph.add_edge(2, 3, 1);
    graph.add_edge(3, 4, 3);

    std::cout << "DFS(0):";
    for (std::size_t vertex : graph.dfs(0)) {
        std::cout << ' ' << vertex;
    }
    std::cout << "\nBFS(0):";
    for (std::size_t vertex : graph.bfs(0)) {
        std::cout << ' ' << vertex;
    }

    const auto topo = graph.topological_sort();
    std::cout << "\n拓扑序:";
    if (topo) {
        for (std::size_t vertex : *topo) {
            std::cout << ' ' << vertex;
        }
    } else {
        std::cout << " 无（有环）";
    }

    const auto distance = graph.dijkstra(0);
    std::cout << "\nDijkstra(0->4) = " << distance[4] << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch07/graph \
    code/ch07/graph/demo.cpp -o /tmp/graph-demo
/tmp/graph-demo
```

```console
DFS(0): 0 1 2 3 4
BFS(0): 0 1 2 3 4
拓扑序: 0 1 2 3 4
Dijkstra(0->4) = 7
```

若再加上 `4→0` 形成环，`topological_sort()` 返回 `nullopt`。

## 7.3 再读实现

邻接矩阵的第 `from` 行、第 `to` 列是边权。构造时对角线为 0，其余为 `infinity`。`add_edge(..., directed=false)` 同时写入对称位置。

DFS 进入一个顶点就立刻标记已访问，再沿编号从小到大的出边递归——所以本例从 0 出发的顺序是 0、1、2、3、4。BFS 用队列按层扩展，先到的顶点先出队。

拓扑排序统计每个顶点的入度，把入度为 0 的点入队；每取出一个点，就把它指出的入度减 1。若最终取出的点数少于顶点数，图里有环。

Dijkstra 反复选出尚未确定的、当前距离最小的顶点，用它的出边松弛邻居。Floyd 则枚举中转点 `via`，比较 `from→to` 与 `from→via→to`。测试里五个源点的 Dijkstra 与 Floyd 逐项对拍。

Prim 从指定源点生长，每次把离当前树最近的顶点加进来。Kruskal 把边按权排序，用并查集跳过会形成环的边。两者在连通图上都应得到 `n-1` 条边、相同总权。

## 7.4 现代实现

建图：

```cpp file=code/ch07/graph/modern.hpp#graph-build
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
    adjacency_[from][to] = weight;
    if (!directed) {
        adjacency_[to][from] = weight;
    }
}
```

深度优先与广度优先：

```cpp file=code/ch07/graph/modern.hpp#dfs
[[nodiscard]] std::vector<std::size_t> dfs(std::size_t source) const {
    check_vertex(source);
    std::vector<bool> seen(vertices());
    std::vector<std::size_t> result;
    visit_depth_first(source, seen, result);
    return result;
}
```

```cpp file=code/ch07/graph/modern.hpp#bfs
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
```

拓扑排序：

```cpp file=code/ch07/graph/modern.hpp#topological
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
```

单源与全源最短路：

```cpp file=code/ch07/graph/modern.hpp#dijkstra
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
```

```cpp file=code/ch07/graph/modern.hpp#floyd
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
```

最小生成树：

```cpp file=code/ch07/graph/modern.hpp#prim
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
```

```cpp file=code/ch07/graph/modern.hpp#kruskal
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
```
