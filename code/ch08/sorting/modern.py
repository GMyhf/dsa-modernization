"""第 8 章内部排序的 Python 实现（D-025）。

这份文件与 `modern.hpp` 是**同一批算法的两种实现**，不是把 C++ 逐行翻译过来。
写它的目的只有一个：让读者看见**算法与语言的分界在哪里**——

  - 策略是同一份：8.1 的哨兵挪位、8.4 的手写筛选、8.6 的末元素划分、
    8.7 的「短侧递归 + 小分区转插入」、8.8 的两路归并、8.11 的 LSD 分趟；
  - 代价不是同一份：定长整数、递归深度、下标越界的行为，三者在两种语言里
    的结论**相反**，具体差在哪写在各函数的注释里，并且每一条都有断言守着。

按 D-025 §2，本文件不得出现 `sorted` / `list.sort` / `heapq` / `bisect`——
那些调用一行就是本章全章。`tools/check_code.py` 的 `check_d025` 用 AST 查这件事。
"""

# >>> sorting

# >>> insertion
# 算法8.1：直接插入排序。相等元素不越过彼此，故稳定。
def insertion_sort(values: list[int]) -> None:
    for index in range(1, len(values)):
        value = values[index]
        hole = index
        # `hole > 0` 这个条件在 Python 里是**承重的**，不是防御性写法：
        # C++ 版越界会被 ASan 当场抓住，Python 的 values[-1] 却合法——
        # 它悄悄环绕到最后一个元素，把排序结果搅乱而不报任何错。
        while hole > 0 and value < values[hole - 1]:
            values[hole] = values[hole - 1]
            hole -= 1
        values[hole] = value
# <<< insertion


# 算法8.2：增量每次减半的 Shell 排序。
def shell_sort(values: list[int]) -> None:
    gap = len(values) // 2
    while gap > 0:
        for index in range(gap, len(values)):
            value = values[index]
            hole = index
            while hole >= gap and value < values[hole - gap]:
                values[hole] = values[hole - gap]
                hole -= gap
            values[hole] = value
        gap //= 2


# 算法8.3：直接选择排序。每趟只做一次交换，写次数 O(n)。
def selection_sort(values: list[int]) -> None:
    for first in range(len(values)):
        minimum = first
        for index in range(first + 1, len(values)):
            if values[index] < values[minimum]:
                minimum = index
        values[first], values[minimum] = values[minimum], values[first]


# >>> heap
# 算法8.4：手写最大堆筛选与堆排序，不借 heapq——那一行就是 5.5 节全节。
def sift_down(values: list[int], root: int, count: int) -> None:
    while root * 2 + 1 < count:
        child = root * 2 + 1
        if child + 1 < count and values[child] < values[child + 1]:
            child += 1
        if values[root] >= values[child]:
            return
        values[root], values[child] = values[child], values[root]
        root = child


def heap_sort(values: list[int]) -> None:
    for root in range(len(values) // 2 - 1, -1, -1):
        sift_down(values, root, len(values))
    for end in range(len(values) - 1, 0, -1):
        values[0], values[end] = values[end], values[0]
        sift_down(values, 0, end)
# <<< heap


# 算法8.5：带「本趟无交换即结束」优化的冒泡排序。
def bubble_sort(values: list[int]) -> None:
    for end in range(len(values), 1, -1):
        changed = False
        for index in range(1, end):
            if values[index] < values[index - 1]:
                values[index], values[index - 1] = values[index - 1], values[index]
                changed = True
        if not changed:
            return


# >>> quick
def partition(values: list[int], first: int, last: int) -> int:
    """以 [first, last) 的末元素为轴划分，返回轴的落点。"""
    pivot = values[last - 1]
    boundary = first
    for index in range(first, last - 1):
        if values[index] < pivot:
            values[boundary], values[index] = values[index], values[boundary]
            boundary += 1
    values[boundary], values[last - 1] = values[last - 1], values[boundary]
    return boundary


def quick_sort_range(values: list[int], first: int, last: int) -> None:
    if last - first < 2:
        return
    middle = partition(values, first, last)
    quick_sort_range(values, first, middle)
    quick_sort_range(values, middle + 1, last)


# 算法8.6：手写快排。
#
# **这一版在 Python 里会真的崩，而在 C++ 里只是慢**：末元素当轴，遇到已排好序的
# 输入就退化成 n 层递归。C++ 有 8 MB 栈，几万层才炸；CPython 的递归上限默认是
# 1000 层，一千个有序元素就抛 RecursionError。同一个算法缺陷，两种语言的暴露
# 阈值差两个数量级——8.7 的「短侧递归」因此在 Python 里不是优化，是能不能跑。
# test.py 里有断言把这两件事都钉住。
def quick_sort(values: list[int]) -> None:
    quick_sort_range(values, 0, len(values))
# <<< quick


SMALL_RANGE = 16


# 算法8.7：小分区转插入排序、优先递归短侧以限制栈深。
def quick_sort_optimized_range(values: list[int], first: int, last: int) -> None:
    while last - first > SMALL_RANGE:
        middle = partition(values, first, last)
        # 只对短的那侧递归，长的那侧改用循环——递归深度因此是 O(log n)，
        # 与输入是否有序无关。这正是上面 quick_sort 缺的那一句。
        if middle - first < last - middle - 1:
            quick_sort_optimized_range(values, first, middle)
            first = middle + 1
        else:
            quick_sort_optimized_range(values, middle + 1, last)
            last = middle
    for index in range(first + 1, last):
        value = values[index]
        hole = index
        while hole > first and value < values[hole - 1]:
            values[hole] = values[hole - 1]
            hole -= 1
        values[hole] = value


def quick_sort_optimized(values: list[int]) -> None:
    quick_sort_optimized_range(values, 0, len(values))


# >>> merge
def merge_ranges(values: list[int], buffer: list[int],
                 first: int, middle: int, last: int) -> None:
    left, right, output = first, middle, first
    while left < middle and right < last:
        # `<` 而不是 `<=`：相等时取左边，稳定性就是从这一个符号来的。
        if values[right] < values[left]:
            buffer[output] = values[right]
            right += 1
        else:
            buffer[output] = values[left]
            left += 1
        output += 1
    while left < middle:
        buffer[output] = values[left]
        left += 1
        output += 1
    while right < last:
        buffer[output] = values[right]
        right += 1
        output += 1
    values[first:last] = buffer[first:last]


def merge_sort_range(values: list[int], buffer: list[int], first: int, last: int) -> None:
    if last - first < 2:
        return
    middle = first + (last - first) // 2
    merge_sort_range(values, buffer, first, middle)
    merge_sort_range(values, buffer, middle, last)
    merge_ranges(values, buffer, first, middle, last)


# 算法8.8：两路归并排序。辅助空间 O(n)，一次开够，不在递归里反复申请。
def merge_sort(values: list[int]) -> None:
    buffer = [0] * len(values)
    merge_sort_range(values, buffer, 0, len(values))
# <<< merge


# 算法8.9：已有序时跳过 merge；小分区改用插入排序。
def merge_sort_optimized_range(values: list[int], buffer: list[int],
                               first: int, last: int) -> None:
    if last - first <= SMALL_RANGE:
        for index in range(first + 1, last):
            value = values[index]
            hole = index
            while hole > first and value < values[hole - 1]:
                values[hole] = values[hole - 1]
                hole -= 1
            values[hole] = value
        return
    middle = first + (last - first) // 2
    merge_sort_optimized_range(values, buffer, first, middle)
    merge_sort_optimized_range(values, buffer, middle, last)
    # 左段末尾已经不大于右段开头，两段拼起来就是有序的，这一趟归并可以整个省掉。
    if values[middle] < values[middle - 1]:
        merge_ranges(values, buffer, first, middle, last)


def merge_sort_optimized(values: list[int]) -> None:
    buffer = [0] * len(values)
    merge_sort_optimized_range(values, buffer, 0, len(values))


COUNTING_RANGE_LIMIT = 10_000_000


# 算法8.10：桶式（计数）排序，支持负数但不适合巨大稀疏值域。
#
# 与 C++ 版的一处**实质**差别：C++ 里 `high - low + 1` 本身就可能溢出 int，
# 那一版必须先转成 long long 再算。Python 的整数没有宽度，这个坑不存在——
# 但值域上限的检查一条都不能少，它挡的是内存而不是溢出。
def counting_sort(values: list[int]) -> None:
    if not values:
        return
    low = high = values[0]
    for value in values:
        if value < low:
            low = value
        if high < value:
            high = value
    span = high - low + 1
    if span > COUNTING_RANGE_LIMIT:
        raise ValueError("counting sort value range is too sparse")
    counts = [0] * span
    for value in values:
        counts[value - low] += 1
    output = 0
    for bucket, count in enumerate(counts):
        for _ in range(count):
            values[output] = bucket + low
            output += 1


RADIX_BITS = 8
RADIX_BUCKETS = 1 << RADIX_BITS


# >>> radix
# 算法8.11：LSD 基数排序，每趟按 8 位分桶。
#
# **C++ 版那个「异或最高位」的技巧在 Python 里不成立**，这是本章最值得看的一处
# 语言差异。C++ 的 int 是定长补码，把符号位翻过来，负数的位模式就整体排到正数
# 前面，一趟不用特殊处理。Python 的整数是任意精度、没有「最高位」这个东西，
# `-1` 的二进制是概念上无限长的 1。所以这里换一种同样只扫两遍的办法：
# **整体平移到非负区间**，排完再平移回来。代价是多一次 min 扫描，
# 换来的是不依赖任何机器字长——这份实现对 10**30 一样成立。
def radix_sort(values: list[int]) -> None:
    if not values:
        return
    shift_base = min(values)
    keys = [value - shift_base for value in values]
    largest = max(keys)
    buffer = [0] * len(keys)
    shift = 0
    while (largest >> shift) > 0 or shift == 0:
        counts = [0] * RADIX_BUCKETS
        for key in keys:
            counts[(key >> shift) & (RADIX_BUCKETS - 1)] += 1
        offset = 0
        for bucket in range(RADIX_BUCKETS):
            counts[bucket], offset = offset, offset + counts[bucket]
        for key in keys:
            digit = (key >> shift) & (RADIX_BUCKETS - 1)
            buffer[counts[digit]] = key
            counts[digit] += 1
        keys, buffer = buffer, keys
        shift += RADIX_BITS
    for index, key in enumerate(keys):
        values[index] = key + shift_base
# <<< radix


# 算法8.13：以显式桶队列演示「分桶 — 按桶序收集」的基数排序。
#
# C++ 版这里用的是【代码8.12】那个固定容量环形队列。Python 侧按 D-025 §1
# 不实现 8.12：容量、环绕、下标回卷都是**存储管理**，而 list 没有容量这个概念，
# 照着写只会得到一层没有内容的包装。桶换成普通 list，这一节要讲的东西
# ——稳定性来自「桶内保持入桶顺序、收集时按桶号从小到大」——一个字都没少。
def radix_sort_linked_style(values: list[int]) -> None:
    if not values:
        return
    shift_base = min(values)
    keys = [value - shift_base for value in values]
    largest = max(keys)
    shift = 0
    while (largest >> shift) > 0 or shift == 0:
        buckets: list[list[int]] = [[] for _ in range(RADIX_BUCKETS)]
        for key in keys:
            buckets[(key >> shift) & (RADIX_BUCKETS - 1)].append(key)
        output = 0
        for bucket in buckets:
            for key in bucket:
                keys[output] = key
                output += 1
        shift += RADIX_BITS
    for index, key in enumerate(keys):
        values[index] = key + shift_base


# 算法8.14：排序索引，不移动原记录。记录大而键小时，搬索引比搬记录便宜得多。
def insertion_index_sort(values: list[int]) -> list[int]:
    indexes = list(range(len(values)))
    for i in range(1, len(indexes)):
        index = indexes[i]
        hole = i
        while hole > 0 and values[index] < values[indexes[hole - 1]]:
            indexes[hole] = indexes[hole - 1]
            hole -= 1
        indexes[hole] = index
    return indexes


# 算法8.15：沿置换环把索引顺序落实为记录顺序。
# 每个元素最多被搬一次，整趟 O(n) 次移动、O(1) 辅助空间。
def adjust_by_index(values: list[int], indexes: list[int]) -> None:
    for first in range(len(values)):
        if indexes[first] == first:
            continue
        current = first
        saved = values[first]
        while indexes[current] != first:
            source = indexes[current]
            values[current] = values[source]
            indexes[current] = current
            current = source
        values[current] = saved
        indexes[current] = current

# <<< sorting
