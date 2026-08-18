"""第 7 章图算法的 Python 断言测试（D-025）。

判据同 `test.cpp`：**若实现退回原书那一版，这里必须有断言变红。**
本文件里最该看的一条是 `test_dfs_and_bfs_are_distinguishable`——
原先那版用的样例图上 DFS 与 BFS 的输出恰好相同，于是把两个实现对调也不会红。
一条区分不了被测对象的断言，比没有断言更坏，因为它看起来像验过了。
"""

import sys
from pathlib import Path

import modern
sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases

checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


DIRECTED_EDGES = ((0, 1, 2), (0, 2, 7), (1, 2, 1), (1, 3, 5), (2, 3, 1), (3, 4, 3))
UNDIRECTED_EDGES = ((0, 1, 1), (0, 2, 4), (1, 2, 2), (1, 3, 5), (2, 3, 1), (3, 4, 3))


def directed() -> modern.Graph:
    graph = modern.Graph(5)
    for source, target, weight in DIRECTED_EDGES:
        graph.add_edge(source, target, weight)
    return graph


def undirected() -> modern.Graph:
    graph = modern.Graph(5)
    for source, target, weight in UNDIRECTED_EDGES:
        graph.add_edge(source, target, weight, False)
    return graph


def test_graph_and_edge() -> None:
    graph = directed()
    check(graph.vertices == 5, "代码7.1 vertex count")
    check(modern.Graph(0).vertices == 0, "代码7.1 零顶点图合法")
    raised = False
    try:
        modern.Graph(-1)
    except ValueError:
        raised = True
    check(raised, "代码7.1 顶点数不能为负")
    raised = False
    try:
        graph.dfs(99)
    except (IndexError, ValueError):
        raised = True
    check(raised, "代码7.1 越界顶点被拒绝")

    edge = modern.Edge(1, 2, 7)
    check((edge.source, edge.target, edge.weight) == (1, 2, 7), "代码7.2 Edge 三个字段")
    check(modern.Edge(1, 2, 7) == modern.Edge(1, 2, 7), "代码7.2 Edge 带权且可比较")


def test_add_edge() -> None:
    graph = modern.Graph(3)
    graph.add_edge(0, 1, 4)
    check(graph.dfs(0) == [0, 1] and graph.dfs(1) == [1], "代码7.3 有向边只通一个方向")
    graph.add_edge(1, 2, 4, False)
    check(2 in graph.dfs(1) and 1 in graph.dfs(2), "代码7.3 无向边两个方向都通")
    raised = False
    try:
        graph.add_edge(0, 1, -1)
    except ValueError:
        raised = True
    check(raised, "代码7.3 rejects negative Dijkstra weight")


def test_dfs_and_bfs_are_distinguishable() -> None:
    """样例图必须让 DFS 与 BFS 的输出**不同**，否则两条断言都是摆设。

    0→1→2 与 0→3：深度优先先把 1-2 那条路走到底，广度优先先把 0 的邻居铺完。
    """
    graph = modern.Graph(4)
    for source, target in ((0, 1), (1, 2), (0, 3)):
        graph.add_edge(source, target, 1)
    depth_first = graph.dfs(0)
    breadth_first = graph.bfs(0)
    check(depth_first == [0, 1, 2, 3], "算法7.5 DFS 先走到底")
    check(breadth_first == [0, 1, 3, 2], "算法7.6 BFS 先铺一层")
    check(depth_first != breadth_first, "算法7.5/7.6 两种遍历在本样例上确实不同")

    # 不连通的部分不该被访问到——起点走得到哪儿就是哪儿。
    split = modern.Graph(4)
    split.add_edge(0, 1, 1)
    split.add_edge(2, 3, 1)
    check(split.dfs(0) == [0, 1], "算法7.5 DFS 不跨越不连通的部分")
    check(split.bfs(0) == [0, 1], "算法7.6 BFS 不跨越不连通的部分")


def test_topological_sort() -> None:
    graph = directed()
    order = graph.topological_sort()
    check(order is not None, "算法7.7 有向无环图能排出拓扑序")
    check(order is not None and sorted(order) == list(range(5)), "算法7.7 每个顶点恰好一次")
    # 只验首尾是不够的：中间接反了照样通过。逐条边验「源在前、汇在后」。
    position = {vertex: index for index, vertex in enumerate(order or [])}
    respects = all(position[source] < position[target]
                   for source, target, _ in DIRECTED_EDGES)
    check(respects, "算法7.7 拓扑序尊重每一条边的方向")

    cycle = modern.Graph(3)
    cycle.add_edge(0, 1, 1)
    cycle.add_edge(1, 2, 1)
    cycle.add_edge(2, 0, 1)
    check(cycle.topological_sort() is None, "算法7.7 cycle returns None")


def test_shortest_paths() -> None:
    graph = directed()
    distance = graph.dijkstra(0)
    # 逐个写死期望值，而不是只跟 Floyd 对——两个都写错的话互相对照也发现不了。
    check(distance[0] == 0, "算法7.8 起点到自己是 0")
    check(distance[1] == 2, "算法7.8 0→1 直达")
    check(distance[2] == 3, "算法7.8 0→1→2 比直达的 7 更短")
    check(distance[3] == 4, "算法7.8 0→1→2→3")
    check(distance[4] == 7, "算法7.8 0→…→4")

    floyd = graph.floyd()
    agree = all(graph.dijkstra(source) == floyd[source] for source in range(graph.vertices))
    check(agree, "算法7.8 Dijkstra matches 算法7.9 Floyd")
    check(floyd[0][4] == 7, "算法7.9 uses intermediate vertices")

    lonely = modern.Graph(3)
    lonely.add_edge(0, 1, 5)
    check(lonely.dijkstra(0)[2] == modern.Graph.infinity, "算法7.8 到不了的顶点是无穷")
    check(lonely.floyd()[0][2] == modern.Graph.infinity, "算法7.9 到不了的顶点是无穷")


def minimum_spanning_weight() -> int:
    """穷举所有 4 条边的组合，作为独立裁判验 MST 的总权重。"""
    import itertools

    best = None
    for combo in itertools.combinations(UNDIRECTED_EDGES, 4):
        parent = list(range(5))

        def root(vertex: int) -> int:
            while parent[vertex] != vertex:
                vertex = parent[vertex]
            return vertex

        ok = True
        for source, target, _ in combo:
            a, b = root(source), root(target)
            if a == b:
                ok = False
                break
            parent[a] = b
        if ok:
            total = sum(weight for _, _, weight in combo)
            best = total if best is None else min(best, total)
    return best if best is not None else 0


def test_minimum_spanning_tree() -> None:
    graph = undirected()
    prim = graph.prim(0)
    kruskal = graph.kruskal()
    optimum = minimum_spanning_weight()
    check(prim is not None and len(prim) == 4, "算法7.10 Prim has n-1 edges")
    check(kruskal is not None and len(kruskal) == 4, "算法7.11 Kruskal has n-1 edges")
    check(sum(edge.weight for edge in prim or []) == optimum,
          "算法7.10 Prim 的总权重等于穷举出的最小值")
    check(sum(edge.weight for edge in kruskal or []) == optimum,
          "算法7.11 Kruskal 的总权重等于穷举出的最小值")
    # 两个算法可以选出不同的边集，但总权重必须相同——这一条比「都等于 7」更结实。
    check(sum(edge.weight for edge in prim or []) ==
          sum(edge.weight for edge in kruskal or []),
          "算法7.10/7.11 两种算法总权重一致")

    disconnected = modern.Graph(3)
    disconnected.add_edge(0, 1, 1, False)
    check(disconnected.prim(0) is None, "算法7.10 disconnected None")
    check(disconnected.kruskal() is None, "算法7.11 disconnected None")


def main() -> int:
    test_graph_and_edge()
    test_add_edge()
    test_dfs_and_bfs_are_distinguishable()
    test_topological_sort()
    test_shortest_paths()
    test_minimum_spanning_tree()
    shared = shared_cases.load()
    for case in shared:
        if case.expected_error == "invalid_argument":
            # 这一行的输入**必须来自表**。原先这里写死了 add_edge(0, 1, -1)，
            # 而 C++ 侧读的是 case.input——同一张表，两边跑的却不是同一件事，
            # 改表里的边也不会有任何反应。共享用例的意义就在这一行上。
            source, target, weight = shared_cases.integers(case.input)
            graph = modern.Graph(max(source, target) + 1)
            raised = False
            try:
                graph.add_edge(source, target, weight)
            except ValueError:
                raised = True
            check(raised, f"T-047 {case.name} exception")
        else:
            count, edges = case.input.split("|", 1)
            graph = modern.Graph(int(count))
            for edge in edges.split(";"):
                source, target, weight = shared_cases.integers(edge)
                graph.add_edge(source, target, weight)
            check(graph.dijkstra(0) == shared_cases.integers(case.expected),
                  f"T-047 {case.name} distances")
    print(f"共享用例: {len(shared)}")
    print(f"Graph(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
