# 第9章 文件管理与外部排序

当数据大到内存装不下时，排序的瓶颈变成磁盘读写。外部排序先生成有序顺串，再进行多路归并。置换选择用最小堆尽可能延长当前顺串；竞赛树维护多路输入中当前最小的选手。

源码：[置换选择与竞赛树](../code/ch09/external_sort/modern.hpp)、
[可运行示例](../code/ch09/external_sort/demo.cpp)、
[测试](../code/ch09/external_sort/test.cpp)。

## 9.1 主存储器和外存储器

前面各章的结构基本上都在内存里，也叫内部数据结构。内存容量有限，程序一结束数据也就没了。大规模数据必须放到外存上，排序过程也就变成多次内存与外存之间的交换。

主存（RAM、cache、显存）在主板上，按单元连续编址，CPU 直接访问，一次存取时间可以看成很小的常数，单位是纳秒。它快、贵、容量相对小，断电就丢。外存（硬盘、磁带、U 盘）便宜、容量大，信息断电不丢，有的还能随身带走。但一次存取以毫秒甚至秒计，比内存慢几个数量级。

外存慢，一个原因是每次访问都要先定位再读写。磁盘要把磁头移到目标磁道，再等扇区转到磁头下，定位往往就要几毫秒到几十毫秒，远慢于真正把数据读出来。所以外存按固定大小的页块存取：一次定位读写一整页，减少定位次数。顺序扫描时再配合缓冲：一次读入一页或几页到内存，后面的访问尽量打在缓冲区里。

对外排序来说，目标首先是减少读写次数，而不是减少内存里的比较。一次磁盘 I/O 往往比一次比较贵几个数量级。

## 9.2 文件的组织和管理

文件是外存上的数据结构，由大量性质相同的记录组成。记录是有独立逻辑意义的一块数据，简单可以是一串字符，复杂则由若干字段组成。操作系统文件常常是连续字符流，结构不明显；数据库文件是有结构的记录集合，每条记录由若干不可再分的数据项组成。学生登记表——姓名、学号、性别、出生年月——就是后一种。

按记录长度，文件分定长和不定长：定长更好处理。按关键码个数，分单关键码和多关键码：多关键码文件除了主码还可以有若干次码。操作通常以记录为单位：顺序读、追加、按条件修改或删除。处理方式有实时（要求很快应答）和批量（允许较长反馈）。

用户看见的是逻辑文件：顺序定长、顺序变长、或按关键码存取。系统实现的是物理文件，常见几种：

1. **顺序文件**：记录按逻辑次序放进连续物理块，物理顺序与逻辑顺序一致。顺序扫描很快，按关键码插入、删除要搬很多块。
2. **索引文件**：主文件之外另造索引，先查索引再读记录。第 11 章专门讨论。
3. **散列文件**：用散列函数把关键码映射到桶或块。第 10 章的闭散列思想可以搬到外存，但冲突处理要按块设计。

本章不实现页缓存和文件句柄，只抽出外排序里两件与文件组织无关、却决定 I/O 次数的事：如何生成更长的初始顺串，以及如何在 $k$ 路归并里选出当前最小。

## 9.3 外排序

### 9.3.1 置换选择排序

顺串是磁盘上已经有序的一段记录。内存一次只能装 M 条时，朴素做法是每批排成一条长为 M 的顺串。置换选择可以做得更好：输出堆顶之后，若下一条记录**不小于**刚输出的值，它还可以进入当前顺串；否则冻结到下一趟。平均情况下第一趟长度约为 2M。

原书图 9.2 的输入是 `50 49 35 45 30 25 15 60 16 27 1`，工作区 `M = 7`。前 7 个建成最小堆后，堆顶是 15。接着读到 60——它比 15 大，可以进当前堆；再读到 16、27、1，它们都比当时的输出值小，被冻结。第一顺串因此是

```text
15 25 30 35 45 49 50 60
```

长度 8，已经超过 M。剩下的 `1 16 27` 构成第二顺串。

图9.1 置换选择：工作区是最小堆。弹出堆顶后，下一条记录若不小于刚输出的值就入堆，否则冻结到下一趟。

若把整份输入推进一个堆再依次弹出，得到的是一条完全有序序列——那是堆排序，不是置换选择。旧实现曾经这样做，测试只断言 `{3,1,2} → {1,2,3}`，堆排序也能过。现在的接口返回**若干顺串**，并用原书这组数据守门。

多路归并时，每次要在 k 路的队首里选出最小者。赢者树的内部结点保存胜者下标；败者树的内部结点保存败者，另用一个冠军槽记录全局最小。替换一名选手后，两者都只需沿叶到根重赛。

先跑一遍：

```cpp file=code/ch09/external_sort/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    const std::vector<int> input{50, 49, 35, 45, 30, 25, 15, 60, 16, 27, 1};
    const auto runs = dsa::external_sort::replacement_selection(input, 7);

    std::cout << "工作区 M=7，得到 " << runs.size() << " 个顺串\n";
    for (std::size_t index = 0; index < runs.size(); ++index) {
        std::cout << "顺串 " << index + 1 << "（长度 " << runs[index].size() << "）:";
        for (int value : runs[index]) {
            std::cout << ' ' << value;
        }
        std::cout << '\n';
    }

    dsa::external_sort::LoserTree tree({20, 6, 8, 9, 11});
    std::cout << "败者树当前冠军: " << *tree.winner() << '\n';
    tree.replace(1, 15);
    std::cout << "替换后冠军: " << *tree.winner() << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch09/external_sort \
    code/ch09/external_sort/demo.cpp -o /tmp/extsort-demo
/tmp/extsort-demo
```

```console
工作区 M=7，得到 2 个顺串
顺串 1（长度 8）: 15 25 30 35 45 49 50 60
顺串 2（长度 3）: 1 16 27
败者树当前冠军: 6
替换后冠军: 8
```

把 `M` 改成 11（装得下全部输入），会只得到一个顺串，内容是整表有序——这时置换选择退化为堆排序，正是「内存够用」的边界。

`replacement_selection(input, memory)` 先读入至多 `memory` 条建成最小堆。循环中弹出堆顶写入当前顺串；若还有输入，按「`incoming >= emitted` 则入堆，否则冻结」分流。当前堆空了，就把冻结区建成新堆，开始下一趟。

```cpp file=code/ch09/external_sort/modern.hpp#replacement-selection
// 算法9.1：置换选择。memory 是内存工作区能容纳的记录数 M。
// 返回若干顺串：每个顺串内部有序，第一趟的平均长度约为 2M，而不是 M。
// 不属于当前顺串的新记录被冻结，等当前堆耗尽后再建下一趟。
inline std::vector<std::vector<int>> replacement_selection(const std::vector<int>& input,
                                                           std::size_t memory) {
    if (memory == 0) {
        throw std::invalid_argument("replacement selection memory must be positive");
    }
    if (input.empty()) {
        return {};
    }

    std::size_t next = 0;
    std::vector<int> heap;
    heap.reserve(memory);
    while (next < input.size() && heap.size() < memory) {
        heap.push_back(input[next++]);
    }
    detail::heap_from(heap);

    std::vector<std::vector<int>> runs;
    std::vector<int> current_run;
    std::vector<int> frozen;

    while (!heap.empty() || next < input.size() || !frozen.empty()) {
        if (heap.empty()) {
            if (!current_run.empty()) {
                runs.push_back(std::move(current_run));
                current_run = {};
            }
            heap = std::move(frozen);
            frozen = {};
            while (next < input.size() && heap.size() < memory) {
                heap.push_back(input[next++]);
            }
            detail::heap_from(heap);
            if (heap.empty()) {
                break;
            }
        }

        const int emitted = detail::heap_pop(heap);
        current_run.push_back(emitted);

        if (next == input.size()) {
            continue;
        }
        const int incoming = input[next++];
        if (incoming >= emitted) {
            detail::heap_push(heap, incoming);
        } else {
            frozen.push_back(incoming);
        }
    }
    if (!current_run.empty()) {
        runs.push_back(std::move(current_run));
    }
    return runs;
}
```

### 9.3.2 二路外排序

对 m 个顺串两两归并，趟数是 $\lceil\log_2 m\rceil$。置换选择把初始顺串变长，就是为了减小 m。

### 9.3.3 多路归并——选择树

赢者树用完全二叉数组：叶存放选手下标，内部结点写 `better(左, 右)`。败者树在内部结点写败者，另用 `champion_` 记全局胜者。替换后只沿叶到根重赛。

```cpp file=code/ch09/external_sort/modern.hpp#winner-tree
// 代码9.2：赢者树。内部结点保存两名选手比较后的胜者下标，根是全局最小。
class WinnerTree {
public:
    explicit WinnerTree(std::vector<int> players) : players_(std::move(players)) {
        if (players_.empty()) {
            return;
        }
        leaf_base_ = detail::TournamentOps::next_power_of_two(players_.size());
        tree_.assign(leaf_base_ * 2, detail::TournamentOps::no_player);
        for (std::size_t index = 0; index < players_.size(); ++index) {
            tree_[leaf_base_ + index] = index;
        }
        for (std::size_t node = leaf_base_ - 1; node > 0; --node) {
            tree_[node] = detail::TournamentOps::better(players_, tree_[node * 2],
                                                        tree_[node * 2 + 1]);
        }
    }

    [[nodiscard]] std::optional<std::size_t> winner_index() const {
        if (players_.empty() || tree_[1] == detail::TournamentOps::no_player) {
            return std::nullopt;
        }
        return tree_[1];
    }

    [[nodiscard]] std::optional<int> winner() const {
        const auto index = winner_index();
        return index ? std::optional<int>(players_[*index]) : std::nullopt;
    }

    void replace(std::size_t player, int value) {
        if (player >= players_.size()) {
            throw std::out_of_range("tournament player");
        }
        players_[player] = value;
        std::size_t node = leaf_base_ + player;
        tree_[node] = player;
        while (node > 1) {
            node /= 2;
            tree_[node] = detail::TournamentOps::better(players_, tree_[node * 2],
                                                        tree_[node * 2 + 1]);
        }
    }

private:
    std::vector<int> players_;
    std::vector<std::size_t> tree_;
    std::size_t leaf_base_{0};
};
```

```cpp file=code/ch09/external_sort/modern.hpp#loser-tree
// 代码9.3：败者树。内部结点保存败者下标，另用 champion_ 记录全局胜者。
// 替换一名选手时只需沿叶到根重赛，不必访问兄弟子树的内部结构。
class LoserTree {
public:
    explicit LoserTree(std::vector<int> players) : players_(std::move(players)) {
        if (players_.empty()) {
            return;
        }
        leaf_base_ = detail::TournamentOps::next_power_of_two(players_.size());
        loser_.assign(leaf_base_, detail::TournamentOps::no_player);
        subtree_winner_.assign(leaf_base_ * 2, detail::TournamentOps::no_player);
        for (std::size_t index = 0; index < players_.size(); ++index) {
            subtree_winner_[leaf_base_ + index] = index;
        }
        for (std::size_t node = leaf_base_ - 1; node > 0; --node) {
            replay_node(node);
        }
        champion_ = subtree_winner_[1];
    }

    [[nodiscard]] std::optional<std::size_t> winner_index() const {
        if (players_.empty() || champion_ == detail::TournamentOps::no_player) {
            return std::nullopt;
        }
        return champion_;
    }

    [[nodiscard]] std::optional<int> winner() const {
        const auto index = winner_index();
        return index ? std::optional<int>(players_[*index]) : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> loser_at(std::size_t node) const {
        if (node == 0 || node >= loser_.size() ||
            loser_[node] == detail::TournamentOps::no_player) {
            return std::nullopt;
        }
        return loser_[node];
    }

    void replace(std::size_t player, int value) {
        if (player >= players_.size()) {
            throw std::out_of_range("tournament player");
        }
        players_[player] = value;
        subtree_winner_[leaf_base_ + player] = player;
        for (std::size_t node = (leaf_base_ + player) / 2; node > 0; node /= 2) {
            replay_node(node);
        }
        champion_ = subtree_winner_[1];
    }

private:
    void replay_node(std::size_t node) {
        const std::size_t left = subtree_winner_[node * 2];
        const std::size_t right = subtree_winner_[node * 2 + 1];
        loser_[node] = detail::TournamentOps::worse(players_, left, right);
        subtree_winner_[node] = detail::TournamentOps::better(players_, left, right);
    }

    std::vector<int> players_;
    std::vector<std::size_t> loser_;
    std::vector<std::size_t> subtree_winner_;
    std::size_t leaf_base_{0};
    std::size_t champion_{detail::TournamentOps::no_player};
};
```

## 本章小结

外存按页存取，一次定位比一次比较贵几个数量级，所以外排序首先要减少读写次数。文件是外存上的记录集合，逻辑组织与物理组织（顺序、索引、散列）要分开看。外排序先生成初始顺串再多路归并。置换选择用堆把不属于当前顺串的记录冻结，第一趟平均长度约 $2M$，而不是 $M$。$k$ 路归并用赢者树或败者树在 $O(\log k)$ 时间内选出当前最小。

## 习题

### 补充外排序题（参考课程第 9 章）

1. 给定内存容量 `M` 的最小堆，模拟置换选择生成全部顺串，并标出每次冻结的记录。
2. 给定页大小、记录大小和内存缓冲页数，计算一次归并的最大路数、顺串长度和访外次数。
3. 比较 winner tree、loser tree 与最小堆在多路归并中的更新代价。

1. 说明为什么外存要按页读写，以及缓冲如何减少定位次数。
2. 对输入 `50 49 35 45 30 25 15 60 16 27 1`、$M=7$，写出置换选择的两个顺串，并指出哪些键被冻结。
3. 若 $M$ 大到能装下全部输入，置换选择退化成什么。
4. 赢者树和败者树的内部结点各记什么？替换一名选手后为什么只需沿叶到根重赛。
5. $m$ 个顺串做二路归并要多少趟？置换选择怎样减少 $m$。

## 上机题

1. 实现置换选择，用原书图 9.2 的输入做守门测试。
2. 实现赢者树，随机替换选手并与每次扫描 $k$ 路的朴素选最小对拍。
3. 模拟 $k$ 路归并：输入是若干已排序向量，用败者树输出完整有序序列。
4. 比较 $M=4,8,16$ 时置换选择产生的顺串个数。
