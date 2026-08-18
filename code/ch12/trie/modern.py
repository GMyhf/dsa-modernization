"""第 12 章 Trie 与 Patricia 的 Python 实现（D-025）。

两种结构讲的是同一件事的两端：Trie 每个字符一层，路径就是键；
Patricia 把「只有一个孩子」的那些层全部压掉，内部结点只记**该比第几位**。
所以 Patricia 的内部结点数恒等于键数减一，与键有多长无关——
这正是它相对 Trie 的全部收益，`test.py` 用 `probe_depth()` 把它钉住。

与 `modern.hpp` 策略同一份：Trie 用「孩子字典 + 经过计数」，
Patricia 用「降到叶子 → 求第一个不同的位 → 在正确的高度插入内部结点」。
"""


class _Node:
    """Trie 的结点：孩子表、是否是一个键的终点、经过它的键数。"""

    def __init__(self) -> None:
        self.children: dict[str, _Node] = {}
        self.terminal = False
        self.passing = 0


# >>> trie
class Trie:
    """12.4 Trie：每个字符一层，公共前缀共享同一条路径。"""

    def __init__(self) -> None:
        self.root = _Node()
        self._size = 0

    @staticmethod
    def _valid(word: str) -> None:
        if any(c < "a" or c > "z" for c in word):
            raise ValueError("trie key must be a..z")

    def insert(self, word: str) -> bool:
        self._valid(word)
        node = self.root
        path = []
        for c in word:
            path.append(node)
            node = node.children.setdefault(c, _Node())
        if node.terminal:
            return False
        node.terminal = True
        self._size += 1
        # passing 是「有多少个键经过这里」，count_with_prefix 靠它一次问答，
        # 而不是把子树数一遍。维护它的代价就落在这一行。
        for item in path:
            item.passing += 1
        node.passing += 1
        return True

    def contains(self, word: str) -> bool:
        node = self._find(word)
        return node is not None and node.terminal

    def starts_with(self, prefix: str) -> bool:
        return self._find(prefix) is not None

    def count_with_prefix(self, prefix: str) -> int:
        if prefix == "":
            return self._size
        node = self._find(prefix)
        return 0 if node is None else node.passing

    def _find(self, text: str) -> _Node | None:
        node = self.root
        for c in text:
            if c not in node.children:
                return None
            node = node.children[c]
        return node

    def size(self) -> int:
        return self._size

    def node_count(self) -> int:
        def count(node: _Node) -> int:
            return sum(1 + count(child) for child in node.children.values())

        return count(self.root)

    def keys_with_prefix(self, prefix: str) -> list[str]:
        node = self._find(prefix)
        out: list[str] = []
        if node is None:
            return out

        def collect(current: _Node, text: str) -> None:
            if current.terminal:
                out.append(text)
            # 按 a..z 的顺序下降，收集结果因此天然有序，不必再排一次。
            for code in range(ord("a"), ord("z") + 1):
                c = chr(code)
                if c in current.children:
                    collect(current.children[c], text + c)

        collect(node, prefix)
        return out

    def erase(self, word: str) -> bool:
        if not self.contains(word):
            return False
        node = self.root
        path = []
        for c in word:
            path.append((node, c))
            node = node.children[c]
        node.terminal = False
        node.passing -= 1
        self._size -= 1
        # 自底向上摘掉不再承载任何键的结点：passing 归零且没有孩子就该消失。
        for parent, c in reversed(path):
            parent.passing -= 1
            child = parent.children[c]
            if child.passing == 0 and not child.children:
                del parent.children[c]
        return True

    # >>> trie-longest-prefix
    def longest_prefix_of(self, text: str) -> str:
        """text 的哪个前缀是树里最长的那个键。走不动就回退到最近一次的词尾。"""
        node = self.root
        best = 0
        for i, c in enumerate(text):
            if c not in node.children:
                break
            node = node.children[c]
            if node.terminal:
                best = i + 1
        return text[:best]
    # <<< trie-longest-prefix
# <<< trie


# >>> patricia-bits
def bit_of(key: str, index: int) -> bool:
    """键的第 index 位（从最高位数起）。超出键长一律当 0——

    这一条让「一个键是另一个键的前缀」也能被分开：`"a"` 与 `"ab"` 在
    第 8 位之后就靠这个 0 区分。求首个不同位时把上界取到 `(最长+1)*8`，
    正是为了给这个「虚拟的结尾」留出位置。
    """
    byte = index // 8
    if byte >= len(key):
        return False
    return bool((ord(key[byte]) >> (7 - index % 8)) & 1)
# <<< patricia-bits


def _first_differing_bit(left: str, right: str) -> int | None:
    longest = max(len(left), len(right))
    for index in range((longest + 1) * 8):
        if bit_of(left, index) != bit_of(right, index):
            return index
    return None


class _PatriciaNode:
    """内部结点只记「比第几位」，叶子只记键。两者共用一个类型，靠 bit 区分。"""

    def __init__(self, bit: int | None, key: str | None) -> None:
        self.bit = bit
        self.key = key
        self.left: _PatriciaNode | None = None
        self.right: _PatriciaNode | None = None

    def is_internal(self) -> bool:
        return self.bit is not None


class PatriciaTree:
    """12.4 Patricia：把 Trie 里只有一个孩子的层全部压掉。

    代价是每个内部结点要记「跳到第几位」，收益是**内部结点数只与键数有关，
    与键长无关**——`internal_count()` 恒等于 `size() - 1`。
    """

    def __init__(self) -> None:
        self._root: _PatriciaNode | None = None
        self._size = 0
        self._internal = 0

    def _descend(self, key: str) -> _PatriciaNode | None:
        node = self._root
        while node is not None and node.is_internal():
            node = node.right if bit_of(key, node.bit) else node.left
        return node

    def insert(self, key: str) -> bool:
        if self._root is None:
            self._root = _PatriciaNode(None, key)
            self._size = 1
            return True
        leaf = self._descend(key)
        if leaf is not None and leaf.key == key:
            return False
        differing = _first_differing_bit(leaf.key, key)
        if differing is None:
            return False
        # 新的内部结点要插在「比它更早的位」之后、「比它更晚的位」之前，
        # 否则位序会乱，后续的降落会走错分支。
        parent = None
        node = self._root
        while node.is_internal() and node.bit < differing:
            parent = node
            node = node.right if bit_of(key, node.bit) else node.left
        fresh = _PatriciaNode(None, key)
        branch = _PatriciaNode(differing, None)
        if bit_of(key, differing):
            branch.left, branch.right = node, fresh
        else:
            branch.left, branch.right = fresh, node
        if parent is None:
            self._root = branch
        elif bit_of(key, parent.bit):
            parent.right = branch
        else:
            parent.left = branch
        self._size += 1
        self._internal += 1
        return True

    def contains(self, key: str) -> bool:
        leaf = self._descend(key)
        return leaf is not None and leaf.key == key

    def size(self) -> int:
        return self._size

    def internal_count(self) -> int:
        return self._internal

    def probe_depth(self, key: str) -> int:
        """降落到叶子经过了几个内部结点。这是真数出来的，不是按键长估的。"""
        depth = 0
        node = self._root
        while node is not None and node.is_internal():
            depth += 1
            node = node.right if bit_of(key, node.bit) else node.left
        return depth
