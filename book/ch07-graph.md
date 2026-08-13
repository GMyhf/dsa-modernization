# 第7章 图

图由顶点和边组成。有向边表示单向关系，无向边表示双向关系，带权边表示代价。先弄清题目属于哪一种，再选算法。

源码：[图与七个算法](../code/ch07/graph/modern.hpp)、
[可运行示例](../code/ch07/graph/demo.cpp)、
[交叉验证测试](../code/ch07/graph/test.cpp)。

## 7.1 图的定义和基本术语

图比线性结构和树更一般：结点之间的关系可以是任意的。按第 1 章的分类——线性结构唯一前驱、唯一后继；树唯一前驱、多个后继；图对前驱和后继的个数都不加限制。线性和树都可以看成受限的图。

一个典型问题：要在几个城市之间建通信网，使每两个城市都能直接或间接通话，并且总造价尽量低。用顶点代表城市，边代表线路，边旁的数代表造价，问题就变成：在带权图里找一棵连通全部顶点、边权之和最小的树。这就是后面的最小生成树。

图记作 $G=\langle V,E\rangle$。$V$ 是顶点的有穷非空集合；$E$ 是边的集合，每条边是一对顶点。边没有方向时叫无向图，无序对写成 $(v_1,v_2)$，$(v_1,v_2)$ 与 $(v_2,v_1)$ 是同一条边。边有方向时叫有向图，有序对写成 $\langle v_1,v_2\rangle$，也叫弧：$v_1$ 是弧尾（起点），$v_2$ 是弧头（终点）；$\langle v_1,v_2\rangle$ 与 $\langle v_2,v_1\rangle$ 是两条不同的弧。

边或弧上可以附带权，表示距离、时间或代价。每条边都带权的图叫带权图。本书不考虑多重边（同一对顶点之间多于一条边）和自环（顶点到自己的边）。

用 $n$ 表示顶点数，$e$ 表示边数。无向图 $e$ 的范围是 $0\sim n(n-1)/2$，有向图是 $0\sim n(n-1)$。边相对少的叫稀疏图，相对多的叫稠密图。任意两个顶点之间都有边的图叫完全图，它取到边数的上限。

若 $V'$ 是 $V$ 的子集，$E'$ 是 $E$ 的子集，且 $E'$ 里的边只连 $V'$ 里的顶点，则 $G'=\langle V',E'\rangle$ 是 $G$ 的子图。一条边所连的两个顶点互为邻接点；这条边与这两个顶点相关联。有向图里还要分清「邻接到」和「邻接自」。

顶点的度是与它相关联的边数。有向图再分成入度（指进来的弧）和出度（指出去的弧）。度为 0 的顶点是孤立点。

从 $u$ 出发、沿边走到 $v$ 的顶点序列叫路径；边数是路径长度。有向图必须顺着弧走。起点和终点相同、且至少含一条边的路径叫回路（环）。除端点外没有重复顶点的路径叫简单路径。

无向图中，若任意两点之间都有路径，称图连通；极大的连通子图叫连通分量。有向图中，若任意两点 $u$、$v$ 都存在 $u$ 到 $v$ 和 $v$ 到 $u$ 的有向路径，称强连通；只要求把有向边看成无向边后连通，叫弱连通。

无向连通图的生成树，是包含全部顶点、有 $n-1$ 条边、因而没有环的连通子图。带权连通图里边权之和最小的生成树，就是最小生成树。最短路的「短」是边权总和最小，不一定是经过的边数最少。

选算法之前，先分清题目：

| 题目 | 前提 | 结果 |
| --- | --- | --- |
| 从一个点能走到哪些点 | 任意图 | DFS 或 BFS 的访问序列 |
| 课程或任务的可行顺序 | 有向无环图 | 拓扑序；有环则无解 |
| 一个源点到各点的最短路 | 边权非负 | Dijkstra 的距离数组 |
| 任意两点最短路 | 顶点数较小 | Floyd 的距离矩阵 |
| 连通所有点且总权最小 | 无向连通图 | Prim 或 Kruskal 的边集 |

本单元用邻接矩阵。没有边记为 `infinity`（`int` 最大值的四分之一，给加法留余量）。环、非连通等正常失败用 `optional` 表达。DFS 仍是递归，深图有栈溢出风险。

## 7.2 图的抽象数据类型

图的对外运算是：指定顶点数构造、加一条边、问有多少个顶点，以及后面各节的周游、拓扑、最短路和最小生成树。原书【代码7.1】【代码7.2】的 ADT 声明残缺，标识符还被 OCR 空格切断。本书直接定义在 `Graph` 上。

## 7.3 图的存储结构

图常用两种存法。邻接矩阵用 $n\times n$ 的表，$A[i][j]$ 表示 $i$ 到 $j$ 有没有边、权是多少，适合稠密图和需要 $O(1)$ 问「两点之间有没有边」的算法（Floyd、Prim）。邻接表为每个顶点挂一条出边链表，适合稀疏图和只沿出边走的算法（DFS、BFS、Dijkstra、拓扑）。本章主实现是邻接矩阵；邻接表是同一组运算的另一种存储，不在这里另写一份未验证的实现。

### 7.3.1 相邻矩阵

邻接矩阵的第 `from` 行、第 `to` 列是边权。构造时对角线为 0（到自己的距离是 0），其余为 `infinity`。`add_edge(..., directed=false)` 同时写入对称位置，用来表示无向边。零权边可以表示——原书把 0 同时当成「无边」，那样权为 0 的边就存不进去。

## 7.4 图的周游

### 7.4.1 先跑一遍

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

### 7.4.2 深度优先与广度优先

DFS 进入一个顶点就立刻标记已访问，再沿编号从小到大的出边递归。BFS 用队列按层扩展，先到的顶点先出队。

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

### 7.4.3 拓扑排序

统计每个顶点的入度，把入度为 0 的点入队；每取出一个点，就把它指出的入度减 1。若最终取出的点数少于顶点数，图里有环。

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

## 7.5 最短路径

### 7.5.1 单源最短路径

Dijkstra 反复选出尚未确定的、当前距离最小的顶点，用它的出边松弛邻居。边权必须非负。

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

### 7.5.2 每对顶点之间的最短路径

Floyd 枚举中转点 `via`，比较 `from→to` 与 `from→via→to`。测试里五个源点的 Dijkstra 与 Floyd 逐项对拍。

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

## 7.6 最小生成树

目标不是任意两点最短，而是连通全部顶点且选中边的总权最小。

### 7.6.1 Prim 算法

从指定源点生长，每次把离当前树最近的顶点加进来。非连通则返回 `nullopt`。

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

### 7.6.2 Kruskal 算法

把边按权排序，用并查集跳过会形成环的边。连通图上与 Prim 得到相同总权、`n-1` 条边。

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

