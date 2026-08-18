"""第 9 章外部排序 Python 实现的断言测试（D-025）。

判据同 `test.cpp`：**若实现退回「每次重新扫一遍」的写法，这里必须有断言变红。**
竞赛树的全部意义就是替换一名选手后只沿一条路径重赛，所以比较次数本身
就是被测对象，`comparisons()` 把它变成可断言的量。
"""

import sys

import modern

checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


def test_replacement_selection() -> None:
    runs = modern.replacement_selection([4, 1, 7, 2, 8, 3, 6, 5], 3)
    check(len(runs) == 2, "算法9.1 textbook input makes two runs")
    check(all(run == sorted(run) for run in runs), "算法9.1 每个顺串有序")
    check(sorted(v for run in runs for v in run) == [1, 2, 3, 4, 5, 6, 7, 8],
          "算法9.1 一个记录都不丢")
    # 置换选择的卖点：顺串长度可以超过内存容量。已经递增的输入应当只出一个顺串，
    # 长度 8 > 内存 3——退化成「读满内存就排序输出」的写法在这里会红。
    single = modern.replacement_selection([1, 2, 3, 4, 5, 6, 7, 8], 3)
    check(len(single) == 1 and len(single[0]) == 8,
          "算法9.1 顺串长度可超过内存容量（置换选择的全部卖点）")
    check(modern.replacement_selection([], 4) == [], "算法9.1 空输入没有顺串")
    raised = False
    try:
        modern.replacement_selection([1], 0)
    except ValueError:
        raised = True
    check(raised, "算法9.1 memory must be positive")


PLAYERS = [7, 2, 5, 1, 9, 3, 8, 4]


def test_both_trees_agree_with_brute_force() -> None:
    """胜者树与败者树是同一个问题的两种记法，答案必须一致，也必须等于暴力最小值。"""
    for cls, label in ((modern.WinnerTree, "代码9.2 胜者树"),
                       (modern.LoserTree, "代码9.3 败者树")):
        tree = cls(list(PLAYERS))
        check(tree.winner() == min(PLAYERS), f"{label} initial winner")
        check(tree.winner_index() == PLAYERS.index(min(PLAYERS)),
              f"{label} initial winner index")
        # 连续替换：每一步都跟暴力最小值对一次，写错重赛路径会在某一步露馅。
        players = list(PLAYERS)
        ok = True
        for player, value in ((3, 10), (1, 0), (0, 6), (7, -5), (3, 11), (6, -9)):
            players[player] = value
            tree.replace(player, value)
            if tree.winner() != min(players):
                ok = False
        check(ok, f"{label} replacement replay 与暴力最小值逐步一致")


def test_replay_is_logarithmic() -> None:
    """替换一名选手只沿叶到根重赛——这是竞赛树相对线性扫描的唯一优势。

    16 路时一次替换应当在 5 次比较以内（树高 4）；每次重扫一遍则要 15 次。
    退化成 `min(range(n))` 的写法在这里必红。
    """
    players = list(range(16, 0, -1))
    for cls, label in ((modern.WinnerTree, "代码9.2 胜者树"),
                       (modern.LoserTree, "代码9.3 败者树")):
        tree = cls(list(players))
        tree.reset_comparisons()
        tree.replace(9, -1)
        used = tree.comparisons()
        check(used <= 5, f"{label} 一次替换只比较 {used} 次（树高 4，上限 5）")
        check(used >= 4, f"{label} 确实沿整条路径重赛了（{used} 次，不能少于树高）")
        check(tree.winner() == -1, f"{label} 替换后的新最小值胜出")


def test_loser_tree_stores_real_losers() -> None:
    """败者树内部结点记的必须是**那一场的输家**，不是随便一个大值。

    判据落在结构上：输家必须落在该结点管辖的叶子区间内。
    只比键的大小是不够的——全局最大值也满足「不小于胜者」。
    """
    tree = modern.LoserTree(list(PLAYERS))
    size = len(PLAYERS)

    def leaf_span(node: int) -> tuple[int, int]:
        """结点 node 管着哪一段叶子。判据要落在**结构**上，不能只看键的大小。"""
        left = right = node
        while left < size:
            left *= 2
        while right < size:
            right = right * 2 + 1
        return left - size, right - size

    structural = True
    for node in range(1, size):
        loser = tree.loser_at(node)
        low, high = leaf_span(node)
        if loser is None or not low <= loser <= high:
            structural = False
    # 「键不小于全局胜者」这条太松——**全局最大值也满足它**，返回 max(...) 的写法
    # 能蒙混过去（2026-08-18 变异自检实测漏网）。改成「输家必须落在该结点管辖的
    # 叶子区间内」，这是只有真的在那一场比过才成立的性质。
    check(structural, "代码9.3 每个内部结点记的输家落在它管辖的叶子区间内")
    check(tree.loser_at(0) is None and tree.loser_at(10**6) is None,
          "代码9.3 越界结点返回 None")


def test_edge_cases() -> None:
    for cls, label in ((modern.WinnerTree, "代码9.2 胜者树"),
                       (modern.LoserTree, "代码9.3 败者树")):
        check(cls([]).winner() is None, f"{label} 空树没有胜者")
        one = cls([42])
        check(one.winner() == 42, f"{label} 单选手就是胜者")
        # 选手数不是 2 的幂：补出来的叶子是 +∞，不能被选成胜者。
        odd = cls([5, 3, 9])
        check(odd.winner() == 3, f"{label} 选手数非 2 的幂时补位不夺冠")
        odd.replace(1, 100)
        check(odd.winner() == 5, f"{label} 非 2 的幂时替换后仍不选中补位")
        raised = False
        try:
            odd.replace(9, 1)
        except IndexError:
            raised = True
        check(raised, f"{label} 越界选手抛 IndexError")


def main() -> int:
    test_replacement_selection()
    test_both_trees_agree_with_brute_force()
    test_replay_is_logarithmic()
    test_loser_tree_stores_real_losers()
    test_edge_cases()
    print(f"ExternalSort(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
