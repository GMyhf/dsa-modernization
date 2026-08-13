# 第9章 文件管理与外部排序

当数据大到内存装不下时，排序的瓶颈变成磁盘读写。外部排序先生成有序顺串，再进行多路归并。置换选择用最小堆尽可能延长当前顺串；竞赛树维护多路输入中当前最小的选手。

源码：[置换选择与竞赛树](../code/ch09/external_sort/modern.hpp)、
[可运行示例](../code/ch09/external_sort/demo.cpp)、
[测试](../code/ch09/external_sort/test.cpp)。

## 9.1 主存储器和外存储器

内存按字节随机访问，外存按页块读写，一次定位往往比一次比较贵几个数量级。所以外排序的目标首先是减少读写次数，而不是减少内存里的比较。

## 9.2 文件的组织和管理

文件是外存上的记录集合。本章不实现页缓存和文件句柄，只抽出「如何生成更长的初始顺串」和「如何在 k 路中选最小」。

## 9.3 外排序

### 9.3.1 置换选择排序

顺串是磁盘上已经有序的一段记录。内存一次只能装 M 条时，朴素做法是每批排成一条长为 M 的顺串。置换选择可以做得更好：输出堆顶之后，若下一条记录**不小于**刚输出的值，它还可以进入当前顺串；否则冻结到下一趟。平均情况下第一趟长度约为 2M。

原书图 9.2 的输入是 `50 49 35 45 30 25 15 60 16 27 1`，工作区 `M = 7`。前 7 个建成最小堆后，堆顶是 15。接着读到 60——它比 15 大，可以进当前堆；再读到 16、27、1，它们都比当时的输出值小，被冻结。第一顺串因此是

```text
15 25 30 35 45 49 50 60
```

长度 8，已经超过 M。剩下的 `1 16 27` 构成第二顺串。

![图9.1 置换选择算法流程](assets/f23f55c21cffd0b7.jpg)

图9.1 置换选择算法流程

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

