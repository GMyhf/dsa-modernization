"""第 9 章外部排序 Python 断言。"""

import modern

checks = 0


def check(condition: bool, name: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(name)


runs = modern.replacement_selection([4, 1, 7, 2, 8, 3, 6, 5], 3)
check(len(runs) == 2, "算法9.1 textbook input makes two runs")
check(all(run == sorted(run) for run in runs), "算法9.1 每个顺串有序")
for cls in (modern.WinnerTree, modern.LoserTree):
    tree = cls([7, 2, 5, 1])
    check(tree.winner_index() == 3 and tree.winner() == 1, "竞赛树 initial winner")
    tree.replace(3, 9)
    check(tree.winner() == 2, "竞赛树 replacement replay")
check(modern.LoserTree([]).winner() is None, "空败者树")
raised = False
try:
    modern.replacement_selection([1], 0)
except ValueError:
    raised = True
check(raised, "算法9.1 memory must be positive")
print(f"{checks} 项断言")
