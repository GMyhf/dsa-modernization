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

    # -------------------------------------------------------------
    # 1. 状态定义（模拟 CPU 的“程序计数器” PC / 返回地址 Return Address）
    # enter      : 函数入口点（开始执行当前层的逻辑）
    # after_rule1: 从“选择当前物品”的子调用返回后的恢复点
    # after_rule2: 从“不选当前物品”的子调用返回后的恢复点
    # -------------------------------------------------------------
    enter, after_rule1, after_rule2 = range(3)

    # 显式模拟调用栈：
    # 栈帧与递归版 solve(remaining, count) 的参数一一对应，再加上返回地址：
    # (当前剩余容量 remaining, 可选物品数 count, 当前执行阶段 stage)
    stack = [(capacity, len(weights), enter)]

    # 记录最终被选中的物品索引（构成解的路径）
    chosen: list[int] = []

    # 模拟 CPU 的“返回值寄存器”（例如 x86 中的 RAX/EAX）
    # 子调用执行完毕后，将结果（True/False）写入此变量，交由父调用读取
    child_result = False

    while stack:
        # 弹出当前栈顶执行上下文，恢复该层的全部“局部变量”
        # count 表示当前还有前 count 个物品可选（对应下标 0 到 count - 1）
        remaining, count, stage = stack.pop()

        # ==================== 阶段 0：函数入口 ====================
        if stage == enter:
            # Base Case 1: 恰好凑齐容量，递归成功
            if remaining == 0:
                child_result = True
            # Base Case 2: 超过承重，或已无物品可选，递归失败
            elif remaining < 0 or count == 0:
                child_result = False
            else:
                # 分支 1：尝试“选择”第 (count - 1) 个物品
                # [压栈操作 1 - 保存断点]：子调用结束后，需回到 after_rule1 处继续
                stack.append((remaining, count, after_rule1))
                # [压栈操作 2 - 发起子调用]：扣减当前物品重量，可选物品数减 1，进入新的 enter
                stack.append((remaining - weights[count - 1], count - 1, enter))

        # ==================== 阶段 1：分支 1 返回 ====================
        elif stage == after_rule1:
            # 从“返回值寄存器”读取分支 1 的成败
            if child_result:
                # 分支 1 成功：说明拿该物品能凑齐，将其记录进结果集中
                chosen.append(count - 1)
                # 当前层任务完成，直接结束，准备弹出更上一层的断点
            else:
                # 分支 1 失败：回溯，转入分支 2——尝试“不选”第 (count - 1) 个物品
                # [压栈操作 1 - 保存断点]：分支 2 结束后恢复到 after_rule2
                stack.append((remaining, count, after_rule2))
                # [压栈操作 2 - 发起子调用]：容量不变，可选物品数减 1，发起新调用
                stack.append((remaining, count - 1, enter))

        # ==================== 阶段 2：分支 2 返回 ====================
        # elif stage == after_rule2:
        #     pass
        # 说明：分支 2 的结果已经直接留存在 child_result 里了。
        # 这里无需额外代码，当前帧直接随 pop() 销毁，继续回溯上层。

    # 整个搜索结束后，若根调用成功则返回结果集，否则返回 None
    return chosen if child_result else None
# <<< explicit-stack


# >>> optimized
def knapsack_optimized(capacity: int, weights: list[int]) -> list[int] | None:
    """算法3.12：栈帧只保存剩余承重和返回地址。"""
    _validate(capacity, weights)

    # -------------------------------------------------------------
    # 1. 状态定义（模拟 CPU 的“程序计数器” PC / 返回地址 Return Address）
    # enter      : 函数入口点（开始执行当前层的逻辑）
    # after_rule1: 从“选择当前物品”的子调用返回后的恢复点
    # after_rule2: 从“不选当前物品”的子调用返回后的恢复点
    # -------------------------------------------------------------
    enter, after_rule1, after_rule2 = range(3)

    # 显式模拟调用栈：
    # 栈帧中只保留绝对必需的两个状态：(当前剩余容量 remaining, 当前执行阶段 stage)
    stack = [(capacity, enter)]

    # 记录最终被选中的物品索引（构成解的路径）
    chosen: list[int] = []

    # 模拟 CPU 的“返回值寄存器”（例如 x86 中的 RAX/EAX）
    # 子调用执行完毕后，将结果（True/False）写入此变量，交由父调用读取
    child_result = False

    size = len(weights)

    # 全局调用深度计数器：
    # 【核心优化】：不需要在每个栈帧里都存“当前处理到第几个物品(count)”。
    # 递归深度与可选物品数是一一对应的，利用公式直接计算，极大节省了栈内存。
    depth = 1

    while stack:
        # 弹出当前栈顶执行上下文，深度减 1
        remaining, stage = stack.pop()
        depth -= 1

        # 通过全局深度逆向推导出当前正在处理的物品索引范围
        # count 表示当前还有前 count 个物品可选（对应下标 0 到 count - 1）
        count = size - depth

        # ==================== 阶段 0：函数入口 ====================
        if stage == enter:
            # Base Case 1: 恰好凑齐容量，递归成功
            if remaining == 0:
                child_result = True
            # Base Case 2: 超过承重，或已无物品可选，递归失败
            elif remaining < 0 or count == 0:
                child_result = False
            else:
                # 分支 1：尝试“选择”第 (count - 1) 个物品
                # [压栈操作 1 - 保存断点]：子调用结束后，需回到 after_rule1 处继续
                stack.append((remaining, after_rule1))
                # [压栈操作 2 - 发起子调用]：剩余容量扣减当前物品重量，进入新的 enter
                stack.append((remaining - weights[count - 1], enter))
                # 同时压入“返回断点”和“新调用”，栈深度增加 2
                depth += 2

        # ==================== 阶段 1：分支 1 返回 ====================
        elif stage == after_rule1:
            # 从“返回值寄存器”读取分支 1 的成败
            if child_result:
                # 分支 1 成功：说明拿该物品能凑齐，将其记录进结果集中
                chosen.append(count - 1)
                # 当前层任务完成，直接结束，准备弹出更上一层的断点
            else:
                # 分支 1 失败：回溯，转入分支 2——尝试“不选”第 (count - 1) 个物品
                # [压栈操作 1 - 保存断点]：分支 2 结束后恢复到 after_rule2
                stack.append((remaining, after_rule2))
                # [压栈操作 2 - 发起子调用]：容量不变，发起新调用
                stack.append((remaining, enter))
                depth += 2

        # ==================== 阶段 2：分支 2 返回 ====================
        # elif stage == after_rule2:
        #     pass
        # 说明：分支 2 的结果已经直接留存在 child_result 里了。
        # 这里无需额外代码，当前帧直接随 pop() 销毁，继续回溯上层。

    # 整个搜索结束后，若根调用成功则返回结果集，否则返回 None
    return chosen if child_result else None
# <<< optimized
