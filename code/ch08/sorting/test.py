"""第 8 章 Python 实现的断言测试（D-025）。

与 `test.cpp` 同规矩：零框架、纯断言、退出码非 0 即失败、末行报断言数。
判据也同一条——**若实现退回原书那一版，这里必须有断言变红**。

测试里用 `sorted()` 是正当的，正如 `test.cpp` 里用 `std::is_sorted`：
它在这儿是**独立的裁判**，不是被测对象。`modern.py` 里出现它才是违规，
`tools/check_code.py` 的 `check_d025` 只查实现文件。
"""

import sys
from pathlib import Path

import modern

sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases  # noqa: E402  共享用例表的读取器（T-047）

checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


# 每个排序都跑同一套输入。挑这几组是有理由的：
#   已排好序 / 逆序   —— 快排的两个退化输入
#   全相等           —— 划分把边界推到端点的场合
#   含重复、含负数    —— 计数与基数排序最容易写错的地方
#   单元素、空表      —— 循环边界
BATTERY = {
    "mixed": [3, -2, 7, 3, 0, -2, 9, 1],
    "already sorted": [-3, -1, 0, 4, 8],
    "reversed": [9, 7, 5, 3, 1, -1],
    "all equal": [4, 4, 4, 4],
    "duplicates": [5, 1, 5, 1, 5, 1],
    "single": [42],
    "empty": [],
    "negatives only": [-9, -1, -7, -3],
}


def exercise(sort, label: str) -> None:
    for name, data in BATTERY.items():
        values = list(data)
        sort(values)
        check(values == sorted(data), f"{label} / {name}")
    # 幂等：已排好的表再排一次不能变。写错的 in-place 排序常常在这一条上露馅。
    twice = [6, 2, 9, 2, 0]
    sort(twice)
    once = list(twice)
    sort(twice)
    check(twice == once, f"{label} / 幂等")


def test_elementary_sorts() -> None:
    exercise(modern.insertion_sort, "算法8.1 Python insertion")
    exercise(modern.shell_sort, "算法8.2 Python shell")
    exercise(modern.selection_sort, "算法8.3 Python selection")
    exercise(modern.heap_sort, "算法8.4 Python heap")
    exercise(modern.bubble_sort, "算法8.5 Python bubble")


class CountingList(list):
    """会数自己被读了多少次的 list。

    这是 Python 能做、C++ 那份实现做不到的一件事：`bubble_sort` 声明的形参
    类型是 `list[int]`，但注解在运行期不生效，传一个 list 的子类进去照样跑。
    于是「本趟无交换即提前结束」这个优化**可以被直接观测**，而不是只能靠
    「结果仍然有序」这种连优化删掉也不会红的弱断言。
    """

    reads = 0

    def __getitem__(self, index):
        CountingList.reads += 1
        return list.__getitem__(self, index)


def test_bubble_early_exit_is_observable() -> None:
    """算法8.5 的「本趟无交换即结束」必须真的省掉后面的趟。

    已有序的 n=100 输入：带优化只需一趟，读次数与 n 同阶；把 changed 那三句
    删掉就变成 n 趟，读次数与 n² 同阶。两者差两个数量级，阈值取 1000 足够稳。
    """
    CountingList.reads = 0
    values = CountingList(range(100))
    modern.bubble_sort(values)
    check(list(values) == list(range(100)), "算法8.5 Python bubble 已有序输入不被打乱")
    check(CountingList.reads < 1000,
          f"算法8.5 Python bubble 有序输入一趟即停（实读 {CountingList.reads} 次）")

    CountingList.reads = 0
    worst = CountingList(range(100, 0, -1))
    modern.bubble_sort(worst)
    check(CountingList.reads > 5000,
          "算法8.5 Python bubble 逆序输入确实跑满 n 趟（对照组，证明上面那条不是恒真）")


def test_quick_sorts() -> None:
    exercise(modern.quick_sort, "算法8.6 Python quick")
    exercise(modern.quick_sort_optimized, "算法8.7 Python quick optimized")


def test_recursion_depth_is_the_python_specific_cost() -> None:
    """算法8.6 与 8.7 的差别，在 Python 里是「能不能跑完」而不是「快不快」。

    末元素当轴 + 已排好序的输入 = n 层递归。CPython 默认上限 1000 层，
    所以 8.6 在 2000 个有序元素上必然抛 RecursionError；8.7 只递归短侧，
    深度 O(log n)，同一份输入必须跑得完。

    这两条断言是**一对**：只留前一条，8.7 退化成 8.6 也不会红。
    """
    ordered = list(range(2000))

    raised = False
    values = list(ordered)
    try:
        modern.quick_sort(values)
    except RecursionError:
        raised = True
    check(raised, "算法8.6 Python quick 在有序输入上撞到递归上限（原书这一版的代价）")

    for name, data in (("有序", ordered), ("逆序", list(reversed(ordered)))):
        values = list(data)
        completed = True
        try:
            modern.quick_sort_optimized(values)
        except RecursionError:
            completed = False
        # 两件事一起断言：没撞上限、且排对了。只断言前者，实现返回原样也能过。
        check(completed and values == ordered,
              f"算法8.7 Python quick optimized {name}输入跑得完（递归深度 O(log n)）")


def test_merge_sorts() -> None:
    exercise(modern.merge_sort, "算法8.8 Python merge")
    exercise(modern.merge_sort_optimized, "算法8.9 Python merge optimized")


class Keyed:
    """只按 key 比较、payload 不参与比较的记录。

    只排 int 是看不出稳定性的：两个相等的 3 换不换位置，结果一模一样。
    C++ 那份 `std::vector<int>` 的实现因此**根本无法**验证稳定性；
    Python 这份不需要改一个字就能排任何可比较对象，稳定性于是变成可观测的。
    """

    def __init__(self, key: int, payload: str) -> None:
        self.key = key
        self.payload = payload

    def __lt__(self, other: "Keyed") -> bool:
        return self.key < other.key

    # 补 `<=` 不是为了给实现多一个可用的运算符，恰恰相反：少了它，
    # 把 `<` 误写成 `<=` 时抛的是 TypeError，变红的理由就成了「对象不支持
    # 这个运算符」而不是「顺序不对」——那条断言等于没在守稳定性。
    def __le__(self, other: "Keyed") -> bool:
        return self.key <= other.key


def test_stability_of_merge_and_insertion() -> None:
    """`values[right] < values[left]` 若写成 `<=`，算法8.8 的稳定性当场没了。

    归并、插入、冒泡是原书点名的稳定排序；选择与快排不是。这里只钉稳定的那几个，
    钉法是让相等 key 携带不同 payload，排完检查 payload 仍是入场顺序。
    """
    def sample() -> list:
        return [Keyed(1, "a"), Keyed(0, "b"), Keyed(1, "c"), Keyed(0, "d"), Keyed(1, "e")]

    for name, sort in (
        ("算法8.8 Python merge 稳定", modern.merge_sort),
        ("算法8.9 Python merge optimized 稳定", modern.merge_sort_optimized),
        ("算法8.1 Python insertion 稳定", modern.insertion_sort),
        ("算法8.5 Python bubble 稳定", modern.bubble_sort),
    ):
        records = sample()
        sort(records)
        check([r.payload for r in records] == ["b", "d", "a", "c", "e"], name)

    # 索引排序（算法8.14）也承诺稳定：相等键的原下标必须仍是升序。
    keys = [1, 0, 1, 0, 1]
    indexes = modern.insertion_index_sort(keys)
    check([i for i in indexes if keys[i] == 1] == [0, 2, 4],
          "算法8.14 Python 相等键保持原序（稳定）")


def test_counting_sort() -> None:
    exercise(modern.counting_sort, "算法8.10 Python counting")
    rejected = False
    try:
        modern.counting_sort([-10**9, 10**9])
    except ValueError:
        rejected = True
    check(rejected, "算法8.10 Python counting 拒绝过于稀疏的值域")
    # Python 的整数没有宽度，值域计算不会溢出——但上限检查照样要有，
    # 它挡的是内存不是溢出。刚好卡在限内的值域必须被接受。
    span = modern.COUNTING_RANGE_LIMIT - 1
    values = [0, span]
    modern.counting_sort(values)
    check(values == [0, span], "算法8.10 Python counting 限内的稀疏值域仍然接受")


def test_radix_sorts() -> None:
    exercise(modern.radix_sort, "算法8.11 Python radix")
    exercise(modern.radix_sort_linked_style, "算法8.13 Python linked radix")


def test_radix_does_not_depend_on_machine_word() -> None:
    """C++ 版靠「异或最高位」处理负数，那依赖 int 是 32 位定长补码。

    Python 的整数是任意精度，这份实现改用整体平移，所以对远超 64 位的值
    一样成立。这条断言就是那句注释的凭据——换回移位取符号位的写法，它会红。
    """
    huge = [10**30, -(10**30), 0, 10**30 + 1, -(10**30) - 1]
    values = list(huge)
    modern.radix_sort(values)
    check(values == sorted(huge), "算法8.11 Python radix 对超出机器字长的整数成立")

    values = list(huge)
    modern.radix_sort_linked_style(values)
    check(values == sorted(huge), "算法8.13 Python linked radix 同样不依赖字长")


def test_index_sort() -> None:
    values = [29, 12, 34, 8]
    indexes = modern.insertion_index_sort(values)
    check(indexes == [3, 1, 0, 2], "算法8.14 Python 返回按值排好的下标")
    check(values == [29, 12, 34, 8], "算法8.14 Python 不动原记录")

    modern.adjust_by_index(values, indexes)
    check(values == [8, 12, 29, 34], "算法8.15 Python 沿置换环落实记录顺序")
    check(indexes == [0, 1, 2, 3], "算法8.15 Python 把索引复位成恒等")

    # 多环、含自环的一般情形：置换环那段代码最容易在这里写错。
    data = [50, 40, 30, 20, 10, 60]
    order = modern.insertion_index_sort(data)
    modern.adjust_by_index(data, order)
    check(data == [10, 20, 30, 40, 50, 60], "算法8.15 Python 多个置换环")
    check(order == list(range(6)), "算法8.15 Python 多环后索引仍复位")


def test_shared_cases() -> int:
    """用例表由 support/shared_cases.py 读，**不再自己解析**（T-047）。

    本单元原先手抄了一份解析，两种语言各一份。它能跑，但格式一变就会与另外
    十个单元静默分家：C++ 那份抄本对列数只补不校，六列的行照收；
    这份却按五元组解包、当场抛。两边对同一张表的读法不同，
    「共享用例」这四个字就名存实亡了。
    """
    shared = shared_cases.load()
    for case in shared:
        if case.operation == "constant":
            check(modern.COUNTING_RANGE_LIMIT == int(case.expected), f"T-047 {case.name}")
            continue
        values = shared_cases.integers(case.input)
        if case.expected_error == "invalid_argument":
            raised = False
            try:
                modern.counting_sort(values)
            except ValueError:
                raised = True
            check(raised, f"T-047 {case.name} expected invalid_argument")
            continue
        if case.operation == "insertion":
            modern.insertion_sort(values)
        elif case.operation == "radix":
            modern.radix_sort(values)
        elif case.operation == "index":
            values = modern.insertion_index_sort(values)
        check(values == shared_cases.integers(case.expected), f"T-047 {case.name} shared result")
    print(f"共享用例: {len(shared)}")
    return len(shared)


def main() -> int:
    test_elementary_sorts()
    test_bubble_early_exit_is_observable()
    test_quick_sorts()
    test_recursion_depth_is_the_python_specific_cost()
    test_merge_sorts()
    test_stability_of_merge_and_insertion()
    test_counting_sort()
    test_radix_sorts()
    test_radix_does_not_depend_on_machine_word()
    test_index_sort()
    test_shared_cases()
    print(f"Sorting(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
