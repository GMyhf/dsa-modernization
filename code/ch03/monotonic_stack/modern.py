"""单调栈：下一个更大/更小值与直方图最大矩形。"""


# >>> next-greater
def next_greater_indices(values: list[int]) -> list[int]:
    """返回右侧第一个严格更大值的位置；不存在时返回 len(values)。"""
    answer = [len(values)] * len(values)
    stack: list[int] = []
    for index, value in enumerate(values):
        while stack and values[stack[-1]] < value:
            answer[stack.pop()] = index
        stack.append(index)
    return answer
# <<< next-greater


# >>> next-smaller
def next_smaller_indices(values: list[int]) -> list[int]:
    """返回右侧第一个严格更小值的位置；不存在时返回 len(values)。"""
    answer = [len(values)] * len(values)
    stack: list[int] = []
    for index, value in enumerate(values):
        while stack and values[stack[-1]] > value:
            answer[stack.pop()] = index
        stack.append(index)
    return answer
# <<< next-smaller


# >>> histogram
def largest_rectangle_area(heights: list[int]) -> int:
    """返回直方图最大矩形面积。"""
    if any(height < 0 for height in heights):
        raise ValueError("histogram height must be non-negative")
    stack: list[int] = []
    best = 0
    for index in range(len(heights) + 1):
        current = 0 if index == len(heights) else heights[index]
        while stack and heights[stack[-1]] > current:
            top = stack.pop()
            left = 0 if not stack else stack[-1] + 1
            best = max(best, heights[top] * (index - left))
        stack.append(index)
    return best
# <<< histogram
