"""最佳二叉搜索树 Python 实现。"""
# >>> optimal-bst
def optimal_bst(successful, unsuccessful):
    if len(unsuccessful) != len(successful) + 1: raise ValueError("weight count")
    n = len(successful); cost = [[0]*(n+1) for _ in range(n+1)]; root = [[0]*(n+1) for _ in range(n+1)]
    weight = [[0]*(n+1) for _ in range(n+1)]
    for i in range(n+1): weight[i][i] = unsuccessful[i]
    for length in range(1, n+1):
        for first in range(n-length+1):
            last = first + length; weight[first][last] = weight[first][last-1] + successful[last-1] + unsuccessful[last]
            cost[first][last] = 10**30
            for r in range(first+1, last+1):
                candidate = cost[first][r-1] + cost[r][last] + weight[first][last]
                if candidate < cost[first][last]: cost[first][last], root[first][last] = candidate, r
    return cost, root
# <<< optimal-bst
