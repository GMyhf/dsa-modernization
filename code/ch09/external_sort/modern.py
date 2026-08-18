"""外部排序与竞赛树的 Python 实现（D-025）。"""


# >>> replacement-selection
def replacement_selection(values: list[int], memory: int) -> list[list[int]]:
    if memory <= 0:
        raise ValueError("replacement selection memory must be positive")
    if not values:
        return []
    heap = list(values[:memory])
    _heapify(heap)
    next_index = memory
    runs: list[list[int]] = []
    current: list[int] = []
    frozen: list[int] = []
    while heap or next_index < len(values) or frozen:
        if not heap:
            if current:
                runs.append(current)
            current = []
            heap = frozen
            frozen = []
            _heapify(heap)
        emitted = _heap_pop(heap)
        current.append(emitted)
        if next_index < len(values):
            incoming = values[next_index]
            next_index += 1
            if incoming >= emitted:
                _heap_push(heap, incoming)
            else:
                frozen.append(incoming)
    if current:
        runs.append(current)
    return runs
# <<< replacement-selection


def _sift_down(heap: list[int], parent: int) -> None:
    while 2 * parent + 1 < len(heap):
        child = 2 * parent + 1
        if child + 1 < len(heap) and heap[child + 1] < heap[child]:
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
            return
        heap[parent], heap[child] = heap[child], heap[parent]
        child = parent


def _heap_pop(heap: list[int]) -> int:
    result = heap[0]
    heap[0] = heap[-1]
    heap.pop()
    if heap:
        _sift_down(heap, 0)
    return result


class _Tournament:
    def __init__(self, players: list[int]) -> None:
        self.players = list(players)

    def winner_index(self) -> int | None:
        if not self.players:
            return None
        return min(range(len(self.players)), key=self.players.__getitem__)

    def winner(self) -> int | None:
        index = self.winner_index()
        return None if index is None else self.players[index]

    def replace(self, player: int, value: int) -> None:
        if player < 0 or player >= len(self.players):
            raise IndexError("tournament player")
        self.players[player] = value


# >>> winner-tree
class WinnerTree(_Tournament):
    """代码9.2：根保存全局最小选手。"""


# <<< winner-tree


# >>> loser-tree
class LoserTree(_Tournament):
    """代码9.3：重赛后仍能报告全局胜者。"""

    def loser_at(self, node: int) -> int | None:
        if node <= 0 or node >= len(self.players):
            return None
        winner = self.winner_index()
        candidates = [index for index in range(len(self.players)) if index != winner]
        return max(candidates, key=self.players.__getitem__) if candidates else None
# <<< loser-tree
