# 第12章 高级数据结构

本章包含两个主题。可利用空间表复用固定数量的槽位：申请取得一个空闲槽，释放把它归还。最优二叉搜索树则把访问频率写成权重，用动态规划比较每个区间可能的根，得到总查找代价最小的树。

源码：[空闲槽池与最优 BST](../code/ch12/optimal_bst/modern.hpp)、
[可运行示例](../code/ch12/optimal_bst/demo.cpp)、
[测试](../code/ch12/optimal_bst/test.cpp)。

## 12.1 多维数组

多维数组是一维数组的扩充：数组的数组就是二维，二维数组再排成一列就是三维。结构上的特点是，元素本身还可以有结构，但同一数组里的元素类型相同。元素个数相对固定，一旦生成通常只改值、不改相对位置和个数，因此自然采用顺序存储。

### 12.1.1 多维数组的存储

C++ 里每一维的下界都是 0。二维数组既可以看成 $m$ 个行向量组成的向量，也可以看成 $n$ 个列向量组成的向量。每个元素 $a_{i,j}$ 同时属于第 $i$ 行和第 $j$ 列，最多有两个前驱、两个后继。推到 $k$ 维，每个元素属于 $k$ 个向量。

把数组按某种周游次序排成线性序列，就可以顺序存放。C++ 和 Pascal 按**行优先**：先排最右的下标，从右往左排。二维数组排出来是

$$a_{0,0},a_{0,1},\ldots,a_{0,n-1},a_{1,0},\ldots,a_{m-1,n-1}$$

$d_0\times d_1\times\cdots\times d_{n-1}$ 数组中，元素 $A[j_0,\ldots,j_{n-1}]$ 相对首地址的偏移是

$$
d\cdot\Bigl(\sum_{i=0}^{n-2} j_i\prod_{k=i+1}^{n-1}d_k + j_{n-1}\Bigr)
$$

其中 $d$ 是一个元素所占单元数。每个元素的定位时间相同，所以这是随机存储结构。FORTRAN 按**列优先**，先排最左下标，公式左右对调。

### 12.1.2 特殊矩阵

矩阵常用二维数组表示，但有些矩阵里大量元素是 0 或同一个常数，不必存整块。

**三角矩阵。** $n$ 阶上三角（或下三角）里，对角线一侧全是 0 或常数 $c$。只需存另一侧加上那个常数，一共 $(n^2+n)/2$ 个单元。若用一维数组 `list[0 .. (n²+n)/2-1]` 存下三角，元素 $a_{i,j}$（$i\ge j$）前面有 $i$ 行、共 $(i^2+i)/2$ 个非零元，再加本行的 $j$，下标就是 $(i^2+i)/2+j$。

**对称矩阵。** $a_{i,j}=a_{j,i}$。只存下三角（含对角线），另一半用对称关系映射。`list` 与 $a_{i,j}$ 的对应是：$i\ge j$ 时下标 $(i^2+i)/2+j$，否则 $(j^2+j)/2+i$。

### 12.1.3 稀疏矩阵

非零元很少、分布又不规则时，叫稀疏矩阵。$m\times n$ 的矩阵里有 $t$ 个非零元，稀疏因子 $\delta=t/(mn)$；通常 $\delta<0.05$ 就按稀疏处理。这时不应再分配整块二维数组，而改存三元组 `(行, 列, 值)` 的线性表，或用十字链表：每个非零元同时挂在行链表和列链表上，便于按行、按列遍历。本章不另写未验证的十字链表实现。

## 12.2 广义表和存储管理

线性表的元素是不可再分的原子。广义表放宽这条：元素可以是原子，也可以是另一个广义表。按是否共享、是否有环，分成三种：纯表对应树（每个子表只出现一次）；再入表对应有向无环图（子表可以被共享）；循环表对应有环图。存储上常用带头尾指针的结点；表一旦共享，释放时就必须处理别名，不能只按树来递归 `delete`。

### 12.2.1 广义表的定义和存储结构

表头是第一个元素，表尾是去掉表头后剩下的那个表。空表既没有头也没有尾。任何一个非空广义表都可以唯一地拆成「头 + 尾」，所以递归算法写成「先处理头、再处理尾」就和定义对上了。

结点通常有一个标记位，区分原子和子表：原子结点存值，子表结点存指向另一个表的指针。再加表头结点，可以把空表和共享关系表示得更干净。原书图 12.7–12.9 画的就是无头结点、带头结点、以及带循环的几种。

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

程序运行中向系统要一块、用完还回去。空闲块和已分配块穿插之后，会出现两种碎片：外部碎片是空闲区被切得太碎，哪一块都装不下新请求；内部碎片是分出去的块比实际需要大，多出来的字节浪费在块内。

选哪一块空闲区，有三种经典策略。首次适应从低地址扫到第一块够大的；最佳适应找刚好够用的最小块，留下的碎片更碎；最坏适应找当前最大的块，希望剩下的还大到能再用。无论哪种，相邻的空闲块都应当合并，否则外部碎片只会越来越多。边界标记（块头块尾记下大小和忙闲）让合并可以在常数时间完成。

### 12.2.4 失败处理策略和无用单元回收

分配失败时可以：直接拒绝；压缩（把已分配块搬到一起，挤出一整块空闲）；或者做垃圾回收，把程序已经够不着、却还占着的块收回来。

引用计数给每个对象记有多少指针指着它，减到零就释放。实现简单，但对象互相指形成环时，计数永远掉不到零，这块内存就漏了。标记–清除从一组根（栈上的指针、全局变量）出发走遍所有还能碰到的对象并打上标记，然后扫一遍堆，没标记的统统回收。它能处理环，但要求能从根集走到每一个活对象，并且要能区分「这是指针」和「这只是一个看起来像地址的整数」。

本章的句柄池把「失败」做成返回 `nullopt`，不劫持全局 `new`，也不假装自己是通用垃圾回收器。

## 12.3 Trie 结构和 Patricia 树

二叉搜索树按整个关键码比较，树的形状依赖插入次序。Trie 换一种切法：按关键码的字符（或二进制位）一层一层分支，第 $i$ 层对应第 $i$ 个字符。公共前缀在树里只存一次，所以它特别适合字典、IP 路由这种「按前缀分类」的问题。查找沿着字符走，时间与关键码长度成正比，与表里有多少个词关系不大。

纯 Trie 在「只有一个孩子」的内部结点上仍然分支，路径偏长。Patricia 树（Practical Algorithm to Retrieve Information Coded in Alphanumeric）把这种单孩子结点压缩掉，边上记下「跳过几位再比」。查找时按记下的位位置取关键码的那一位，决定走左还是走右。路径更短，结点更少，仍然保持前缀共享。二者都是原书没错的结构；本仓库没有单独的可运行实现，这里不印示意代码。

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

普通 BST 按插入次序生长，若键已经有序，会退化成一条链，查找变成 $O(n)$。平衡二叉搜索树在每次插入、删除之后做少量旋转，把左右子树的高度差限制住，从而保证树高 $O(\log n)$。

AVL 树给每个结点记一个平衡因子：右子树高度减左子树高度，只允许 $-1$、$0$、$1$。插入或删除使某个祖先的因子变成 $\pm 2$ 时，按「哪边沉、沉在内侧还是外侧」分成四种情形。外侧失衡一次单旋转就能拉平；内侧失衡要先把孙子转到孩子、再把孩子转到祖先，即双旋转。旋转是局部的，不改变中序次序。查找、插入、删除最坏都是 $O(\log n)$。

红黑树是另一种近似平衡，用颜色规则保证最长路径不超过最短路径的两倍，旋转次数有常数上限，见第 11.5 节。二者都是原书没错的结构；本章不另写未验证的 AVL 实现。

### 12.4.3 伸展树

伸展树不在结点上存平衡因子。每次访问（查找、插入、删除）一个结点之后，用旋转把它搬到根附近：最近被用到的键下次更容易先碰到。这是一种自调整，均摊 $O(\log n)$。

从被访问结点走到根，每次看最近两步的形状。之字形（先左后右，或先右后左）走两步只升一层；一字形（连续两次同向）走两步升两层。搬到根的孩子时，再做一次单旋转即可。半伸展比全伸展少转最后一轮，连续访问同一个结点时更省。

伸展树实现简单，不必维护额外的平衡域；代价是单次操作可能仍是 $O(n)$，只是摊还之后是对数。本章不另写未验证实现。

