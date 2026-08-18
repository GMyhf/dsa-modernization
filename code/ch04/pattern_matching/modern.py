"""字符串模式匹配的 Python 实现（D-025）。"""


# >>> naive
def naive_search(text: str, pattern: str) -> int | None:
    """朴素匹配：返回首次出现的 0 起始下标。"""
    if not pattern:
        return 0
    i = 0
    j = 0
    while i < len(pattern) and j < len(text):
        if text[j] == pattern[i]:
            i += 1
            j += 1
        else:
            j = j - i + 1
            i = 0
    return j - len(pattern) if i == len(pattern) else None
# <<< naive


# >>> build-next
def build_next(pattern: str) -> list[int]:
    """计算原书算法4.7的优化版 next 数组。"""
    if not pattern:
        return []
    next_values = [-1] * len(pattern)
    i = 0
    k = -1
    while i < len(pattern):
        while k >= 0 and pattern[i] != pattern[k]:
            k = next_values[k]
        i += 1
        k += 1
        if i == len(pattern):
            break
        next_values[i] = next_values[k] if pattern[i] == pattern[k] else k
    return next_values
# <<< build-next


# >>> kmp
def kmp_search(text: str, pattern: str, next_values: list[int] | None = None) -> int | None:
    """KMP 匹配；目标串下标只向前移动。"""
    if not pattern:
        return 0
    if next_values is None:
        next_values = build_next(pattern)
    if len(next_values) != len(pattern):
        raise ValueError("kmp_search: next 数组长度与模式不符")
    i = 0
    j = 0
    while i < len(pattern) and j < len(text):
        if i == -1 or text[j] == pattern[i]:
            i += 1
            j += 1
        else:
            i = next_values[i]
    return j - len(pattern) if i == len(pattern) else None
# <<< kmp
