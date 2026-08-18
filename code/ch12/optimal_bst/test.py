"""算法12.2 最佳二叉搜索树的 Python 断言测试（D-025）。

判据：**若实现退回「按权值排序建树」或「取中位数当根」，这里必须有断言变红。**
最佳 BST 的全部内容就是「根不一定是权值最大的那个」，所以第一条断言
盯的不是总代价，而是**根选在哪里**。
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


SUCCESSFUL = [1, 5, 4, 3]
UNSUCCESSFUL = [5, 4, 3, 2, 1]


def brute_force(successful: list[int], unsuccessful: list[int]) -> int:
    """把所有形状枚举一遍，作为独立裁判。n 很小，指数级也跑得动。"""
    total = {}

    def weight(first: int, last: int) -> int:
        value = unsuccessful[first]
        for index in range(first, last):
            value += successful[index] + unsuccessful[index + 1]
        return value

    def best(first: int, last: int) -> int:
        if first == last:
            return 0
        if (first, last) in total:
            return total[(first, last)]
        answer = min(best(first, r - 1) + best(r, last)
                     for r in range(first + 1, last + 1)) + weight(first, last)
        total[(first, last)] = answer
        return answer

    return best(0, len(successful))


def test_textbook_case() -> None:
    cost, root = modern.optimal_bst(SUCCESSFUL, UNSUCCESSFUL)
    check(cost[0][4] == 57, "算法12.2 textbook total cost")
    # 权值最大的键是第 2 个（successful[1] == 5），而最优根是它——但这不是巧合，
    # 下一条用一组「最大权值不该当根」的权重把这件事分开。
    check(root[0][4] == 2, "算法12.2 textbook root")
    check(cost[0][4] == brute_force(SUCCESSFUL, UNSUCCESSFUL),
          "算法12.2 与穷举所有形状的结果一致")


def test_root_is_not_simply_the_heaviest_key() -> None:
    """贪心地「把权值最大的键放根上」是错的，这条断言就是那道分界线。"""
    successful = [1, 100, 1, 1, 1]
    unsuccessful = [0, 0, 0, 0, 0, 0]
    cost, root = modern.optimal_bst(successful, unsuccessful)
    check(cost[0][5] == brute_force(successful, unsuccessful),
          "算法12.2 偏斜权值下仍等于穷举结果")
    check(root[0][5] == 2, "算法12.2 最重的键确实该当根（对照组）")

    # 反过来：权值均匀时最优根落在中间，而不是第一个或最后一个。
    flat = [1, 1, 1, 1, 1]
    zeros = [0] * 6
    _, balanced = modern.optimal_bst(flat, zeros)
    check(1 < balanced[0][5] < 5, "算法12.2 权值均匀时根落在中间")


def test_subtree_table_is_consistent() -> None:
    """DP 表本身要自洽：每个区间的代价必须等于「按它自己记的根拆开」之后的和加权重。

    只验最终答案的测试，写错了填表顺序也可能蒙对；这一条盯的是整张表。
    """
    cost, root = modern.optimal_bst(SUCCESSFUL, UNSUCCESSFUL)
    consistent = True
    for first in range(len(SUCCESSFUL)):
        for last in range(first + 1, len(SUCCESSFUL) + 1):
            r = root[first][last]
            weight = UNSUCCESSFUL[first]
            for index in range(first, last):
                weight += SUCCESSFUL[index] + UNSUCCESSFUL[index + 1]
            if cost[first][last] != cost[first][r - 1] + cost[r][last] + weight:
                consistent = False
    check(consistent, "算法12.2 整张 DP 表与它自己记录的根自洽")


def test_edges_and_errors() -> None:
    empty, roots = modern.optimal_bst([], [7])
    check(empty[0][0] == 0, "算法12.2 空键集代价为 0")
    check(roots[0][0] == 0, "算法12.2 空键集没有根")
    single, single_root = modern.optimal_bst([3], [1, 2])
    check(single[0][1] == 6, "算法12.2 单键代价 = 该区间总权重")
    check(single_root[0][1] == 1, "算法12.2 单键的根就是它自己")
    raised = False
    try:
        modern.optimal_bst([1, 2], [3, 4])
    except ValueError:
        raised = True
    check(raised, "算法12.2 不成功权值必须比成功权值多一个")


def main() -> int:
    test_textbook_case()
    test_root_is_not_simply_the_heaviest_key()
    test_subtree_table_is_consistent()
    test_edges_and_errors()
    shared = shared_cases.load()
    for case in shared:
        left, right = case.input.split("|", 1)
        successful = shared_cases.integers(left)
        unsuccessful = shared_cases.integers(right)
        if case.expected_error:
            raised = False
            try:
                modern.optimal_bst(successful, unsuccessful)
            except ValueError:
                raised = True
            check(raised, "T-047 optimal exception")
        else:
            result, roots = modern.optimal_bst(successful, unsuccessful)
            expected_cost, expected_root = shared_cases.integers(case.expected)
            check(result[0][len(successful)] == expected_cost, "T-047 optimal cost")
            check(roots[0][len(successful)] == expected_root, "T-047 optimal root")
    print(f"共享用例: {len(shared)}")
    print(f"OptimalBST(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
