"""第 9 章外部排序的 Python 实现（D-025）。

置换选择产生初始顺串，胜者树 / 败者树在多路归并里选出全局最小的那一路。
与 `modern.hpp` 是同一批算法的两种实现，策略同一份：置换选择用一个最小堆加
一个「冻结区」，竞赛树把选手放在叶子上、内部结点记比赛结果、
替换一名选手后**只沿叶到根的一条路径重赛**。

最后那句是本单元的全部意义，也是唯一值得机器守的东西：
每次替换只比较 $O(\\log n)$ 次，而不是把 $n$ 路重新扫一遍。
`comparisons()` 把这件事变成可断言的量，`test.py` 里有断言钉着——
退化成线性扫描时它会红。
"""

# 空的归并段用 +∞ 占位：某一路读完之后它就永远输，不必把树重建成另一个形状。
# 这不是为了把选手数凑成 2 的幂，而是外排序里段读完时本来就要这么处理。
INFINITY = float("inf")


# >>> replacement-selection
def replacement_selection(values: list[int], memory: int) -> list[list[int]]:
    """算法9.1：置换选择。内存里放得下 memory 个记录，产生长度可超过 memory 的顺串。"""
    if memory <= 0:
        raise ValueError("memory must be positive")
    runs: list[list[int]] = []
    heap: list[int] = []
    frozen: list[int] = []
    source = list(values)
    cursor = 0
    while cursor < len(source) and len(heap) < memory:
        heap.append(source[cursor])
        cursor += 1
    _heapify(heap)
    current: list[int] = []
    while heap or frozen:
        if not heap:
            # 工作区空了：这一趟顺串到此为止，冻结区整体解冻成下一趟的工作区。
            runs.append(current)
            current = []
            heap, frozen = frozen, []
            _heapify(heap)
            continue
        smallest = _heap_pop(heap)
        current.append(smallest)
        if cursor < len(source):
            nxt = source[cursor]
            cursor += 1
            if nxt < smallest:
                frozen.append(nxt)  # 比刚输出的还小，进不了本趟顺串
            else:
                _heap_push(heap, nxt)
    if current:
        runs.append(current)
    return runs
# <<< replacement-selection


def _sift_down(heap: list[int], parent: int) -> None:
    count = len(heap)
    while parent * 2 + 1 < count:
        child = parent * 2 + 1
        if child + 1 < count and heap[child + 1] < heap[child]:
            child += 1
        if heap[parent] <= heap[child]:
            return
        heap[parent], heap[child] = heap[child], heap[parent]
        parent = child


def _heapify(heap: list[int]) -> None:
    for parent in range(len(heap) // 2 - 1, -1, -1):
        _sift_down(heap, parent)


def _heap_push(heap: list[int], value: int) -> None:
    heap.append(value)
    child = len(heap) - 1
    while child > 0:
        parent = (child - 1) // 2
        if heap[parent] <= heap[child]:
            break
        heap[parent], heap[child] = heap[child], heap[parent]
        child = parent


def _heap_pop(heap: list[int]) -> int:
    top = heap[0]
    last = heap.pop()
    if heap:
        heap[0] = last
        _sift_down(heap, 0)
    return top


def _next_power_of_two(count: int) -> int:
    size = 1
    while size < count:
        size *= 2
    return size


class _Tournament:
    """胜者树与败者树共用的骨架：叶子放选手，内部结点放比赛结果。

    叶子数补齐到 2 的幂，补出来的位置用 `INFINITY` 当选手——那是「这一路已经读完」。
    两棵树的差别只在内部结点**记谁**：胜者树记赢家，败者树记输家。
    """

    def __init__(self, players: list[int]) -> None:
        self.players = list(players)
        self._comparisons = 0
        self._size = _next_power_of_two(len(self.players)) if self.players else 0
        self._tree = [0] * (2 * self._size)
        if self.players:
            self._build()

    def _key(self, index: int):
        return INFINITY if index >= len(self.players) else self.players[index]

    def _better(self, left: int, right: int) -> int:
        """一场比赛。计数就发生在这里——比较次数是本单元要讲的那个量。"""
        self._comparisons += 1
        return left if self._key(left) <= self._key(right) else right

    def _build(self) -> None:
        raise NotImplementedError

    def winner_index(self) -> int | None:
        raise NotImplementedError

    def winner(self) -> int | None:
        index = self.winner_index()
        return None if index is None else self.players[index]

    def replace(self, player: int, value: int) -> None:
        raise NotImplementedError

    def comparisons(self) -> int:
        return self._comparisons

    def reset_comparisons(self) -> None:
        self._comparisons = 0

    def _check_player(self, player: int) -> None:
        if player < 0 or player >= len(self.players):
            raise IndexError("tournament player")


# >>> winner-tree
class WinnerTree(_Tournament):
    """代码9.2：内部结点记**赢家**，根就是全局最小的那一路。

    重建一个结点要看它两个孩子的赢家，所以替换选手后沿路每层各比一次。
    """

    def _winner_at(self, node: int) -> int:
        # 叶子层不占内部结点的位置：第 j 个选手就在 _size + j 上，它自己是自己的赢家。
        return node - self._size if node >= self._size else self._tree[node]

    def _build(self) -> None:
        for node in range(self._size - 1, 0, -1):
            self._tree[node] = self._better(self._winner_at(node * 2),
                                            self._winner_at(node * 2 + 1))

    def winner_index(self) -> int | None:
        if not self.players:
            return None
        return self._winner_at(1)

    def replace(self, player: int, value: int) -> None:
        self._check_player(player)
        self.players[player] = value
        node = (self._size + player) // 2
        while node >= 1:
            self._tree[node] = self._better(self._winner_at(node * 2),
                                            self._winner_at(node * 2 + 1))
            node //= 2
# <<< winner-tree


# >>> loser-tree
class LoserTree(_Tournament):
    """代码9.3：内部结点记**输家**，另用 `_champion` 记全局胜者。

    与胜者树同一套重赛路径，差别只在留下什么痕迹：胜者树只留赢家，
    败者树把每一场的输家也记在结点上。外排序要这份痕迹——
    `loser_at(node)` 就是「这一路是在哪一层、被谁淘汰的」，
    而胜者树把这件事丢掉了。

    两个数组一起维护（`_subtree_winner` 与 `_loser`），所以替换**任意**一片叶子
    都成立，不只是替换当前冠军。只留输家数组的写法看着更省，
    但那样只有「替换冠军」这一种用法是对的——k 路归并恰好只用那一种，
    于是错误可以长期不被发现。这里不取那条捷径。
    """

    def __init__(self, players: list[int]) -> None:
        self._loser: list[int | None] = []
        self._subtree_winner: list[int] = []
        self._champion: int | None = None
        super().__init__(players)

    def _match(self, left: int, right: int) -> tuple[int, int]:
        """一场比赛，返回 (赢家, 输家)。一次比较，两个结果。"""
        winner = self._better(left, right)
        return winner, (right if winner == left else left)

    def _build(self) -> None:
        self._loser = [None] * self._size
        self._subtree_winner = [0] * (self._size * 2)
        for index in range(self._size):
            self._subtree_winner[self._size + index] = index
        for node in range(self._size - 1, 0, -1):
            self._replay_node(node)
        self._champion = self._subtree_winner[1]

    def _replay_node(self, node: int) -> None:
        winner, loser = self._match(self._subtree_winner[node * 2],
                                    self._subtree_winner[node * 2 + 1])
        self._loser[node] = loser
        self._subtree_winner[node] = winner

    def winner_index(self) -> int | None:
        if not self.players:
            return None
        return self._champion

    def replace(self, player: int, value: int) -> None:
        self._check_player(player)
        self.players[player] = value
        node = (self._size + player) // 2
        while node >= 1:
            self._replay_node(node)
            node //= 2
        self._champion = self._subtree_winner[1]

    def loser_at(self, node: int) -> int | None:
        """第 node 个内部结点上记着的输家。越界返回 None。"""
        if node <= 0 or node >= self._size:
            return None
        return self._loser[node]
# <<< loser-tree
