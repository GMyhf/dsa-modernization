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


# >>> border-lengths
def border_lengths(s: str) -> list[int]:
    """未优化的失效函数：border[i] 是前缀 s[0..i] 的最长真边界长度。

    求周期不能拿 build_next 代替：优化版 next[i] = next[k] 不再是边界长度，
    例如 "aaaa" 的优化版 next 是 [-1, -1, -1, -1]，边界长度是 [0, 1, 2, 3]。
    """
    border = [0] * len(s)
    for i in range(1, len(s)):
        k = border[i - 1]
        while k > 0 and s[i] != s[k]:
            k = border[k - 1]
        if s[i] == s[k]:
            k += 1
        border[i] = k
    return border
# <<< border-lengths


# >>> minimal-period
def minimal_period(s: str) -> int:
    """最小周期 p = n - 整串的最长真边界长度；空串约定返回 0。"""
    if not s:
        return 0
    return len(s) - border_lengths(s)[-1]


def is_repetition(s: str) -> bool:
    """s 能否写成更短的串重复至少两次。

    p < n 不能省：边界为 0 时 p == n，而 n % n == 0 恒成立。
    """
    p = minimal_period(s)
    return p < len(s) and len(s) % p == 0


def repetition_count(s: str) -> int:
    """最大的 K，使 s 是某个串重复 K 次；不是循环串时为 1，空串为 0。"""
    if not s:
        return 0
    return len(s) // minimal_period(s) if is_repetition(s) else 1
# <<< minimal-period
