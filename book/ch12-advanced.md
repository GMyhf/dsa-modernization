# 第12章 高级数据结构

本章包含两个主题。可利用空间表复用固定数量的槽位：申请取得一个空闲槽，释放把它归还。最优二叉搜索树则把访问频率写成权重，用动态规划比较每个区间可能的根，得到总查找代价最小的树。

源码：[空闲槽池与最优 BST](../code/ch12/optimal_bst/modern.hpp)、
[可运行示例](../code/ch12/optimal_bst/demo.cpp)、
[测试](../code/ch12/optimal_bst/test.cpp)。

## 12.1 多维数组

多维数组是「数组的数组」。元素个数相对固定，一旦生成只改值、不改相对位置，因此用顺序存储，按下标随机访问。

### 12.1.1 多维数组的存储

C++ 按行优先：先排最右下标。$d_0\times d_1\times\cdots\times d_{n-1}$ 数组中元素 $A[j_0,\ldots,j_{n-1}]$ 相对首地址的偏移是

$$
d\cdot\Bigl(\sum_{i=0}^{n-2} j_i\prod_{k=i+1}^{n-1}d_k + j_{n-1}\Bigr)
$$

每个元素定位时间相同，所以是随机存储结构。FORTRAN 按列优先，公式左右对调。

### 12.1.2 特殊矩阵

上/下三角矩阵和对角线对称矩阵都可以压成一维：n 阶下三角只需 $(n^2+n)/2$ 个单元，$a_{i,j}$（$i\ge j$）落在 $\mathrm{list}[(i^2+i)/2+j]$。对称矩阵只存一半，另一半用 $a_{i,j}=a_{j,i}$ 映射。

### 12.1.3 稀疏矩阵

非零元很少且分布不规则时，$\delta=t/(m n)$ 小于约 0.05 就按稀疏处理，改存三元组或十字链表，而不是整块二维数组。本章不另写未验证的十字链表实现。

## 12.2 广义表和存储管理

广义表的元素可以是原子，也可以是另一个表。纯表对应树，再入表对应有向无环图，循环表对应有环图。存储上常用头尾指针的结点；表共享时必须处理别名和回收。

### 12.2.1 广义表的定义和存储结构

表头是第一个元素，表尾是去掉表头后剩下的表。空表没有头尾。头尾表示法让递归算法自然对应「先处理头、再处理尾」。

### 12.2.2 可利用空间表

原书用重载 `operator new/delete` 和一条全局 `avail` 链实现结点复用。所有对象共享隐式全局状态，生命周期结束时还要 `::delete` 整条链。现代实现改成显式的索引句柄池：`acquire` 从空闲栈弹出一个下标，`release` 把它推回去；耗尽返回 `nullopt`，重复/越界归还返回 `false`。释放后的下标失效，`get` 得到空指针。

普通二叉搜索树只要求中序遍历有序，树形可能很多。若键的查找频率不同，应把常查的键放得更靠近根。最优 BST 的输入包括成功查找权 `p[1..n]` 与失败查找权 `q[0..n]`；动态规划对每个区间尝试每一个键作根，选择「左子树代价 + 右子树代价 + 本区间总权」最小的方案。

教材样例 `p = {1,5,4,3}`、`q = {5,4,3,2,1}` 的总成本是 57，根是第 2 个键。`cost[i][j]` 是区间代价，`root[i][j]` 记录取得最小值的根，因而还能按根表重建树形。朴素实现为 O(n³)。

先跑一遍：

```cpp file=code/ch12/optimal_bst/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::advanced::ReusableNodePool<int> pool(2);
    const auto first = pool.acquire(11);
    const auto second = pool.acquire(22);
    std::cout << "申请到槽 " << *first << " 和 " << *second
              << "，剩余 " << pool.available() << '\n';
    pool.release(*first);
    const auto reused = pool.acquire(44);
    std::cout << "归还后再申请得到槽 " << *reused
              << "，值为 " << *pool.get(*reused) << '\n';

    const auto tree = dsa::advanced::optimal_bst({1, 5, 4, 3}, {5, 4, 3, 2, 1});
    std::cout << "最优 BST 总成本 " << tree.cost[0][4]
              << "，根为键 " << tree.root[0][4] << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch12/optimal_bst \
    code/ch12/optimal_bst/demo.cpp -o /tmp/bst-demo
/tmp/bst-demo
```

```console
申请到槽 0 和 1，剩余 0
归还后再申请得到槽 0，值为 44
最优 BST 总成本 57，根为键 2
```

把 `optimal_bst({1,2}, {3,4})` 这种长度对不上的输入送进去，会抛 `std::invalid_argument`。空树对应 `p = {}`、`q = {某个失败权}`，代价为 0。

池用 `vector<optional<T>>` 表示槽位占用，`vector<size_t>` 当空闲栈。构造时下标从大到小入栈，所以第一次 `acquire` 拿到 0。`release` 先确认该槽确实被占用，再 `reset` 并归还。

```cpp file=code/ch12/optimal_bst/modern.hpp#reusable-node-pool
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
```

### 12.2.3 存储的动态分配和回收

向系统要一块、用完还回去，中间可能产生外部碎片（空闲块太碎）和内部碎片（分出去的块没用完）。首次适应、最佳适应、最坏适应是三种空闲块选择策略；相邻空闲块应合并。

### 12.2.4 失败处理策略和无用单元回收

分配失败可以：拒绝、压缩、或做垃圾回收。引用计数遇循环引用会漏；标记-清除能处理环，但要能从根集走到所有活对象。本章的句柄池把「失败」显式做成 `nullopt`，不劫持全局 `new`。

## 12.3 Trie 结构和 Patricia 树

Trie 按关键码的字符（或二进制位）逐层分支，公共前缀共享路径。适合字典和最长前缀匹配。Patricia 树把只有一个孩子的内部结点压缩掉，路径更短。二者都是原书没错的结构；本仓库没有单独的可运行实现，不在这里印示意代码。

## 12.4 改进的二叉搜索树

### 12.4.1 最佳二叉搜索树

最优 BST 先校验 `q.size() == p.size() + 1`。空区间的 `cost[i][i] = 0`，`weight[i][i] = q[i]`。长度从 1 扩到 n，对每个区间 `[first, last]` 枚举根 `r`，候选代价是 `cost[first][r-1] + cost[r][last] + weight[first][last]`。

```cpp file=code/ch12/optimal_bst/modern.hpp#optimal-bst
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
```

### 12.4.2 平衡的二叉搜索树

AVL 树用平衡因子把左右高度差限制在 1 以内，插入删除后通过单旋转或双旋转恢复。查找、插入、删除都是 $O(\log n)$。红黑树是另一种近似平衡，见第 11.6 节。本章不另写未验证的 AVL 实现。

### 12.4.3 伸展树

访问一个结点后，用旋转把它搬到根附近：之字形走两步升一层，一字形走两步升两层。均摊 $O(\log n)$，不必在每个结点上存平衡信息。半伸展比全伸展少转一轮，适合连续访问同一结点。本章不另写未验证实现。

