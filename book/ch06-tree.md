# 第6章 树

一般树允许一个结点有任意多个孩子。本章用「左孩子、右兄弟」表示它：`child` 指向第一个孩子，`sibling` 指向下一个兄弟。并查集则回答另一类问题：若干元素分属哪些互不相交的集合。

源码：[一般树和并查集](../code/ch06/general_tree/modern.hpp)、
[可运行示例](../code/ch06/general_tree/demo.cpp)、
[测试](../code/ch06/general_tree/test.cpp)。

## 6.1 树的定义和基本术语

树形结构用分支关系定义层次：一个结点至多一个前驱，但可以有任意多个后继。家谱、机关编制、文件系统目录、编译器的句法树，都是树。和第 5 章的二叉树不同，一般树的结点不限制孩子个数。

### 6.1.1 树和森林

树是 $n$（$n\ge 1$）个结点的有穷集合 $T$，满足：有且仅有一个称为根的结点；其余结点被分成 $m$（$m\ge 0$）个互不相交的集合，每个集合又是一棵树，叫做根的子树。这个定义是递归的。

也可以用二元关系来写。结点集 $K$ 上有一个关系 $r$：恰好一个结点没有前驱，它是根；其余每个结点恰好一个前驱；从根到任意结点都存在一条由关系元组串起来的路径。

父、子、兄弟、根、叶、度、层、路径这些词，含义与二叉树相同。结点的度是孩子个数，树的度是各结点度的最大值。根在第 0 层，非根结点的层数是父结点层数加 1。

自然界里孩子的左右次序往往无所谓，这种树叫无序树。计算机存储是有序的，所以实现时通常把孩子从左到右编上号，当作有序树。注意：度为 2 的有序树还不是二叉树——删掉第一个孩子后，第二个会顶上来占第一的位置；二叉树必须能表示「左空、右不空」这种左右不对称。

森林是零棵或多棵互不相交的树的集合（通常有序）。一棵树里，某个结点的全部子树组成一个森林；给这个森林加上一个公共根，就又变成一棵树。

树有多种画法。树形表示法是根在上的倒挂树；凹入表示法用长短线表示层次，像书的目录；文氏图用嵌套圆；嵌套括号把根写在左边、子树写在括号里，例如 `A(B(D(I,J),E), C(F,G(K,L),H))`。表示法多样，说明树在建模里用得广。

例如 A 的孩子是 B、C、D，B 的孩子是 E、F。用左孩子 / 右兄弟画出来：

```text
A
└─ child → B ── sibling → C ── sibling → D
            └─ child → E ── sibling → F
```

任意度的树只需两个指针域。代价是不能 $O(1)$ 取得「第 $k$ 个孩子」，必须沿兄弟链走过去。图 6.7 就是这张图。

### 6.1.2 森林与二叉树的等价转换

树和森林都可以一对一地变成二叉树，相关操作也就都能转到二叉树上做。形象的做法是「连线、切线、旋转」三步：

1. **连线**：把兄弟结点用线连起来。
2. **切线**：只保留父结点到第一个孩子的连线，砍掉到其余孩子的连线。
3. **旋转**：以根为轴顺时针转一下，画面才像通常的二叉树。

转换后，一个结点的左孩子是它在原树（或森林）里的第一个孩子，右孩子是原来的下一个兄弟。左枝上是父子关系，右枝上是兄弟关系。单棵树的根没有兄弟，所以转成二叉树后根的右孩子一定为空。

形式地说：森林 $F=\{T_1,T_2,\ldots,T_n\}$ 转成二叉树 $B(F)$——$F$ 空则 $B(F)$ 空；否则 $B(F)$ 的根是 $T_1$ 的根，$B(F)$ 的左子树是 $T_1$ 的子树森林转成的二叉树，$B(F)$ 的右子树是 $\{T_2,\ldots,T_n\}$ 转成的二叉树。

反过来是三步的逆：逆时针旋转；若 $x$ 是 $y$ 的左孩子，就把 $x$ 以及 $x$ 右侧整条右链上的结点都补连到 $y$；再删掉所有到右孩子的边。二叉树 $B$ 对应的森林 $F(B)$：空树对应空森林；否则 $F(B)$ 是一棵以 $B$ 的根为根、以 $F(B_L)$ 为子树森林的树，再加上 $F(B_R)$。这两种转换互逆。

### 6.1.3 树的抽象数据类型

和二叉树一样，树的 ADT 分结点类和树类。对外运算是：用一个值建根、在某结点下插入第一个孩子、在某结点旁插入下一个兄弟、问父结点、删掉一棵子树、以及先根 / 后根 / 层次三种周游。原书【代码6.1】【代码6.2】只给出声明。本书把这些运算直接写在 `GeneralTree` 上，不再另设空基类。

### 6.1.4 树的周游

先根：先访问结点，再依次周游各孩子子树。后根：先周游各孩子子树，再访问结点。层次：按离根的距离一层一层走，需要队列。对本节的例子：

| 周游 | 顺序 | 本例 |
| --- | --- | --- |
| 先根 | 结点，再孩子子树 | A B E F C D |
| 后根 | 孩子子树，再结点 | E F B C D A |
| 层次 | 按离根的距离 | A B C D E F |

递归周游和递归销毁在极深的退化树上会耗尽调用栈，和第 5 章是同一类风险。

## 6.2 树的链式存储结构

一般树的存储比二叉树麻烦，因为度不固定。常见有四种想法，本章主实现是第四种。

### 6.2.1 「子结点表」表示方法

每个结点保存一块孩子指针数组（或一张表）。取第 $k$ 个孩子是 $O(1)$，按编号随机访问方便。度不固定时数组要扩容，在孩子序列中间插入也要搬指针。空间上，度为 $d$ 的结点就要留 $d$ 个指针槽，稀疏时浪费。本章不把它当主实现。

### 6.2.2 静态「左子/右兄」表示法

结点数事先已知时，不必每个结点 `new` 一次，用数组下标代替指针即可：`child`、`sibling` 存的是另一个下标，而不是地址。语义与动态版相同，只是存储从堆变成一块连续数组。适合并查集那种「一开始就知道有 $n$ 个元素」的场合。

### 6.2.3 动态表示法

每个结点单独分配，用指针相连。树的大小事先不知道、会频繁长出新结点时，比静态数组合适。后面的 `GeneralTree` 就是动态分配。

### 6.2.4 动态「左子/右兄」表示

这是本章的主实现。任意度的树只需两个指针域：`child` 指向第一个孩子，`sibling` 指向下一个兄弟。再加一个 `parent` 便于向上走。代价是取第 $k$ 个孩子必须沿兄弟链走 $k$ 步。

```cpp file=code/ch06/general_tree/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::GeneralTree<char> tree;
    tree.create_root('A');
    auto* b = tree.insert_first(tree.root(), 'B');
    auto* c = tree.insert_next(b, 'C');
    tree.insert_next(c, 'D');
    tree.insert_first(b, 'E');
    tree.insert_next(tree.root()->child->child, 'F');

    std::cout << "先根: ";
    tree.preorder([](char value) { std::cout << value; });
    std::cout << "\n后根: ";
    tree.postorder([](char value) { std::cout << value; });
    std::cout << "\n层次: ";
    tree.breadth_first([](char value) { std::cout << value; });
    std::cout << '\n';

    dsa::DisjointSet sets(5);
    sets.unite(0, 1);
    sets.unite(1, 2);
    sets.unite(3, 4);
    std::cout << "0 与 2 同集合: " << (sets.same(0, 2) ? "是" : "否") << '\n';
    std::cout << "0 与 3 同集合: " << (sets.same(0, 3) ? "是" : "否") << '\n';
}
```

在仓库根目录运行：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch06/general_tree \
    code/ch06/general_tree/demo.cpp -o /tmp/tree-demo
/tmp/tree-demo
```

输出是：

```console
先根: ABEFCD
后根: EFBCDA
层次: ABCDEF
0 与 2 同集合: 是
0 与 3 同集合: 否
```

`insert_first(parent, value)` 把新结点插到孩子链的最前面；`insert_next(node, value)` 插到该结点的下一个兄弟。先插入 B，再 `insert_next(B, C)`、`insert_next(C, D)`，孩子从左到右就是 B、C、D。

`create_root` 清空旧树并新建根。`insert_first` 先让新结点的 `sibling` 指向原来的第一个孩子，再把它接到 `parent->child` 上——所以后插入的孩子会出现在兄弟链前端。`delete_subtree` 沿着父的孩子链或森林的根兄弟链找到指向它的指针，改写成下一个兄弟，再递归销毁。

孩子-兄弟树：

```cpp file=code/ch06/general_tree/modern.hpp#general-tree
template <typename T>
class GeneralTree {
public:
    struct Node {
        T value;
        Node* child{nullptr};
        Node* sibling{nullptr};
        Node* parent{nullptr};

        explicit Node(const T& value) : value(value) {}
    };

    GeneralTree() = default;

    GeneralTree(const GeneralTree& other) : root_(clone(other.root_, nullptr)) {}

    GeneralTree& operator=(const GeneralTree& other) {
        if (this != &other) {
            GeneralTree copy(other);
            swap(copy);
        }
        return *this;
    }

    GeneralTree(GeneralTree&& other) noexcept : root_(other.release()) {}

    GeneralTree& operator=(GeneralTree&& other) noexcept {
        if (this != &other) {
            clear();
            root_ = other.release();
        }
        return *this;
    }

    ~GeneralTree() { clear(); }

    void swap(GeneralTree& other) noexcept {
        using std::swap;
        swap(root_, other.root_);
    }

    [[nodiscard]] Node* root() noexcept { return root_; }
    [[nodiscard]] const Node* root() const noexcept { return root_; }

    void create_root(const T& value) {
        clear();
        root_ = new Node(value);
    }

    Node* insert_first(Node* parent, const T& value) {
        if (parent == nullptr) {
            throw std::invalid_argument("parent must not be null");
        }
        Node* node = new Node(value);
        node->sibling = parent->child;
        node->parent = parent;
        parent->child = node;
        return node;
    }

    Node* insert_next(Node* node, const T& value) {
        if (node == nullptr) {
            throw std::invalid_argument("node must not be null");
        }
        Node* next = new Node(value);
        next->sibling = node->sibling;
        next->parent = node->parent;
        node->sibling = next;
        return next;
    }

    [[nodiscard]] Node* parent_of(Node* node) const noexcept {
        return node == nullptr ? nullptr : node->parent;
    }

    void delete_subtree(Node* node) {
        if (node == nullptr) {
            return;
        }

        Node** link = node->parent == nullptr ? &root_ : &node->parent->child;
        while (*link != nullptr && *link != node) {
            link = &(*link)->sibling;
        }
        if (*link != node) {
            throw std::invalid_argument("node is not part of this tree");
        }

        *link = node->sibling;
        node->sibling = nullptr;
        destroy(node);
    }

    void clear() noexcept {
        destroy(root_);
        root_ = nullptr;
    }

    template <class Visitor>
    void preorder(Visitor&& visitor) const {
        pre(root_, visitor);
    }

    template <class Visitor>
    void postorder(Visitor&& visitor) const {
        post(root_, visitor);
    }

    template <class Visitor>
    void breadth_first(Visitor&& visitor) const {
        std::vector<Node*> queue;
        for (Node* node = root_; node != nullptr; node = node->sibling) {
            queue.push_back(node);
        }
        for (std::size_t index = 0; index < queue.size(); ++index) {
            visitor(queue[index]->value);
            for (Node* child = queue[index]->child; child != nullptr;
                 child = child->sibling) {
                queue.push_back(child);
            }
        }
    }

private:
    // Recursive destruction and traversals preserve the textbook presentation.
    // They have a Stack Overflow Risk for a pathologically deep tree.
    static void destroy(Node* node) noexcept {
        if (node == nullptr) {
            return;
        }
        destroy(node->child);
        destroy(node->sibling);
        delete node;
    }

    static Node* clone(const Node* node, Node* parent) {
        if (node == nullptr) {
            return nullptr;
        }
        Node* copy = new Node(node->value);
        copy->parent = parent;
        try {
            copy->child = clone(node->child, copy);
            copy->sibling = clone(node->sibling, parent);
        } catch (...) {
            destroy(copy);
            throw;
        }
        return copy;
    }

    template <class Visitor>
    static void pre(Node* node, Visitor& visitor) {
        for (; node != nullptr; node = node->sibling) {
            visitor(node->value);
            pre(node->child, visitor);
        }
    }

    template <class Visitor>
    static void post(Node* node, Visitor& visitor) {
        for (; node != nullptr; node = node->sibling) {
            post(node->child, visitor);
            visitor(node->value);
        }
    }

    Node* release() noexcept {
        Node* result = root_;
        root_ = nullptr;
        return result;
    }

    Node* root_{nullptr};
};
```

### 6.2.5 父指针表示法和并查集

`find` 在返回根之前把沿途结点的父指针改成根，这就是路径压缩。`unite` 比较两棵树的秩，把较矮的挂到较高的下面；两棵一样高时，新根的秩加 1。

```cpp file=code/ch06/general_tree/modern.hpp#disjoint-set
class DisjointSet {
public:
    explicit DisjointSet(std::size_t count) : parent_(count), rank_(count, 0) {
        for (std::size_t index = 0; index < count; ++index) {
            parent_[index] = index;
        }
    }

    std::size_t find(std::size_t index) {
        if (index >= parent_.size()) {
            throw std::out_of_range("disjoint-set index");
        }
        if (parent_[index] != index) {
            parent_[index] = find(parent_[index]);
        }
        return parent_[index];
    }

    bool unite(std::size_t left, std::size_t right) {
        left = find(left);
        right = find(right);
        if (left == right) {
            return false;
        }
        if (rank_[left] < rank_[right]) {
            std::swap(left, right);
        }
        parent_[right] = left;
        if (rank_[left] == rank_[right]) {
            ++rank_[left];
        }
        return true;
    }

    [[nodiscard]] bool same(std::size_t left, std::size_t right) {
        return find(left) == find(right);
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> rank_;
};
```

## 6.3 树的顺序存储结构

链式存储每个结点单独分配，指针占空间，也不利于整块读写。如果把结点按某一种周游次序排进数组，再用很少的附加信息记下「谁是下一个兄弟」或「有几个孩子」，就能在连续空间里还原整棵树。原书给了四种，思路都对；本章不另写一套未验证的实现，只把还原办法说清楚。

### 6.3.1 带右链的先根次序表示

按先根次序把结点排进数组。每个结点另存一个「右链」下标，指向它的下一个兄弟；没有下一个兄弟就存空。因为先根次序里，一个结点的孩子们正好排在它后面、直到右链所指的位置之前，所以顺着数组往下走、再靠右链跳过整棵子树，就能还原所有父子和兄弟关系。

### 6.3.2 带双标记的先根次序表示

右链要用一个整型下标。若改成两个布尔标记——「有没有孩子」「有没有下一个兄弟」——每位只占 1 bit，更省。扫描时用栈：遇到「有孩子」就下降；遇到「没有下一个兄弟」就上升到最近那个还没处理完兄弟的祖先。信息量和带右链相同，只是用栈换空间。

### 6.3.3 带度数的后根次序表示

按后根次序存放，每个结点记下自己的度数（孩子个数）。后根的特点是：一个结点出现时，它的全部孩子已经作为连续的一段排在它前面。扫描时用栈，每读到一个度数为 $d$ 的结点，就从栈顶弹出 $d$ 棵子树做它的孩子，再把这棵新树压回去。扫完数组，栈里剩下的就是整棵树（或森林）。

### 6.3.4 带双标记的层次次序表示

按层次周游的次序存放，标记含义与 6.3.2 相同：有没有孩子、有没有下一个兄弟。因为同一层的兄弟本来就排在一起，一层扫完再扫下一层，用队列就能还原。适合需要按层处理的外部存储。

## 6.4 K 叉树

有些应用里每个结点的孩子数有固定上限 $K$，例如三子棋的博弈树、某些 B 树的内存模拟。这时不必走「任意度 + 兄弟链」，可以规定每个结点至多 $K$ 个孩子，叫做 $K$ 叉树。

满 $K$ 叉树、完全 $K$ 叉树的定义与二叉树平行：满 $K$ 叉树的每个结点要么是叶，要么恰好 $K$ 个孩子；完全 $K$ 叉树只有最下两层的度可以小于 $K$，且最下层靠左对齐。按层从 0 编号时，结点 $i$ 的孩子们是 $Ki+1,\ldots,Ki+K$，父结点是 $\lfloor(i-1)/K\rfloor$。$K=2$ 就回到二叉树。

本章主实现仍是任意度的左子 / 右兄，不单独做一份 $K$ 叉数组。需要固定 $K$、又想顺序存放时，用上面的编号公式即可。

## 本章小结

一般树允许任意多个孩子。森林是若干互不相交的树。树和森林与二叉树可以按「长子—左、兄弟—右」一一转换。链式存储里，左子/右兄用两个指针表示任意度；顺序存储则按某种周游次序排进数组，再靠右链、双标记或度数还原树形。并查集用父指针表示集合，路径压缩和按秩合并让大量操作接近常数。$K$ 叉树是度有上限的特例。

## 习题

### 补充证明与算法题（参考课程第 6 章）

1. 高度为 `h` 的满 `k` 叉树按层编号，推导第 `l` 层结点数、结点 `i` 的第 `m` 个孩子编号及右兄弟条件。
2. 用并查集判断变量方程组 `a==b`、`a!=b` 是否有解，并分别分析无优化、按秩合并和路径压缩的复杂度。
3. 给定一棵树的左孩子/右兄弟表示，写出其森林的先根序列和带度数后根序列。

1. 画出三棵树组成的森林转换成的二叉树，再转换回去，验证互逆。
2. 对图 6.7 那棵树写出先根、后根、层次序列。
3. 度为 2 的有序树和二叉树差在哪里？举一个「左空右不空」的例子。
4. 用带度数的后根次序表示一棵小树，并说明扫描时栈如何弹出孩子。
5. 对元素 $0..6$ 依次 `unite(0,1)`、`unite(1,2)`、`unite(3,4)`，画出父指针，再 `find(0)` 后画出路径压缩的结果。
6. 完全 3 叉树按层编号，写出结点 4 的父和孩子们。

## 上机题

1. 实现森林与二叉树的互相转换，并用先根序列对拍。
2. 用并查集判断无向图是否连通，并统计连通分量个数。
3. 比较带路径压缩与不带路径压缩的 `find` 在退化链上的时间。
