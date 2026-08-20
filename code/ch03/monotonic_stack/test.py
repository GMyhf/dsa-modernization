"""单调栈 Python 断言测试。"""

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


def brute_next(values: list[int], smaller: bool) -> list[int]:
    result = [len(values)] * len(values)
    for index, value in enumerate(values):
        for candidate in range(index + 1, len(values)):
            if (values[candidate] < value) if smaller else (values[candidate] > value):
                result[index] = candidate
                break
    return result


def brute_histogram(heights: list[int]) -> int:
    best = 0
    for left in range(len(heights)):
        current = heights[left]
        for right in range(left, len(heights)):
            current = min(current, heights[right])
            best = max(best, current * (right - left + 1))
    return best


def test_next_indices() -> None:
    for values in ([], [2], [2, 2, 2], [4, 1, 3, 2, 5], list(range(8, -1, -1))):
        check(modern.next_greater_indices(values) == brute_next(values, False), "下一个更大值对拍")
        check(modern.next_smaller_indices(values) == brute_next(values, True), "下一个更小值对拍")


def test_histograms() -> None:
    for heights in ([], [2], [2, 1, 2], [2, 1, 5, 6, 2, 3], [1, 2, 3, 4]):
        check(modern.largest_rectangle_area(heights) == brute_histogram(heights), "直方图暴力对拍")
    raised = False
    try:
        modern.largest_rectangle_area([1, -1])
    except ValueError:
        raised = True
    check(raised, "负高度必须拒绝")


def test_shared_cases() -> None:
    cases = shared_cases.load()
    for case in cases:
        values = shared_cases.integers(case.input)
        if case.expected_error:
            raised = False
            try:
                modern.largest_rectangle_area(values)
            except ValueError:
                raised = True
            check(raised, "T-047 monotonic exception")
        elif case.operation == "nge":
            check(modern.next_greater_indices(values) == shared_cases.integers(case.expected),
                  "T-047 monotonic nge")
        elif case.operation == "nse":
            check(modern.next_smaller_indices(values) == shared_cases.integers(case.expected),
                  "T-047 monotonic nse")
        else:
            check(modern.largest_rectangle_area(values) == int(case.expected),
                  "T-047 monotonic histogram")
    print(f"共享用例: {len(cases)}")


test_next_indices()
test_histograms()
test_shared_cases()
print(f"MonotonicStack(Python): {checks} 项断言，{failures} 失败")
sys.exit(0 if failures == 0 else 1)
