"""树状数组：动态前缀和的 Python 实现。"""


# >>> fenwick
class FenwickTree:
    """公共接口 0 起始，内部数组 1 起始，区间使用 [left, right)。"""

    def __init__(self, size: int) -> None:
        if size < 0:
            raise ValueError("Fenwick size must be non-negative")
        self.values = [0] * size
        self.tree = [0] * (size + 1)

    @staticmethod
    def lowbit(index: int) -> int:
        return index & -index

    def add(self, index: int, delta: int) -> None:
        self._check_index(index)
        self.values[index] += delta
        cursor = index + 1
        while cursor <= self.size:
            self.tree[cursor] += delta
            cursor += self.lowbit(cursor)

    def set(self, index: int, value: int) -> None:
        self._check_index(index)
        self.add(index, value - self.values[index])

    def prefix_sum(self, end: int) -> int:
        if not 0 <= end <= self.size:
            raise IndexError("Fenwick prefix end out of range")
        result = 0
        cursor = end
        while cursor:
            result += self.tree[cursor]
            cursor -= self.lowbit(cursor)
        return result

    def range_sum(self, left: int, right: int) -> int:
        if left < 0 or left > right or right > self.size:
            raise IndexError("Fenwick range out of range")
        return self.prefix_sum(right) - self.prefix_sum(left)

    def value_at(self, index: int) -> int:
        self._check_index(index)
        return self.values[index]

    @property
    def size(self) -> int:
        return len(self.values)

    def _check_index(self, index: int) -> None:
        if not 0 <= index < self.size:
            raise IndexError("Fenwick index out of range")
# <<< fenwick
