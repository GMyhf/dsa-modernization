"""第 3 章背包 Python 断言。"""

import itertools
import modern

checks = 0


def check(condition: bool, name: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(name)


def exists(capacity: int, weights: list[int]) -> bool:
    return any(sum(weights[i] for i in range(len(weights)) if mask & (1 << i)) == capacity
               for mask in range(1 << len(weights)))


versions = [modern.knapsack_recursive, modern.knapsack_with_explicit_stack,
            modern.knapsack_optimized]
for weights in ([], [2, 3, 7], [1, 1, 2, 5], [4, 6, 9, 13]):
    for capacity in range(20):
        expected = exists(capacity, weights)
        for solve in versions:
            result = solve(capacity, weights)
            check((result is not None) == expected, "背包存在性")
            if result is not None:
                check(len(result) == len(set(result)), "下标不重复")
                check(sum(weights[index] for index in result) == capacity, "返回解可复验")

for solve in versions:
    for capacity, weights in ((-1, [1]), (1, [0]), (1, [-1])):
        raised = False
        try:
            solve(capacity, weights)
        except ValueError:
            raised = True
        check(raised, "非法输入必须拒绝")

check("knapsack_recursive" not in modern.knapsack_with_explicit_stack.__code__.co_names,
      "算法3.11 没有调用递归版")
check("knapsack_recursive" not in modern.knapsack_optimized.__code__.co_names,
      "算法3.12 没有调用递归版")
print(f"{checks} 项断言")
