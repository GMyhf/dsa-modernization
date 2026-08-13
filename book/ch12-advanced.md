# 第12章 高级结构

## 本章先读什么

本章包含两个主题。可利用空间表复用固定数量的槽位：申请取得一个空闲槽，释放把它归还；现代
实现用索引句柄避免劫持全局 `new/delete`。最优二叉搜索树则把访问频率写成权重，用动态规划比较
每个区间可能的根，得到总查找代价最小的树。

源码入口：[空闲槽池与最优 BST](../code/ch12/optimal_bst/modern.hpp)、
[测试](../code/ch12/optimal_bst/test.cpp)。运行：
`python3 tools/check_code.py --allow-degraded code/ch12/optimal_bst`。

### 最优 BST 在优化什么

普通二叉搜索树只要求中序遍历有序，树形可能很多。若键的查找频率不同，应把常查的键放得更靠近
根。最优 BST 的输入包括成功查找权 `p[1..n]` 与失败查找权 `q[0..n]`；动态规划对每个区间
尝试每一个键作根，选择“左子树代价 + 右子树代价 + 本区间总权”最小的方案。

结果中的 `cost[i][j]` 是区间内键构成最优树的代价，`root[i][j]` 记录取得最小值的根，因而不只
能得到最小代价，还能按根表重建树形。它的朴素实现为 O(n³)，适合说明动态规划的填表思想。

最佳二叉搜索树以动态规划的成本表与根表求解；权重输入长度不匹配时明确报错。

```cpp file=code/ch12/optimal_bst/modern.hpp
#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa::advanced {

// >>> reusable-node-pool
template <typename T>
class ReusableNodePool {
public:
    explicit ReusableNodePool(std::size_t capacity) : slots_(capacity) {
        for (std::size_t index = 0; index < capacity; ++index) {
            free_.push_back(capacity - index - 1);
        }
    }

    [[nodiscard]] std::optional<std::size_t> acquire(const T& value) {
        if (free_.empty()) {
            return std::nullopt;
        }
        const std::size_t index = free_.back();
        free_.pop_back();
        slots_[index] = value;
        return index;
    }

    bool release(std::size_t index) {
        if (index >= slots_.size() || !slots_[index]) {
            return false;
        }
        slots_[index].reset();
        free_.push_back(index);
        return true;
    }

    [[nodiscard]] const T* get(std::size_t index) const noexcept {
        if (index >= slots_.size() || !slots_[index]) {
            return nullptr;
        }
        return &*slots_[index];
    }

    [[nodiscard]] std::size_t available() const noexcept { return free_.size(); }

private:
    std::vector<std::optional<T>> slots_;
    std::vector<std::size_t> free_;
};
// <<< reusable-node-pool

// >>> optimal-bst
struct OptimalBstResult {
    std::vector<std::vector<long long>> cost;
    std::vector<std::vector<std::size_t>> root;
};

inline OptimalBstResult optimal_bst(const std::vector<int>& successful,
                                    const std::vector<int>& unsuccessful) {
    if (unsuccessful.size() != successful.size() + 1) {
        throw std::invalid_argument("weight count");
    }
    const std::size_t count = successful.size();
    OptimalBstResult result{
        std::vector<std::vector<long long>>(count + 1,
                                            std::vector<long long>(count + 1, 0)),
        std::vector<std::vector<std::size_t>>(count + 1,
                                              std::vector<std::size_t>(count + 1, 0))};
    std::vector<std::vector<long long>> weight(
        count + 1, std::vector<long long>(count + 1, 0));

    for (std::size_t index = 0; index <= count; ++index) {
        // The book's c table measures internal-key comparison cost; an empty
        // interval has zero c cost while its unsuccessful weight remains in w.
        result.cost[index][index] = 0;
        weight[index][index] = unsuccessful[index];
    }
    for (std::size_t length = 1; length <= count; ++length) {
        for (std::size_t first = 0; first + length <= count; ++first) {
            const std::size_t last = first + length;
            weight[first][last] = weight[first][last - 1] + successful[last - 1] +
                                  unsuccessful[last];
            result.cost[first][last] = std::numeric_limits<long long>::max() / 4;
            for (std::size_t root = first + 1; root <= last; ++root) {
                const long long candidate = result.cost[first][root - 1] +
                                            result.cost[root][last] + weight[first][last];
                if (candidate < result.cost[first][last]) {
                    result.cost[first][last] = candidate;
                    result.root[first][last] = root;
                }
            }
        }
    }
    return result;
}
// <<< optimal-bst

}  // namespace dsa::advanced
```
