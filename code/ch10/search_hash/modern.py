"""检索、集合与闭散列的 Python 实现（D-025）。"""

# >>> search-hash
class Item:
    def __init__(self, key): self._key = key
    def key(self): return self._key
    def set_key(self, key): self._key = key
# <<< search-hash

# >>> sequential-binary
def sequential_search(values, key):
    for i, value in enumerate(values):
        if value == key: return i
    return None

def binary_search(values, key):
    first, last = 0, len(values)
    while first < last:
        middle = first + (last - first) // 2
        if values[middle] == key: return middle
        if values[middle] < key: first = middle + 1
        else: last = middle
    return None
# <<< sequential-binary

# >>> int-set
class IntSet:
    def __init__(self): self._values = []
    def insert(self, value):
        if self.contains(value): return False
        self._values.append(value); return True
    def erase(self, value):
        found = sequential_search(self._values, value)
        if found is None: return False
        del self._values[found]; return True
    def contains(self, value): return sequential_search(self._values, value) is not None
    def intersection(self, other):
        result = IntSet()
        for value in self._values:
            if other.contains(value): result.insert(value)
        return result
    def includes(self, other): return all(self.contains(v) for v in other._values)
    def size(self): return len(self._values)
# <<< int-set

# >>> elf-hash
def elf_hash(text):
    value = 0
    for character in text:
        value = (value << 4) + ord(character)
        high = value & 0xF0000000
        if high: value ^= high >> 24
        value &= ~high
    return value
# <<< elf-hash

# >>> hash-table
class HashTable:
    def __init__(self, capacity):
        if capacity <= 0: raise ValueError("hash table capacity must be positive")
        self._slots = [None] * capacity; self._size = 0
    def _home(self, key): return abs(key) % len(self._slots)
    def insert(self, key):
        first_tombstone = None
        for step in range(len(self._slots)):
            i = (self._home(key) + step) % len(self._slots)
            if self._slots[i] == key: return False
            if self._slots[i] is _TOMBSTONE and first_tombstone is None:
                first_tombstone = i
            if self._slots[i] is None:
                target = first_tombstone if first_tombstone is not None else i
                self._slots[target] = key; self._size += 1; return True
        if first_tombstone is None: return False
        self._slots[first_tombstone] = key; self._size += 1; return True
    def contains(self, key): return self._find(key) is not None
    def erase(self, key):
        i = self._find(key)
        if i is None: return False
        self._slots[i] = _TOMBSTONE; self._size -= 1; return True
    def _find(self, key):
        for step in range(len(self._slots)):
            i = (self._home(key) + step) % len(self._slots)
            if self._slots[i] is None: return None
            if self._slots[i] == key: return i
        return None
    def size(self): return self._size
    def capacity(self): return len(self._slots)
    def slot_at(self, index):
        if index < 0 or index >= len(self._slots): raise IndexError("hash table slot")
        return self._slots[index]
# <<< hash-table

_TOMBSTONE = object()
