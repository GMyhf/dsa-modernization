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

批量装入本身就是「按页填满、自底向上建层」，`code/ch11/bplus_tree` 的 `bulk_load` 实现的正是这件事——把下一节那棵 3 阶 B+ 树按这个办法装出来，得到的就是 11.4 图里的形状。区别只在后续：静态多分树靠重组，B+ 树靠分裂与合并就地维护。

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

插入使结点超过上限（溢出）时，把它分裂成两个，并把分界 key 上推到父结点；父结点也可能接着分裂，最坏会一直裂到根，树增高一层。删除使结点低于下限（下溢）时，先向左右兄弟借 key；借不到就把两个结点合并，分界 key 从父结点拿下来。借和合并都可能向上传播。这些局部调整保证所有叶在同一层，所以 B 树始终平衡。

B+ 树把记录（或记录位置）全部放在叶上，内部结点只作路标，不存完整记录。叶按 key 从小到大串成一条链。点查询仍然从根走到叶；范围查询找到下限所在的叶之后，沿叶链顺序扫到上限即可，不必再回到内部结点。关系数据库的磁盘索引多用 B+ 树，正是因为范围扫描是常见操作。

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

B 树与 B+ 树可以记成三句话：记录在不在内部结点；叶有没有横向链接；范围扫描要不要反复爬树。

## 11.5 位索引技术

### 位图、签名和红黑树

位图索引适合取值很少的属性，例如性别、省份、是否及格。每个属性值对应一个位串，第 $i$ 位表示第 $i$ 条记录有没有这个值。AND、OR、NOT 查询变成机器字上的按位运算，一次可以处理几十上百条记录。属性取值一多，位图会变得很宽，需要压缩（游程、字对齐混合等）。

签名文件把一篇文档的特征散列成较短的位串。查询时先用签名快速丢掉不可能匹配的文档，再回到原文确认。签名可能产生假阳性（签名说可能有、原文里没有），不能有假阴性。它适合「先粗筛、再精查」的全文检索，不替代倒排。

红黑树是内存里的近似平衡二叉搜索树：每个结点带一种颜色，通过几条局部规则保证从根到叶的黑结点数相同，最长路径不超过最短路径的两倍。查找、插入、删除都是 $O(\log n)$，旋转次数有常数上限。它适合进程地址空间里的有序映射，不替代按页组织的 B+ 树。本书不另写红黑树实现，只作对照讨论。

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

## 练习路径

本章四个方向都已有可运行实现，练习因此从「照着写一遍」改成「在已有实现上再往前走一步」：

1. 给 `MultiLevelIndex` 的页内定位换成二分查找，测量比较次数的变化，并确认**页访问次数一次都没变**——本章的关键指标不在这里。
2. 给 `InvertedIndex` 的倒排表加差值编码（只存与前一个文档号的差），比较压缩前后的表长；注意求交时要能边解码边归并。
3. 给 `BPlusTree` 加一个「按范围批量删除」的接口，并用 `validate()` 确认每一步之后结构仍然合法。
4. 给 `BitmapIndex` 换成字对齐混合编码（WAH），和现在的字级游程比压缩率；用交替位图确认它不会像现在这样越压越大。
5. 为 `SignatureFile` 做一次参数实验：固定语料，改变每词位数，画出假阳性率随位数的变化。

## 本章小结

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

见 11.6 的四条练习路径。
