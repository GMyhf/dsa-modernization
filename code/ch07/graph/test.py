"""第 7 章图算法 Python 断言。"""

import modern

checks = 0


def check(condition: bool, name: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(name)


def directed() -> modern.Graph:
    graph = modern.Graph(5)
    for source, target, weight in ((0, 1, 2), (0, 2, 7), (1, 2, 1),
                                   (1, 3, 5), (2, 3, 1), (3, 4, 3)):
        graph.add_edge(source, target, weight)
    return graph


graph = directed()
check(graph.vertices == 5, "代码7.1 vertex count")
check(graph.dfs(0) == [0, 1, 2, 3, 4], "算法7.5 DFS order")
check(graph.bfs(0) == [0, 1, 2, 3, 4], "算法7.6 BFS order")
topological = graph.topological_sort()
check(topological is not None and topological[0] == 0 and topological[-1] == 4,
      "算法7.7 topological endpoints")
floyd = graph.floyd()
for source in range(graph.vertices):
    check(graph.dijkstra(source) == floyd[source], "算法7.8 Dijkstra matches 算法7.9 Floyd")
check(floyd[0][4] == 7, "算法7.9 uses intermediate vertices")

undirected = modern.Graph(5)
for source, target, weight in ((0, 1, 1), (0, 2, 4), (1, 2, 2),
                               (1, 3, 5), (2, 3, 1), (3, 4, 3)):
    undirected.add_edge(source, target, weight, False)
prim = undirected.prim(0)
kruskal = undirected.kruskal()
check(prim is not None and len(prim) == 4, "算法7.10 Prim has n-1 edges")
check(kruskal is not None and len(kruskal) == 4, "算法7.11 Kruskal has n-1 edges")
check(sum(edge.weight for edge in prim or []) == 7, "算法7.10 Prim weight")
check(sum(edge.weight for edge in kruskal or []) == 7, "算法7.11 Kruskal weight")

cycle = modern.Graph(3)
cycle.add_edge(0, 1, 1)
cycle.add_edge(1, 2, 1)
cycle.add_edge(2, 0, 1)
check(cycle.topological_sort() is None, "算法7.7 cycle returns None")
disconnected = modern.Graph(3)
disconnected.add_edge(0, 1, 1, False)
check(disconnected.prim(0) is None, "算法7.10 disconnected None")
check(disconnected.kruskal() is None, "算法7.11 disconnected None")
raised = False
try:
    graph.add_edge(0, 1, -1)
except ValueError:
    raised = True
check(raised, "代码7.3 rejects negative Dijkstra weight")
print(f"{checks} 项断言")
