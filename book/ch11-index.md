# 第11章 索引技术

索引不是保存完整记录的主文件，而是一张「关键码到记录位置」的查找表。它的目标是在海量外存记录中避免逐条扫描：先查较小的索引，再按位置读取目标记录。

原书本章没有独立的 `【算法】` / `【代码】` 清单——`dsa_raw.md` 里 105 条清单，第 11 章一条都没有。本章的实现因此全部是新写的，不认领任何原书清单，台账等式不受影响。四个小节各有一个通过闸门的实现单元：

| 小节 | 实现状态 | 代码 |
| --- | --- | --- |
| 11.1 线性索引 | 实现并测试 | `code/ch11/linear_index` |
| 11.2 静态多分树 | 实现并测试 | `code/ch11/bplus_tree` 的 `bulk_load` |
| 11.3 倒排索引 | 实现并测试 | `code/ch11/inverted_index` |
| 11.4 B 树与 B+ 树 | 实现并测试 | `code/ch11/bplus_tree` |
| 11.5 位图、签名 | 实现并测试 | `code/ch11/bitmap_index` |
| 11.5 红黑树 | 概念导读 | 只作对照讨论，不另写实现 |

这些都是**内存里的页模拟**：结点即页，`page_reads()` / `page_writes()` 数的是页访问次数。它们不做真实磁盘 I/O、页缓存和并发控制——本章要教的是访外次数怎么被结构决定，不是写一个存储引擎。

## 11.1 线性索引

第 1 章和第 9 章已经说过：外存上逐条扫描主文件太贵，应该先查一张较小的表，再按位置读记录。线性索引就是这张表：项是 `(key, location)`，按 key 排序，可以在内存里二分。

稠密索引为每条记录建一项，主文件可以无序，查到索引项就直接得到记录位置。稀疏索引只为每个有序数据块建一项，通常记下该块的最小 key；查到块之后还要在块内再找一次。稀疏索引更省空间，但要求主文件按 key 分块有序。

![图11.1 线性索引与不等长记录](assets/6e8a0ef3ed26c107.jpg)

图11.1中记录长度不同，不能用“基地址 + 下标 × 固定长度”直接计算位置。索引项保存关键码和记录地址，把逻辑顺序与物理布局分开；主文件移动后要更新地址，索引本身不保存完整记录。

第一层索引太大、内存放不下时，再为它建一层，成为多级索引。一次查询的路径是：内存里的顶层 → 读一个索引页 → 读数据页。这时的关键指标不是 CPU 比较次数，而是磁盘页访问次数。变长记录尤其适合「索引 + 位置」，因为主文件无法按下标直接算出地址。

先跑一遍：

```cpp file=code/ch11/linear_index/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    using dsa::index::IndexKind;
    using dsa::index::MultiLevelIndex;
    std::vector<std::pair<int, std::string>> records;
    for (int i = 0; i < 1000; ++i) {
        records.emplace_back(i * 10, std::string(static_cast<std::size_t>(i % 7) + 1, 'x'));
    }

    // 每个数据页 4 条记录，每个索引页 4 项。
    MultiLevelIndex sparse(IndexKind::Sparse, 4, 4);
    sparse.load(records);
    sparse.reset_counters();
    const bool hit = sparse.find(5000).has_value();
    std::printf("稀疏 · 每页 4 项 : %zu 个数据页，%zu 个索引项，%zu 层，查一次读 %zu 页（命中 %d）\n",
                sparse.data_pages(), sparse.entries(), sparse.levels(), sparse.page_reads(),
                static_cast<int>(hit));

    // 索引页装得多，层数就少，访外次数随之下降——这就是 11.2 要做多分树的理由。
    MultiLevelIndex flat(IndexKind::Sparse, 4, 64);
    flat.load(records);
    flat.reset_counters();
    (void)flat.find(5000);
    std::printf("稀疏 · 每页 64 项: %zu 层，查一次读 %zu 页\n", flat.levels(), flat.page_reads());

    MultiLevelIndex dense(IndexKind::Dense, 4, 64);
    dense.load(records);
    dense.reset_counters();
    (void)dense.find(5005);  // 不存在
    const std::size_t miss_reads = dense.page_reads();
    dense.reset_counters();
    (void)dense.find(5000);  // 存在
    std::printf("稠密 · %zu 层      : 查不到读 %zu 页，命中读 %zu 页——"
                "多出来的那一页就是数据页，索引里没有就不必去读\n",
                dense.levels(), miss_reads, dense.page_reads());
    return 0;
}
```

```text
稀疏 · 每页 4 项 : 250 个数据页，250 个索引项，4 层，查一次读 4 页（命中 1）
稀疏 · 每页 64 项: 2 层，查一次读 2 页
稠密 · 2 层      : 查不到读 1 页，命中读 2 页——多出来的那一页就是数据页，索引里没有就不必去读
```

三行输出对应三个结论。第一行：索引项一页放不下就逐层上建，查一次的代价等于层数。第二行：**索引页装得多，层数就少，访外次数随之下降**——这正是下一节要做多分树的理由。第三行是稠密与稀疏最实用的差别：稠密索引在索引里查不到就是真没有，一个数据页都不用读；稀疏索引只能定位到页，**查不到也得先把那一页读上来**才知道。

#### 从 1000 条记录算出四层索引

示例中每个数据页放 4 条记录，因此主文件有 $\lceil1000/4\rceil=250$ 个数据页。稀疏索引每页一项，第一层恰有 250 项；索引页也只放 4 项时，自底向上各层页数为：

| 层 | 项数 | 占用页数 | 上一层要为它建多少项 |
| ---: | ---: | ---: | ---: |
| 数据页 | 1000 条记录 | 250 | 250 |
| 一级索引 | 250 | 63 | 63 |
| 二级索引 | 63 | 16 | 16 |
| 三级索引 | 16 | 4 | 4 |
| 顶层索引 | 4 | 1 | 常驻内存 |

一次命中从顶层定位后，要读三级、二级、一级索引中的各一页，再读数据页，共 4 页。把每个索引页容量从 4 提高到 64 后，250 项只占 4 页，再用 1 个顶层页索引它们；查询因此只读 1 个下层索引页和 1 个数据页。

![图 11.2 二级索引文件](assets/0c75a3db9929b61e.jpg)

页内用顺序查还是二分查，只改变 CPU 比较次数，不改变“这一页已经读进来”这个事实。优化存储索引时先降树高和随机 I/O，再讨论页内比较，顺序不能倒过来。

查找过程写出来就是「逐层读索引页，最后读一个数据页」：

```cpp file=code/ch11/linear_index/modern.hpp#index-find
[[nodiscard]] std::optional<std::string> find(int key) const {
    if (levels_.empty()) {
        return std::nullopt;
    }
    // 顶层常驻内存：定位到它所在的那一页，不计页访问。
    std::size_t page = locate(levels_.back(), 0, levels_.back().size(), key);
    for (std::size_t level = levels_.size() - 1; level > 0; --level) {
        page = levels_[level][page].target;
        ++reads_;  // 读一个下层索引页
        const std::size_t first = page * entries_per_page_;
        const std::size_t last = std::min(first + entries_per_page_, levels_[level - 1].size());
        if (first >= last) {
            return std::nullopt;
        }
        page = locate(levels_[level - 1], first, last, key);
    }

    const Entry& entry = levels_[0][page];
    if (kind_ == IndexKind::Dense) {
        // 稠密索引：索引里没有就是真没有，数据页一次都不用读。
        if (entry.key != key) {
            return std::nullopt;
        }
        ++reads_;
        return records_[entry.target].second;
    }
    // 稀疏索引：只能定位到页，页内还要再找一次；不命中也已经付出了这一页。
    ++reads_;
    const std::size_t first = entry.target * records_per_page_;
    const std::size_t last = std::min(first + records_per_page_, records_.size());
    for (std::size_t i = first; i < last; ++i) {
        if (records_[i].first == key) {
            return records_[i].second;
        }
    }
    return std::nullopt;
}
```

## 11.2 静态索引——多分树

磁盘按页读写，一个索引结点通常设计成恰好装满一页。二叉树每个结点只有两个孩子，同样多的 key 会把树撑得很高，查询就要读很多页。多分树每个结点有许多孩子，树高低，页访问少。

静态多分树在批量装入时按页填满，之后结构不再变。查找从根走到叶，每层读一页。插入、删除会破坏「一页正好满」的约定：多出来的记录只能进溢出区，少了的页会变稀，最终往往要重组整棵树。所以它适合「一次建好、很少改」的文件，不适合频繁更新的目录。

![图 11.3 多分树](assets/879fc43030b02cb8.jpg)

多分树降低的是高度。若每个结点最多有 $m$ 个孩子、树高为 $h$，理想满树可导航约 $m^h$ 个叶页；同样覆盖一百万个叶页，二叉树约需 20 层，100 路树只需 3 层。页内要比较更多分界 key，但整页已经在内存里，代价通常远小于多读一次外存页。

扇出不是随意选的。页大小 4096 字节，若每个“关键码 + 孩子页号”占 16 字节，扣除页头后大约能放 250 项，扇出约 251；关键码变长或页头元数据增加，扇出就会下降。数据库索引喜欢短关键码，不只为省总空间，也为让一页容纳更多分支、压低树高。

批量装入本身就是「按页填满、自底向上建层」，`code/ch11/bplus_tree` 的 `bulk_load` 实现的正是这件事——把下一节那棵 3 阶 B+ 树按这个办法装出来，得到的就是 11.4 图里的形状。区别只在后续：静态多分树靠重组，B+ 树靠分裂与合并就地维护。

批量装入前必须按 key 排好记录，然后连续填叶页，再用每个孩子的最小 key 建上一层。这样叶页天然相邻，范围扫描接近顺序 I/O。若一条条随机插入再指望得到相同布局，结点会因分裂留下空隙，树形和页利用率都不同。

## 11.3 倒排索引

前面的索引都是从记录走到属性：先找到学号 0310，再看他是不是计算机系。倒排反过来：从属性值走到记录集合。例如

```text
计算机系 -> [0310, 0330, 0341]
英语专长 -> [0310, 0421]
```

「计算机系且擅长英语」就是两个有序列表的交集，结果是 `[0310]`。或查询是并集，差查询是差集。全文检索用同一结构：词项映射到「哪些文档、出现在哪些位置」。短语查询还要用位置是否相邻来过滤。

倒排把查询变成集合运算，速度很快；代价是每次插入、删除、修改记录，都要同步维护所有相关列表。倒排表通常只存标识和位置，完整记录仍在主文件里。

先跑一遍：

```cpp file=code/ch11/inverted_index/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    dsa::index::InvertedIndex index;
    index.add_document(310, {"计算机系", "英语专长"});
    index.add_document(330, {"计算机系"});
    index.add_document(341, {"计算机系"});
    index.add_document(421, {"英语专长"});

    const auto show = [](const char* label, const std::vector<int>& docs) {
        std::printf("%s:", label);
        for (const int doc : docs) {
            std::printf(" %04d", doc);
        }
        std::printf("\n");
    };
    show("计算机系          ", index.postings("计算机系"));
    show("英语专长          ", index.postings("英语专长"));
    show("计算机系且擅长英语", index.and_query({"计算机系", "英语专长"}));
    show("计算机系或擅长英语", index.or_query({"计算机系", "英语专长"}));
    show("不擅长英语        ", index.not_query("英语专长"));

    dsa::index::InvertedIndex text;
    text.add_document(1, {"the", "quick", "brown", "fox"});
    text.add_document(2, {"the", "brown", "quick", "fox"});
    show("含 quick 与 brown ", text.and_query({"quick", "brown"}));
    show("短语 quick brown  ", text.phrase_query({"quick", "brown"}));
    return 0;
}
```

```text
计算机系          : 0310 0330 0341
英语专长          : 0310 0421
计算机系且擅长英语: 0310
计算机系或擅长英语: 0310 0330 0341 0421
不擅长英语        : 0330 0341
含 quick 与 brown : 0001 0002
短语 quick brown  : 0001
```

最后两行是短语查询存在的理由：2 号文档两个词都有，但不相邻，只有位置信息能把它排除掉。

#### 两个指针怎样求交

对倒排表 `A=[310,330,341]` 与 `B=[310,421]` 求交：

| 步 | `A[i]` | `B[j]` | 动作 | 输出 |
| ---: | ---: | ---: | --- | --- |
| 1 | 310 | 310 | 相等，两边都前进 | 310 |
| 2 | 330 | 421 | 左边小，只移动 `i` | 310 |
| 3 | 341 | 421 | 左边小，只移动 `i` | 310 |
| 4 | 耗尽 | 421 | 停止 | 310 |

每次至少有一个指针前进，所以总步数不超过两张表长度之和。不能对 A 的每个文档号都从头扫描 B，那会把 $O(n+m)$ 退化成 $O(nm)$。

多词 AND 查询还要决定合并顺序。先交最短的倒排表，往往能尽早把候选集缩小；例如三张表长度分别为 10、1000、100000，先合并后两张会制造一个很大的中间结果。OR 查询则要在归并时去重，NOT 查询必须相对于“全部文档集合”求差，单独一张倒排表无法知道哪些文档不存在该词。

短语查询的倒排项不能只存文档号，还要存词在文档中的位置。查询 `quick brown` 时，文档 1 中位置可以是 `(1,2)`，相差 1，保留；文档 2 中若为 `(2,1)`，两个词都出现却次序相反，应排除。先按文档号求交，再在候选文档内检查位置，可避免读取无关位置表。

倒排表按文档号升序，所以求交就是一次归并，两个指针各走一趟，代价 $O(n+m)$：

```cpp file=code/ch11/inverted_index/modern.hpp#inverted-intersect
/// 有序表求交。归并一遍，两个指针各走一趟，代价 O(n+m)。
[[nodiscard]] static std::vector<int> intersect(const std::vector<int>& left,
                                                const std::vector<int>& right) {
    std::vector<int> out;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < left.size() && j < right.size()) {
        if (left[i] < right[j]) {
            ++i;
        } else if (right[j] < left[i]) {
            ++j;
        } else {
            out.push_back(left[i]);
            ++i;
            ++j;
        }
    }
    return out;
}
```

这段归并是本节的核心，所以要自己写；换成一行库调用，这一节就没有内容了。

## 11.4 动态索引

### B 树与 B+ 树

B 树是保持平衡的多路搜索树，专门为磁盘页设计。一个结点里有多个有序 key，key 之间是指向孩子的指针。查找从根开始，在页内确定下一条分支，再读孩子页。树高随阶数增大而降低，一次查找的页数大约是 $\log_m n$。

![图 11.4 B 树结点的一般形式](assets/97a7189d3ea9584c.jpg)

一个含 $r$ 个关键码的内部结点有 $r+1$ 个孩子：最左孩子的键都小于第一个关键码，相邻两个关键码之间各管一个区间，最右孩子的键都大于最后一个关键码。页内先定位区间，再沿对应页号下降；若孩子数与关键码数不满足“多一个”，结点结构已经损坏。

![图 11.5 3 阶 B 树](assets/combined/fig-11-5.png)

图 11.5　同一棵 3 阶 B 树的三种画法：(a) 通常的画法；(b) 把外部空结点画出来；(c) 画出隐含指针——$a_1 \sim a_{16}$ 是关键码在主文件里对应磁盘块的索引地址。**B 树的关键码在内部结点上也带着数据地址**，这正是它与后面 B+ 树的分水岭。

插入使结点超过上限（溢出）时，把它分裂成两个，并把分界 key 上推到父结点；父结点也可能接着分裂，最坏会一直裂到根，树增高一层。

![图11.6 在图 11.5 的 3 阶 B 树中插入关键码 14、55 后的结果](assets/3032fa408668c89e.jpg)

图 11.6　在图 11.5 的 3 阶 B 树里插入 14、55。两次插入都能就地放下，树高不变——**大多数插入就是这样结束的**。

![图11.7 在图11.5 的 3 阶 B 树中插入关键码 19 后导致根分裂](assets/ea9001b8f902803b.jpg)

图 11.7　再插入 19 就不行了：叶溢出、分裂、分界 key 上推，父结点跟着溢出，一路裂到根，树**增高一层**。B 树只在这一种情况下长高，所以它的所有叶永远在同一层。

删除使结点低于下限（下溢）时，先向左右兄弟借 key；借不到就把两个结点合并，分界 key 从父结点拿下来。借和合并都可能向上传播。

![图11.8 5 阶 B 树，连续删除关键码 120、150](assets/combined/fig-11-8.png)

图 11.8　在一棵 (a) 5 阶 B 树上连续删除：(b) 删 120——先与中序后继交换再删，删后下溢，向左邻**借**一个关键码；(c) 接着删 150——同样先交换，删后下溢而两侧都借不到，只能**合并**，合并又让父结点下溢，一路传到根。

这些局部调整保证所有叶在同一层，所以 B 树始终平衡。

B+ 树把记录（或记录位置）全部放在叶上，内部结点只作路标，不存完整记录。叶按 key 从小到大串成一条链。点查询仍然从根走到叶；范围查询找到下限所在的叶之后，沿叶链顺序扫到上限即可，不必再回到内部结点。关系数据库的磁盘索引多用 B+ 树，正是因为范围扫描是常见操作。

![图 11.9 3 阶 B+ 树](assets/8573da94ca956749.jpg)

B 树命中内部关键码时就可能拿到记录；B+ 树内部关键码只是叶键的复写，点查询仍要走到叶。看似多走一步，换来的是内部页只放短 key 和孩子指针，扇出更大、树更矮，而且所有记录都在同一叶层，范围访问路径统一。

一棵 3 阶 B+ 树可以这样看（内部结点只导航，数据全在最下一层，叶之间有横链）：

```text
                [  30  |  70  ]
               /       |       \
        [10|20]     [30|50]    [70|90]
           ↔           ↔           ↔
         记录…       记录…       记录…
```

本章的约定：3 阶指一个结点最多 3 个孩子，因此叶和内部结点都最多 2 个 key；内部结点的分界 key 取右子树最小 key 的复写。原书 11.4.2 用的是另一套约定——最大关键码复写，并把 m 阶 B+ 叶的容量定为 $\lceil m/2 \rceil$ 到 $m$ 个 key（3 阶叶因此存 2 到 3 个 key）。两套约定都成立，对照原书读时注意这处差别。

查找 50：根上 $30<50<70$，走中间孩子；叶上直接取到 50。查找 35 到 80：先落到含 35 的叶，再沿 `↔` 扫过 50、70，直到超过 80。

插入 60：中间叶暂成 `[30|50|60]`，超过 2 个 key 的上限。按原书 11.4.1 的分裂规则取中位数 50 作分界，叶裂成 `[30]` 和 `[50|60]`，50 复写一份上推（B+ 的分界 key 在叶上仍保留）。上推之后根成了 `[30|50|70]`、要挂 4 个孩子，同样超限，于是根也分裂：中位数 50 上移，成为新根，树增高一层。

| 阶段 | 发生溢出的页 | 局部结果 | 向父层传什么 |
| --- | --- | --- | --- |
| 插入前 | - | 叶 `[30,50]` | - |
| 插入 60 | 叶页 | `[30]` 与 `[50,60]` | 复写 50 |
| 父层接收 50 | 原根 | key 变为 `[30,50,70]`，4 个孩子 | 上移 50 |
| 建新根 | 根 | 新根 `[50]`，树高加 1 | 结束 |

![图 11.10 插入后 B+ 树增高一层](assets/bcafc0a9de581967.jpg)

```text
                    [  50  ]
                 /            \
          [  30  ]            [  70  ]
         /       \          /        \
    [10|20]     [30]      [50|60]    [70|90]
       ↔          ↔           ↔          ↔
```

叶分裂时分界 key 是**复写**（叶上仍留一份），内部结点分裂时分界 key 是**上移**（原结点不再保留）——这是 B+ 树与 B 树最容易混的一处。两条规则并排写出来就是：

```cpp file=code/ch11/bplus_tree/modern.hpp#bplus-split
void split_leaf(Node* node, Split& split) {
    const std::size_t mid = node->keys.size() / 2;
    auto right = std::make_unique<Node>(true);
    right->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid), node->keys.end());
    right->values.assign(std::make_move_iterator(node->values.begin() + static_cast<std::ptrdiff_t>(mid)),
                         std::make_move_iterator(node->values.end()));
    node->keys.resize(mid);
    node->values.resize(mid);
    right->next = node->next;
    node->next = right.get();
    // 叶分裂：分界码是右叶最小关键码，复写上推，叶上仍保留。
    split.happened = true;
    split.separator = right->keys.front();
    split.right = std::move(right);
    writes_ += 2;
}

void split_internal(Node* node, Split& split) {
    const std::size_t mid = node->keys.size() / 2;
    auto right = std::make_unique<Node>(false);
    // 内部结点分裂：中位数上移，原结点不再保留它。
    split.separator = node->keys[mid];
    right->keys.assign(node->keys.begin() + static_cast<std::ptrdiff_t>(mid) + 1, node->keys.end());
    right->children.assign(
        std::make_move_iterator(node->children.begin() + static_cast<std::ptrdiff_t>(mid) + 1),
        std::make_move_iterator(node->children.end()));
    node->keys.resize(mid);
    node->children.resize(mid + 1);
    split.happened = true;
    split.right = std::move(right);
    writes_ += 2;
}
```

先跑一遍。上面那两张图不是画出来的，是这段程序打印出来的：

```cpp file=code/ch11/bplus_tree/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    using dsa::index::BPlusTree;
    std::vector<std::pair<int, std::string>> rows;
    for (const int key : {10, 20, 30, 50, 70, 90}) {
        rows.emplace_back(key, "记录" + std::to_string(key));
    }
    // 11.2：批量装入，按页填满。
    BPlusTree tree = BPlusTree::bulk_load(3, rows, 2);
    std::printf("装入后   : %s\n", tree.to_string().c_str());

    // 11.4：插入 60，叶裂 → 根裂 → 树增高一层。
    tree.insert(60, "记录60");
    std::printf("插入 60  : %s（树高 %zu）\n", tree.to_string().c_str(), tree.height());
    tree.insert(65, "记录65");
    std::printf("插入 65  : %s（父结点没满，没裂到根）\n", tree.to_string().c_str());

    tree.reset_counters();
    // 先取结果再取计数：printf 的实参求值顺序没有保证。
    const auto found = tree.find(50);
    const std::size_t point_reads = tree.page_reads();
    std::printf("查找 50  : %s，读了 %zu 页\n", found->c_str(), point_reads);

    tree.reset_counters();
    const auto scan = tree.range(35, 80);
    const std::size_t scan_reads = tree.page_reads();
    std::printf("范围 35..80 :");
    for (const auto& row : scan) {
        std::printf(" %d", row.first);
    }
    std::printf("，读了 %zu 页\n", scan_reads);

    tree.erase(70);
    std::printf("删除 70  : %s\n", tree.to_string().c_str());
    return 0;
}
```

```text
装入后   : [30,70] / [10,20] [30,50] [70,90]
插入 60  : [50] / [30] [70] / [10,20] [30] [50,60] [70,90]（树高 3）
插入 65  : [50] / [30] [60,70] / [10,20] [30] [50] [60,65] [70,90]（父结点没满，没裂到根）
查找 50  : 记录50，读了 3 页
范围 35..80 : 50 60 65 70，读了 6 页
删除 70  : [50] / [30] [60,90] / [10,20] [30] [50] [60,65] [90]
```

范围扫描的写法直接对应「找到下限所在的叶，然后沿叶链横着走」：

```cpp file=code/ch11/bplus_tree/modern.hpp#bplus-range
/// 范围扫描：找到下限所在的叶，然后沿叶链横着走，不再回到内部结点。
[[nodiscard]] std::vector<std::pair<int, std::string>> range(int low, int high) const {
    std::vector<std::pair<int, std::string>> out;
    if (low > high) {
        return out;
    }
    const Node* node = root_.get();
    while (!node->leaf) {
        ++reads_;
        node = node->children[child_slot(*node, low)].get();
    }
    while (node != nullptr) {
        ++reads_;
        for (std::size_t i = 0; i < node->keys.size(); ++i) {
            if (node->keys[i] > high) {
                return out;
            }
            if (node->keys[i] >= low) {
                out.emplace_back(node->keys[i], node->values[i]);
            }
        }
        node = node->next;
    }
    return out;
}
```

删除 70：先在叶上删，若叶下溢就向兄弟借或合并，再改内部结点的路标。这里有一处容易漏：借位和合并都会把父结点的分界 key 搬进孩子里，而那个 key 可能正是刚被删掉的——搬完必须按「分界 key = 右子树最小 key」重算一遍，否则树里会留下一个指向已删关键码的路标。这个错误不影响点查询，只有结构不变量检查才看得出来（`code/ch11/bplus_tree/legacy.md` 记了实测过程）。

删除后的处理顺序固定：

1. 页仍达到最小占用，更新必要的父分界后结束；
2. 页下溢且相邻兄弟有富余，借一个键并重算父分界；
3. 兄弟也在下限，合并两页，父结点少一个 key 和一个孩子；
4. 父结点因此下溢，向上重复；若根只剩一个孩子，让该孩子成为新根，树高减 1。

![图 11.11 从 3 阶 B+ 树删除 75 后](assets/ffef69f4b64e2e43.jpg)

图 11.11　从图 11.9 的 3 阶 B+ 树里删掉 75 之后。内部结点上那份 75 的复写**可以留着不动**——它只是路标，不必对应一个真实存在的键。

实际系统里内部结点和叶结点的容量往往不同：内部结点只放短 key 和页号，放得下的多；叶要放记录或记录地址，放得少。内外阶数不同的叫**混合型 B+ 树**。

![图11.12 混合型 B+ 树，内部结点阶为 4，叶结点阶 5](assets/3de9f1e64abee567.jpg)

图 11.12　混合型 B+ 树：内部结点阶为 4，叶结点阶为 5。

![图 11.13 在图11.12 混合型 B+ 树中插入 22 后的结果](assets/c2cacd7eba558abe.jpg)

图 11.13　在图 11.12 的混合型 B+ 树里插入 22 之后。

![图11.14 在图11.13 混合型 B+ 树中删除 40 后的结果](assets/fe716221bc3bfed5.jpg)

图 11.14　在图 11.13 上删除 40 之后。分裂与合并的规则不变，只是内外两层各按各自的阶判断溢出和下溢。

分裂与合并要同时维护叶链。只修父子指针而漏掉 `next`，点查询仍可能全绿，范围查询却会跨不过断链；所以测试既要逐键 `find`，也要把全范围扫描结果与排序后的记录全集对拍。

B 树与 B+ 树可以记成三句话：记录在不在内部结点；叶有没有横向链接；范围扫描要不要反复爬树。

### 11.4.4 动态索引和静态索引性能的比较

静态索引在建立后不主动重排主文件；新增记录通常进入溢出区并挂链。它的结点紧凑、层数少，记录地址稳定，辅助索引也容易维护；但插删积累后溢出链变长、空洞增多，查询与空间利用率都会下降，最终必须停下来重组文件。

动态索引用 B 树或 B+ 树的分裂、借位和合并保持平衡。新旧记录具有相同的渐近查询代价，空间按需分配，也不需要周期性的全文件重组；代价是每个结点要保留孩子指针，树可能更高，更新还要处理并发锁与辅助索引中的地址变化。

| 工作负载 | 更合适的选择 | 原因 |
| --- | --- | --- |
| 数据几乎只读、可批量重建 | 静态索引 | 结构紧凑，读路径短 |
| 持续插入和删除 | B/B+ 树动态索引 | 局部调整即可保持查询性能 |
| 范围扫描很多 | B+ 树 | 数据集中在叶层，叶链可顺序读 |
| 记录物理地址必须长期稳定 | 静态索引 | 不因结点分裂搬动既有记录 |

选择的关键不是“哪一种总是更快”，而是更新成本由谁承担：静态索引把成本推迟到昂贵的批量重组，动态索引把成本摊到每次更新。读多写少且有维护窗口时静态结构仍有价值；在线系统无法停机重组时，动态索引通常更合适。

## 11.5 位索引技术

### 位图、签名和红黑树

位图索引适合取值很少的属性，例如性别、省份、是否及格。每个属性值对应一个位串，第 $i$ 位表示第 $i$ 条记录有没有这个值。AND、OR、NOT 查询变成机器字上的按位运算，一次可以处理几十上百条记录。属性取值一多，位图会变得很宽，需要压缩（游程、字对齐混合等）。

![图11.15 百货销售数据库记录的位图索引示意](assets/combined/fig-11-15.png)

图 11.15　百货销售数据库记录的位图索引：左边是记录与 `State=NY` 的位向量，右边是数据域 `Class` 的位向量集合。**一个属性值一条位串**，查询就变成位串之间的按位运算。

设 6 条记录的州属性和等级属性形成两条位图：

```text
State=NY : 1 0 1 0 1 0
Class=A  : 1 1 0 1 1 0
AND      : 1 0 0 0 1 0
```

结果位 0 和 4 为 1，所以查询返回第 1、5 条记录（若程序下标从 0 开始则是 0、4）。OR 返回满足任一条件的记录，NOT 必须把超出实际记录数的尾部位清零；否则一个 64 位机器字只用了前 6 位，取反后其余 58 位都会被误认成记录。

位图总空间约为“记录数 × 属性不同取值数”位。低基数列很合算：一百万条记录的“是否及格”只有两张约 122 KiB 的位图；若把几乎每人不同的学号做位图，就会生成近一百万张位图，完全失去意义。

签名文件把一篇文档的特征散列成较短的位串。查询时先用签名快速丢掉不可能匹配的文档，再回到原文确认。签名可能产生假阳性（签名说可能有、原文里没有），不能有假阴性。它适合「先粗筛、再精查」的全文检索，不替代倒排。

红黑树是内存里的近似平衡二叉搜索树：每个结点带一种颜色，通过几条局部规则保证从根到叶的黑结点数相同，最长路径不超过最短路径的两倍。查找、插入、删除都是 $O(\log n)$，旋转次数有常数上限。它适合进程地址空间里的有序映射，不替代按页组织的 B+ 树。本书不另写红黑树实现，只作对照讨论。

![图 11.16 红黑树示意图](assets/32c09de96383c4de.jpg)

图 11.16　红黑树示意图。

红黑树的“平衡”不是要求左右子树等高，而是用颜色约束控制最长路径：根为黑、红结点不能有红孩子、从任一结点到其后代空叶的黑结点数相同。由此可得原书的四条性质，其中最有用的一条是：含 $n$ 个内部结点的红黑树，树高最多 $2\log_2(n+1)+1$——**最长路径不超过最短路径的两倍**，检索因此是 $O(\log n)$。

本章的外存主线是 B+ 树，红黑树只作对照，所以下面按原书把调整规则列一遍、不另写实现。

**插入。** 先按普通二叉搜索树找到位置，把新结点着成**红色**。父结点是黑的就结束；父子都红就要调整，分两种情况看**叔父结点**的颜色。

情况 1：叔父是黑色。这时靠旋转解决。

![图11.17 父结点是红色、叔父结点是黑色的情况](assets/4cd9c419025ff11a.jpg)

图 11.17　父结点红、叔父黑：旋转一次解决红红冲突。图中新增结点 X 是父结点 A 的左孩子；X 是右孩子时先把 X 与 A 换位，就化归成同一种形状。

![图11.18 叔父结点为黑色时的四种重构](assets/a333f195118c2445.jpg)

图 11.18　叔父为黑时一共四种子结构（LL、LR、RL、RR）。四种的实质是同一件事：**取「双红结点 + 祖父」这三个键的中位数当新的子根、着黑，另外两个当它的孩子、着红**。每个结点的阶（黑高）都不变，调整到此结束。

情况 2：叔父也是红色。这时不必旋转，只要换色。

![图11.19 父结点、叔父结点均为红色](assets/74d9765c86b0459e.jpg)

图 11.19　父与叔父都红：把父 A 和叔父 C 都改成黑、祖父 B 改成红。A 以下的冲突解决了，但 B 可能与它的父结点再冲突，于是**把 B 当成新的 X 递归上去**。最坏一路推到根，把根改回黑色即可——这是红黑树唯一会长高（阶加 1）的时刻。

![图 11.20 红黑树插入示例](assets/1a24097d7b698644.jpg)

图 11.20　一次插入引发的红红冲突调整全过程。

**删除。** 先照 BST 删：待删结点若有两个非空孩子，就与右子树的最小结点交换值（颜色不动），转成「至多一个非空孩子」的情形再删。删完可能出现**双黑**结点，这是删除最复杂的地方，按兄弟和侄子的颜色分三种情况——下面都按「双黑是左孩子」写，右孩子左右对称。

情况 1(a)：兄弟黑、红侄子与双黑呈八字形外撇。

![图 11.21 情况 1(a)：双黑结点与红色侄子呈八字形对称](assets/dc4aa499811e6130.jpg)

图 11.21　把兄弟 C 提上去继承原父结点 B 的颜色，B 与侄子 D 都着黑。

情况 1(b)：兄弟黑、红侄子与双黑同边顺。

![图11.22 情况1(b)：双黑结点与红色侄子同边顺](assets/d8199ab1b4f48019.jpg)

图 11.22　把侄子 D 提上去当子根、继承原子根 B 的颜色，B 着黑。也可以看成双旋转：先转一次化归成情况 1(a)，再转一次。

情况 2：兄弟黑，且兄弟的两个孩子都是黑。

![图11.23 情况2：双黑的兄弟为黑色且有两个黑孩子](assets/0314fc9126687660.jpg)

图 11.23　只换色：兄弟 C 着红、父 B 着黑。B 原来是红的就此结束；原来是黑的，就把 B 当成新的双黑继续往上调整。

情况 3：兄弟是红色。

![图 11.24 情况3：双黑的兄弟结点为红色](assets/ea0f0114893b0d4b.jpg)

图 11.24　此时父结点必为黑、兄弟的两个孩子必为黑。旋转一次之后 X 仍是双黑，但已经**化归成前两种情况**，继续处理即可。

![图11.25 红黑树连续删除 90、70、80 引起的双黑调整示例](assets/96123ad82e555db6.jpg)

图 11.25　连续删除 90、70、80 引起的双黑调整全过程。

图 11.16 到 11.25 只用来比较「二叉、内存结点」与「多路、页结点」两种成本模型：红黑树每步只碰几个结点、适合内存里的有序映射；B+ 树每步碰一整页、适合按页读写的外存索引。

先跑一遍：

```cpp file=code/ch11/bitmap_index/demo.cpp
#include "modern.hpp"

#include <cstdio>

int main() {
    dsa::index::BitmapIndex index;
    for (int i = 0; i < 200; ++i) {
        index.add_record((i % 3) == 0 ? "及格" : "不及格");
    }
    index.reset_ops();
    const auto passed = index.select("及格");
    const auto failed = index.select_not("及格");
    std::printf("200 条记录，%zu 个取值，位图共 %zu 个机器字\n",
                index.distinct_values(), index.words());
    std::printf("及格 %zu 条，不及格 %zu 条；取反只做了 %zu 次字运算\n",
                passed.size(), failed.size(), index.word_ops());

    // 稀疏位图：大片全 0 的字，游程压缩很有效。
    dsa::index::BitmapIndex sparse;
    for (int i = 0; i < 1000; ++i) {
        sparse.add_record(i < 3 ? "命中" : "其他");
    }
    const auto bits = sparse.bitmap("命中");
    const auto encoded = dsa::index::run_length_encode(bits);
    std::printf("稀疏位图 %zu 个字 → 游程压缩后 %zu 个字\n", bits.size(), encoded.size());

    dsa::index::SignatureFile signatures(2);
    signatures.add(1, {"数据", "结构"});
    signatures.add(2, {"算法", "分析"});
    std::printf("签名粗筛「数据」的候选文档数：%zu（仍需回原文确认）\n",
                signatures.candidates({"数据"}).size());
    return 0;
}
```

```text
200 条记录，2 个取值，位图共 8 个机器字
及格 67 条，不及格 133 条；取反只做了 4 次字运算
稀疏位图 16 个字 → 游程压缩后 4 个字
签名粗筛「数据」的候选文档数：1（仍需回原文确认）
```

第二行就是位图的全部理由：200 条记录只占 4 个机器字，一次取反做 4 次运算就处理完了全部记录。查询写出来只是逐字的按位运算：

```cpp file=code/ch11/bitmap_index/modern.hpp#bitmap-ops
/// 「与」：逐字 `&`。`word_ops()` 数的就是这里做了几次字运算。
[[nodiscard]] std::vector<std::size_t> select_and(const std::string& a,
                                                  const std::string& b) const {
    return to_records(combine(bitmap(a), bitmap(b), Op::And));
}

[[nodiscard]] std::vector<std::size_t> select_or(const std::string& a,
                                                 const std::string& b) const {
    return to_records(combine(bitmap(a), bitmap(b), Op::Or));
}

[[nodiscard]] std::vector<std::size_t> select_not(const std::string& value) const {
    std::vector<std::uint64_t> bits = bitmap(value);
    for (auto& word : bits) {
        word = ~word;
        ++ops_;
    }
    mask_tail(bits);  // 最后一个字里超出记录数的那些位必须清掉
    return to_records(bits);
}
```

注意 `mask_tail`：记录数不是 64 的整数倍时，最后一个字里超出记录数的那些位取反后会变成 1，于是冒出根本不存在的记录号。这是位图实现最常见的一个洞。

压缩不是万能的。上面稀疏位图 16 个字压到 4 个，但对 0 和 1 交替的稠密位图，游程压缩后**反而更大**——`code/ch11/bitmap_index/test.cpp` 直接断言了这一点，不粉饰。

签名文件也需要一个反例才能看清边界。若“数据”和“结构”经散列后碰巧设置了与“算法”相同的几位，查询“算法”时这篇文档会进入候选集，即假阳性；但只要构造签名时把每个真实词的位都置上，真正含“算法”的文档不可能被漏掉。粗筛之后必须回原文确认，不能把候选集直接当查询答案。

| 结构 | 保存什么 | 可能的额外工作 | 适合场景 |
| --- | --- | --- | --- |
| 倒排表 | 词到文档号/位置的列表 | 合并有序列表 | 精确全文检索、短语查询 |
| 位图 | 属性值到记录位向量 | 按位布尔运算 | 低基数列组合过滤 |
| 签名 | 文档特征的短位串 | 假阳性后回原文确认 | 空间受限的粗筛 |
| B+ 树 | 有序 key 到记录位置 | 沿页树下降、沿叶链扫描 | 点查询与范围查询 |

## 练习路径

本章四个方向都已有可运行实现，练习因此从「照着写一遍」改成「在已有实现上再往前走一步」：

1. 给 `MultiLevelIndex` 的页内定位换成二分查找，测量比较次数的变化，并确认**页访问次数一次都没变**——本章的关键指标不在这里。
2. 给 `InvertedIndex` 的倒排表加差值编码（只存与前一个文档号的差），比较压缩前后的表长；注意求交时要能边解码边归并。
3. 给 `BPlusTree` 加一个「按范围批量删除」的接口，并用 `validate()` 确认每一步之后结构仍然合法。
4. 给 `BitmapIndex` 换成字对齐混合编码（WAH），和现在的字级游程比压缩率；用交替位图确认它不会像现在这样越压越大。
5. 为 `SignatureFile` 做一次参数实验：固定语料，改变每词位数，画出假阳性率随位数的变化。


## 本章小结

### Python 实现的证据链

```python file=code/ch11/linear_index/modern.py#index-find
def find(self,key):
    if not self._levels:
        return None
    page = self._locate(self._levels[-1],0,len(self._levels[-1]),key)
    for level in range(len(self._levels)-1,0,-1):
        page = self._levels[level][page][1]
        self._reads += 1
        first = page*self.epp
        last = min(first+self.epp,len(self._levels[level-1]))
        page = self._locate(self._levels[level-1],first,last,key)
    entry = self._levels[0][page]
    if self.kind==DENSE:
        if entry[0]!=key:
            return None
        self._reads += 1
        return self.records[entry[1]][1]
    self._reads += 1
    for record_key,value in self.records[entry[1]*self.rpp:(entry[1]+1)*self.rpp]:
        if record_key==key:
            return value
    return None
```

```python file=code/ch11/inverted_index/modern.py#inverted-intersect
def intersect(left, right):
    out = []
    i=j = 0
    while i<len(left) and j<len(right):
        if left[i]<right[j]:
            i += 1
        elif right[j]<left[i]:
            j += 1
        else:
            out.append(left[i])
            i += 1
            j += 1
    return out

def unite(left, right):
    out = []
    i=j = 0
    while i<len(left) or j<len(right):
        if j==len(right) or (i<len(left) and left[i]<right[j]):
            value = left[i]
            i += 1
        elif i==len(left) or right[j]<left[i]:
            value = right[j]
            j += 1
        else:
            value = left[i]
            i += 1
            j += 1
        if not out or out[-1]!=value:
            out.append(value)
    return out

def difference(left, right):
    out = []
    j = 0
    for value in left:
        while j<len(right) and right[j]<value:
            j += 1
        if j==len(right) or right[j]!=value:
            out.append(value)
    return out
```

```python file=code/ch11/bitmap_index/modern.py#bitmap-ops
def select_and(self,a,b):
    return self._combine(a,b,lambda x,y:x&y)
def select_or(self,a,b):
    return self._combine(a,b,lambda x,y:x|y)
def select_not(self,value):
    bits = [(~word)&MASK for word in self.bitmap(value)]
    self._ops += len(bits)
    if bits and self._count%64:
        bits[-1] &= (1<<(self._count%64))-1
    return self._records(bits)
```

**B+ 树这一节没有 Python 版，理由值得单独讲一句。** 本书讲算法的章节两种语言各给一份
（D-025），但 B+ 树讲的**不是算法，是页式存储管理**：页容量、装填因子、结点分裂、
删除时的借位与合并，以及 `page_reads()` / `page_writes()` 这两个只有在真的按页访问时
才有意义的计数器。这些东西在一个 `list` 上无处安放——和 3.1 节的顺序栈、4.2 节的
字符串类是同一条线。

这不是事后的托词，是**试出来的**。本书曾提交过一版 Python「B+ 树」：内部是一个有序
`list`，`height()` 与 `leaf_count()` 由记录数套公式算出，`page_reads()` 累加的是那个
公式的结果，而标着「结点分裂」的那段代码里没有任何分裂。它跑得通，闸门也是绿的——
**因为一个不存在的结构，没有任何断言能证伪它**。理由与那次的结论一起记在
`code/ch11/bplus_tree/unit.json` 的 `py_skip` 字段里，闸门核对该单元确实没有
`modern.py`。想看 B+ 树按页分裂到底怎么写，读上面那段 C++。

索引是「关键码到记录位置」的表，用来少读磁盘。线性索引分稠密和稀疏，过大就做成多级。静态多分树把一个结点做成一页，树高低、页访问少，但不适合频繁更新。倒排从属性走到记录集合，查询变成交并差。B 树在页内分裂合并以保持平衡；B+ 树把记录放在叶上并用叶链做范围扫描。位图适合低基数属性，签名做粗筛，红黑树是内存有序映射。

## 习题

### 补充索引题（参考课程第 11 章）

1. 给定页大小、记录大小和索引项大小，计算稠密索引最多支持的记录数及二级索引的访外次数。
2. 对给定的院系表和宿舍表建立倒排索引，并求“院系且宿舍”查询的交集。
3. 模拟 B+ 树查找、插入分裂和删除后继替换，分别统计读页和写页次数。

1. 稠密索引和稀疏索引各要求主文件怎样组织。
2. 多级索引一次查询大约读几页？关键指标为什么不是比较次数。
3. 静态多分树插入一条溢出记录会发生什么。
4. 用倒排表求「计算机系且擅长英语」的交集。
5. 在 11.4 插入 60 之后的那棵 3 阶 B+ 树上继续插入 65，写出叶和根，并说明这次为什么没有一直裂到根。
6. 位图、签名、红黑树、B+ 树各适合内存还是外存、点查询还是范围查询。

## 上机题

见本章末「练习路径」的四条。
