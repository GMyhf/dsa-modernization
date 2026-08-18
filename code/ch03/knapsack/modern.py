"""背包问题的 Python 实现（D-025）。"""


def _validate(capacity: int, weights: list[int]) -> None:
    if capacity < 0:
        raise ValueError("背包：承重量不能为负")
    if any(weight <= 0 for weight in weights):
        raise ValueError("背包：物品重量必须为正")


# >>> recursive
def knapsack_recursive(capacity: int, weights: list[int]) -> list[int] | None:
    """算法3.10：两条递归规则，返回选中物品的下标。"""
    _validate(capacity, weights)
    chosen: list[int] = []

    def solve(remaining: int, count: int) -> bool:
        if remaining == 0:
            return True
        if remaining < 0 or count == 0:
            return False
        if solve(remaining - weights[count - 1], count - 1):
            chosen.append(count - 1)
            return True
        return solve(remaining, count - 1)

    return chosen if solve(capacity, len(weights)) else None
# <<< recursive


# >>> explicit-stack
def knapsack_with_explicit_stack(capacity: int, weights: list[int]) -> list[int] | None:
    """算法3.11：用栈帧中的返回地址机械模拟递归。"""
    _validate(capacity, weights)
    enter, after_rule1, after_rule2 = range(3)
    stack = [(capacity, len(weights), enter)]
    chosen: list[int] = []
    child_result = False
    while stack:
        remaining, count, stage = stack.pop()
        if stage == enter:
            if remaining == 0:
                child_result = True
            elif remaining < 0 or count == 0:
                child_result = False
            else:
                stack.append((remaining, count, after_rule1))
                stack.append((remaining - weights[count - 1], count - 1, enter))
        elif stage == after_rule1:
            if child_result:
                chosen.append(count - 1)
            else:
                stack.append((remaining, count, after_rule2))
                stack.append((remaining, count - 1, enter))
    return chosen if child_result else None
# <<< explicit-stack


# >>> optimized
def knapsack_optimized(capacity: int, weights: list[int]) -> list[int] | None:
    """算法3.12：栈帧只保存剩余承重和返回地址。"""
    _validate(capacity, weights)
    enter, after_rule1, after_rule2 = range(3)
    stack = [(capacity, enter)]
    chosen: list[int] = []
    child_result = False
    size = len(weights)
    depth = 1
    while stack:
        remaining, stage = stack.pop()
        depth -= 1
        count = size - depth
        if stage == enter:
            if remaining == 0:
                child_result = True
            elif remaining < 0 or count == 0:
                child_result = False
            else:
                stack.append((remaining, after_rule1))
                stack.append((remaining - weights[count - 1], enter))
                depth += 2
        elif stage == after_rule1:
            if child_result:
                chosen.append(count - 1)
            else:
                stack.append((remaining, after_rule2))
                stack.append((remaining, enter))
                depth += 2
    return chosen if child_result else None
# <<< optimized
