"""邻接矩阵图与图算法的 Python 实现（D-025）。"""

from dataclasses import dataclass


# >>> graph
@dataclass(frozen=True)
class Edge:
    """带权边。"""

    source: int
    target: int
    weight: int


class Graph:
    """邻接矩阵图；不存在的边以 infinity 表示。"""

    infinity = (2**31 - 1) // 4

    # >>> graph-build
    def __init__(self, count: int) -> None:
        if count < 0:
            raise ValueError("negative vertex count")
        self._adjacency = [[self.infinity] * count for _ in range(count)]
        for vertex in range(count):
            self._adjacency[vertex][vertex] = 0

    @property
    def vertices(self) -> int:
        return len(self._adjacency)

    def add_edge(self, source: int, target: int, weight: int, directed: bool = True) -> None:
        self._check_vertex(source)
        self._check_vertex(target)
        if weight < 0:
            raise ValueError("negative edge")
        self._adjacency[source][target] = weight
        if not directed:
            self._adjacency[target][source] = weight
    # <<< graph-build

    # >>> dfs
    def dfs(self, source: int) -> list[int]:
        self._check_vertex(source)
        seen = [False] * self.vertices
        result: list[int] = []

        def visit(vertex: int) -> None:
            seen[vertex] = True
            result.append(vertex)
            for target in range(self.vertices):
                if self._adjacency[vertex][target] < self.infinity and not seen[target]:
                    visit(target)

        visit(source)
        return result
    # <<< dfs

    # >>> bfs
    def bfs(self, source: int) -> list[int]:
        self._check_vertex(source)
        seen = [False] * self.vertices
        queue = [source]
        head = 0
        seen[source] = True
        result: list[int] = []
        while head < len(queue):
            vertex = queue[head]
            head += 1
            result.append(vertex)
            for target in range(self.vertices):
                if self._adjacency[vertex][target] < self.infinity and not seen[target]:
                    seen[target] = True
                    queue.append(target)
        return result
    # <<< bfs

    # >>> topological
    def topological_sort(self) -> list[int] | None:
        indegree = [0] * self.vertices
        for source in range(self.vertices):
            for target in range(self.vertices):
                if source != target and self._adjacency[source][target] < self.infinity:
                    indegree[target] += 1
        queue = [vertex for vertex in range(self.vertices) if indegree[vertex] == 0]
        head = 0
        result: list[int] = []
        while head < len(queue):
            source = queue[head]
            head += 1
            result.append(source)
            for target in range(self.vertices):
                if source != target and self._adjacency[source][target] < self.infinity:
                    indegree[target] -= 1
                    if indegree[target] == 0:
                        queue.append(target)
        return result if len(result) == self.vertices else None
    # <<< topological

    # >>> dijkstra
    def dijkstra(self, source: int) -> list[int]:
        self._check_vertex(source)
        distance = [self.infinity] * self.vertices
        used = [False] * self.vertices
        distance[source] = 0
        for _ in range(self.vertices):
            vertex = self._nearest(distance, used)
            if vertex is None or distance[vertex] == self.infinity:
                break
            used[vertex] = True
            for target in range(self.vertices):
                candidate = distance[vertex] + self._adjacency[vertex][target]
                if self._adjacency[vertex][target] < self.infinity and candidate < distance[target]:
                    distance[target] = candidate
        return distance
    # <<< dijkstra

    # >>> floyd
    def floyd(self) -> list[list[int]]:
        distance = [list(row) for row in self._adjacency]
        for via in range(self.vertices):
            for source in range(self.vertices):
                for target in range(self.vertices):
                    candidate = distance[source][via] + distance[via][target]
                    if distance[source][via] < self.infinity and distance[via][target] < self.infinity:
                        distance[source][target] = min(distance[source][target], candidate)
        return distance
    # <<< floyd

    # >>> prim
    def prim(self, source: int) -> list[Edge] | None:
        self._check_vertex(source)
        distance = [self.infinity] * self.vertices
        predecessor = [0] * self.vertices
        used = [False] * self.vertices
        result: list[Edge] = []
        distance[source] = 0
        for _ in range(self.vertices):
            vertex = self._nearest(distance, used)
            if vertex is None or distance[vertex] == self.infinity:
                return None
            used[vertex] = True
            if vertex != source:
                result.append(Edge(predecessor[vertex], vertex, distance[vertex]))
            for target in range(self.vertices):
                if not used[target] and self._adjacency[vertex][target] < distance[target]:
                    distance[target] = self._adjacency[vertex][target]
                    predecessor[target] = vertex
        return result
    # <<< prim

    # >>> kruskal
    def kruskal(self) -> list[Edge] | None:
        edges = [Edge(source, target, self._adjacency[source][target])
                 for source in range(self.vertices) for target in range(source + 1, self.vertices)
                 if self._adjacency[source][target] < self.infinity]
        for end in range(len(edges) - 1, 0, -1):
            for index in range(end):
                if edges[index].weight > edges[index + 1].weight:
                    edges[index], edges[index + 1] = edges[index + 1], edges[index]
        parent = list(range(self.vertices))

        def root(vertex: int) -> int:
            while parent[vertex] != vertex:
                parent[vertex] = parent[parent[vertex]]
                vertex = parent[vertex]
            return vertex

        result: list[Edge] = []
        for edge in edges:
            left = root(edge.source)
            right = root(edge.target)
            if left != right:
                parent[left] = right
                result.append(edge)
        return result if len(result) + 1 == self.vertices else None
    # <<< kruskal

    def _nearest(self, distance: list[int], used: list[bool]) -> int | None:
        nearest = None
        for vertex in range(self.vertices):
            if not used[vertex] and (nearest is None or distance[vertex] < distance[nearest]):
                nearest = vertex
        return nearest

    def _check_vertex(self, vertex: int) -> None:
        if vertex < 0 or vertex >= self.vertices:
            raise IndexError("vertex")
# <<< graph
