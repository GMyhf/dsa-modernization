"""树状数组 Python 断言测试。"""

import sys
from pathlib import Path

import modern

sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases  # noqa: E402


checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


def test_prefix_and_ranges() -> None:
    tree = modern.FenwickTree(8)
    values = [3, -2, 7, 0, 5, 1, -4, 6]
    for index, value in enumerate(values):
        tree.add(index, value)
    for end in range(len(values) + 1):
        check(tree.prefix_sum(end) == sum(values[:end]), "前缀和对拍")
    for left in range(len(values) + 1):
        for right in range(left, len(values) + 1):
            check(tree.range_sum(left, right) == sum(values[left:right]), "半开区间对拍")


def test_updates_and_boundaries() -> None:
    tree = modern.FenwickTree(3)
    tree.set(0, 10)
    tree.set(1, -2)
    tree.add(1, 5)
    check(tree.value_at(0) == 10 and tree.value_at(1) == 3, "set 与 add 的当前值")
    check(tree.range_sum(0, 2) == 13, "更新后的区间和")
    for operation in (lambda: tree.add(3, 1), lambda: tree.prefix_sum(4),
                      lambda: tree.range_sum(2, 1)):
        raised = False
        try:
            operation()
        except IndexError:
            raised = True
        check(raised, "越界或反向区间必须拒绝")
    check(modern.FenwickTree.lowbit(12) == 4, "lowbit")


def test_shared_cases() -> None:
    cases = shared_cases.load()
    for case in cases:
        parts = case.input.split("|")
        values = shared_cases.integers(parts[0])
        if case.expected_error:
            raised = False
            try:
                modern.FenwickTree(len(values)).prefix_sum(int(parts[1]))
            except IndexError:
                raised = True
            check(raised, "T-047 Fenwick exception")
        elif case.operation == "prefix":
            tree = modern.FenwickTree(len(values))
            for index, value in enumerate(values):
                tree.add(index, value)
            check(tree.prefix_sum(int(parts[1])) == int(case.expected), "T-047 Fenwick prefix")
        elif case.operation == "range":
            tree = modern.FenwickTree(len(values))
            for index, value in enumerate(values):
                tree.add(index, value)
            left, right = (int(value) for value in parts[1].split(","))
            check(tree.range_sum(left, right) == int(case.expected), "T-047 Fenwick range")
    print(f"共享用例: {len(cases)}")


test_prefix_and_ranges()
test_updates_and_boundaries()
test_shared_cases()
print(f"Fenwick(Python): {checks} 项断言，{failures} 失败")
sys.exit(0 if failures == 0 else 1)
