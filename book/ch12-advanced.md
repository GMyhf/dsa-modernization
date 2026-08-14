# 第12章 高级数据结构

本章包含两个主题。可利用空间表复用固定数量的槽位：申请取得一个空闲槽，释放把它归还。最优二叉搜索树则把访问频率写成权重，用动态规划比较每个区间可能的根，得到总查找代价最小的树。

本章各节的实现状态：

| 小节 | 实现状态 | 代码 |
| --- | --- | --- |
| 12.1 多维数组、特殊矩阵、稀疏矩阵 | 概念导读 | 只给存储与定位公式，不另写十字链表 |
| 12.2.1 广义表 | 实现并测试 | `code/ch12/gen_list` |
| 12.2.2 可利用空间表、最优 BST | 实现并测试 | `code/ch12/optimal_bst` |
| 12.2.3–12.2.4 动态分配与无用单元回收 | 概念导读 | 分配策略与标记-清除只讨论 |
| 12.3 Trie 与 Patricia | 实现并测试 | `code/ch12/trie` |
| 12.4 最优 BST / AVL / 伸展树 | 混合 | 最优 BST 有实现，AVL 与伸展树为概念导读 |

其中广义表与 Trie/Patricia 原书没有给清单（第 12 章的清单只有算法12.1、算法12.2），这两个单元是新写的，不认领任何原书清单。

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

先跑一遍：

```cpp file=code/ch12/gen_list/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    using dsa::advanced::GenList;
    const GenList list = GenList::parse("(a,(b,c),d)");
    std::printf("表      : %s\n", list.to_string().c_str());
    std::printf("表头    : %s\n", list.head()->to_string().c_str());
    std::printf("表尾    : %s\n", list.tail()->to_string().c_str());
    std::printf("长度 %zu，深度 %zu，原子 %zu 个\n",
                list.length(), list.depth(), list.atom_count());

    // 再入表：同一个子表挂到两处，靠引用计数而不是拷贝。
    const GenList shared = GenList::parse("(b,c)");
    const GenList host = GenList::cons(shared, GenList::cons(shared, GenList()));
    std::printf("共享后  : %s，被引用 %zu 次\n", host.to_string().c_str(), shared.use_count());
    return 0;
}
```

```text
表      : (a,(b,c),d)
表头    : a
表尾    : ((b,c),d)
长度 3，深度 2，原子 4 个
共享后  : ((b,c),(b,c))，被引用 3 次
```

最后一行是本节的难点：`(b,c)` 只有一份，挂在两处，引用计数是 3（自己一份，两个宿主各一份）。共享一旦发生，回收就**不能再按树递归 `delete`**——那会把同一个结点删两次。所以结点上带计数，句柄负责加减：

```cpp file=code/ch12/gen_list/modern.hpp#genlist-refcount
static void retain(GenNode* node) noexcept {
    if (node != nullptr) {
        ++node->refs;
    }
}

static void release(GenNode* node) noexcept {
    // 计数归零才真正删除；共享的子表因此只会被删一次。
    while (node != nullptr && --node->refs == 0) {
        GenNode* const head = node->head;
        GenNode* const tail = node->tail;
        delete node;
        // 表尾用循环走，长表不会把栈压穿；表头递归，深度由嵌套层数决定。
        release(head);
        node = tail;
    }
}
```

`release` 沿表尾迭代、只对表头递归，因此三万个元素的长表析构不会压穿栈，栈深度只跟嵌套层数走。这里不用 `shared_ptr`：12.2 要教的就是「谁来回收共享结点」，交给标准库这一节就没了。

引用计数收不回**环**。构造函数自底向上建表，本书的接口造不出循环表；原书 12.2.4 讲的无用单元回收（标记-清扫）正是为环准备的，本书不实现，也不假装实现。

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

把 `can`、`car`、`cat`、`do` 插进一棵字母 Trie，公共前缀 `ca` 只出现一次：

```text
        ·
       / \
      c   d
      |   |
      a   o*
     /|\
    n* r* t*
```

带 `*` 的是词尾。查找 `car` 走 c–a–r，三步；查找 `cab` 在 b 处没有分支，失败。最长前缀匹配（路由）则走到不能再走为止，回退到最近的词尾。

先跑一遍。上面那棵图里的结点数不是数出来的，是程序报的：

```cpp file=code/ch12/trie/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    dsa::advanced::Trie trie;
    dsa::advanced::PatriciaTree patricia;
    for (const char* word : {"can", "car", "cat", "do"}) {
        trie.insert(word);
        patricia.insert(word);
    }
    std::printf("Trie     : %zu 个词，%zu 个结点（字符总数 11）\n",
                trie.size(), trie.node_count());
    std::printf("Patricia : %zu 个词，%zu 个内部结点\n",
                patricia.size(), patricia.internal_count());
    std::printf("前缀 ca 下有 %zu 个词：", trie.count_with_prefix("ca"));
    for (const auto& word : trie.keys_with_prefix("ca")) {
        std::printf("%s ", word.c_str());
    }
    std::printf("\n最长前缀匹配 dozen -> %s（走不动就回退到最近词尾）\n",
                trie.longest_prefix_of("dozen").c_str());
    return 0;
}
```

```text
Trie     : 4 个词，7 个结点（字符总数 11）
Patricia : 4 个词，3 个内部结点
前缀 ca 下有 3 个词：can car cat 
最长前缀匹配 dozen -> do（走不动就回退到最近词尾）
```

第一行就是前缀共享的全部价值：四个词一共 11 个字符，树里只有 7 个结点，因为 `ca` 只存了一次。最长前缀匹配的写法就是「一路往下走，随手记住最近一次经过的词尾」：

```cpp file=code/ch12/trie/modern.hpp#trie-longest-prefix
/// 最长前缀匹配：走到走不动为止，回退到最近的词尾。IP 路由查表就是这个动作。
[[nodiscard]] std::string longest_prefix_of(std::string_view text) const {
    const Node* node = &root_;
    std::size_t best = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!is_letter(text[i])) {
            break;
        }
        const Node* next = node->children[index_of(text[i])].get();
        if (next == nullptr) {
            break;
        }
        node = next;
        if (node->terminal) {
            best = i + 1;
        }
    }
    return std::string(text.substr(0, best));
}
```

纯 Trie 在「只有一个孩子」的内部结点上仍然分支，路径偏长。Patricia 树把这种单孩子结点压缩掉，边上记下「跳过几位再比」。查找时按记下的位位置取关键码的那一位，决定走左还是走右。路径更短，结点更少，仍然保持前缀共享——同样四个词，Patricia 只要 3 个内部结点。

按位取值和「两个关键码第一次不同在第几位」是 Patricia 的两块基石：

```cpp file=code/ch12/trie/modern.hpp#patricia-bits
static bool bit_of(std::string_view key, std::size_t index) noexcept {
    const std::size_t byte = index / 8;
    if (byte >= key.size()) {
        return false;  // 越过关键码长度，一律读 0
    }
    const auto value = static_cast<unsigned char>(key[byte]);
    return ((value >> (7 - index % 8)) & 1U) != 0;
}

static optional_bit first_differing_bit(std::string_view a, std::string_view b) {
    const std::size_t longest = a.size() > b.size() ? a.size() : b.size();
    const std::size_t bits = (longest + 1) * 8;  // +1 让「一个是另一个的前缀」也能分开
    for (std::size_t i = 0; i < bits; ++i) {
        if (bit_of(a, i) != bit_of(b, i)) {
            return {true, i};
        }
    }
    return {};
}
```

越过关键码长度的位一律读作 0，这样「一个关键码是另一个的前缀」（`a` 与 `ab`）也能分开，代价是关键码里不能出现 `'\0'`。还有一处不能省：沿位下降到叶之后，**必须和叶上的完整关键码再比一次**——路上只看了少数几位，不比就会把 `ca`、`cars` 这种根本不在表里的串判成命中。

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

四种情形用中序仍保持 $A<B<C$ 来记（圆括号里是子树）：

```text
LL（左左，一次右旋）          RR（右右，一次左旋）
      C                           A
     /                             \
    B               →               B
   /                                 \
  A                                   C

LR（左右，先左旋再右旋）      RL（右左，先右旋再左旋）
      C                           A
     /                             \
    A               →               B
     \                             / \
      B                           A   C
```

插入 1、2、3 会走出 RR：先得到右链 `1-2-3`，在 1 上左旋变成以 2 为根的平衡树。插入 3、2、1 是对称的 LL。插入 1、3、2 是 RL：先在 3 上右旋，再在 1 上左旋。删除一个结点后，从被删处向上检查，哪一层变成 $\pm 2$ 就在那一层做同样的旋转；有时旋转一次还不够，要继续向上走。

红黑树是另一种近似平衡，用颜色规则保证最长路径不超过最短路径的两倍，旋转次数有常数上限，见第 11.5 节。
可运行的 AVL 四旋转实现见 `code/ch12/balanced_trees/modern.hpp`，测试检查中序有序、查找、删除和高度边界。

### 12.4.3 伸展树

伸展树不在结点上存平衡因子。每次访问（查找、插入、删除）一个结点之后，用旋转把它搬到根附近：最近被用到的键下次更容易先碰到。这是一种自调整，均摊 $O(\log n)$。

从被访问结点走到根，每次看最近两步的形状。之字形（先左后右，或先右后左）走两步只升一层；一字形（连续两次同向）走两步升两层。搬到根的孩子时，再做一次单旋转即可。半伸展比全伸展少转最后一轮，连续访问同一个结点时更省。

伸展树实现简单，不必维护额外的平衡域；代价是单次操作可能仍是 $O(n)$，只是摊还之后是对数。
同一实现中的 `SplayTree` 使用 `unique_ptr` 维护子树所有权，访问后按一字形/之字形规则旋至根。

## 本章小结

多维数组按行优先或列优先顺序存放，按下标随机访问；三角、对称、稀疏矩阵可以压缩。广义表的元素可以是原子或子表，头尾分解对应递归。动态存储要处理碎片和回收：首次/最佳/最坏适应，引用计数与标记清除。Trie 按字符分支并共享前缀，Patricia 压缩单孩子路径。最优 BST 用动态规划选根；AVL 用平衡因子和四种旋转保证 $O(\log n)$；伸展树用访问后上移做均摊平衡。可利用空间表是定长结点的显式空闲栈，不劫持全局 `new`。

## 习题

### 补充概念与上机题（参考课程第 12 章）

1. 用 CSR 三数组表示稀疏矩阵，并计算 `y = A x`；复杂度按非零元数 `nnz` 表示。
2. 说明共享广义表为什么不能简单递归释放，并设计引用计数或访问标记方案。
3. 证明长度为 `N` 的字符串所有后缀插入 Trie 后，每个不同子串对应 Trie 中一个结点；分析最坏时间。
4. 实现 AVL 的四种旋转，并用中序遍历验证旋转前后序列不变。

1. 写出 $3\times 4$ 数组按行优先时 $a_{2,1}$ 相对首地址的偏移（元素占 1 个单元）。
2. 把 4 阶下三角压成一维，给出 $a_{3,1}$ 的下标。
3. 说明纯表、再入表、循环表分别对应什么图，释放时要注意什么。
4. 对空闲块 900、1500、700，各举一个只有首次适应、只有最佳适应、只有最坏适应能满足的请求。
5. 把 `can`、`car`、`cat` 插入字母 Trie，画出结果，并写出查找 `cab` 的失败路径。
6. 教材样例 $p=\{1,5,4,3\}$、$q=\{5,4,3,2,1\}$，指出最优 BST 的根和总成本。
7. 从空 AVL 依次插入 1、2、3，画出每次旋转。再插入 0，需要旋转吗。
8. 伸展树中，之字形和一字形各升几层？为什么半伸展适合连续访问同一结点。

## 上机题

1. 实现下三角矩阵的压缩存储，并支持按下标读写。
2. 用句柄池管理固定个数的结点，测试耗尽、归还、复用和重复释放。
3. 实现一棵字母 Trie 的插入和查找，用一组单词对拍 `std::set`。
4. 按教材权重实现最优 BST，输出根表并重建树形。
