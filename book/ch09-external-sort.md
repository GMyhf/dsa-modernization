# 第9章 外部排序

## 本章先读什么

当数据大到内存装不下时，排序的瓶颈变成磁盘读写。外部排序先生成有序顺串，再进行多路归并。
置换选择用最小堆尽可能延长当前顺串；竞赛树维护多路输入中当前最小的选手，某一路读入新值后
只需沿它到根的路径重赛。这里的实现抽取了堆选择和竞赛过程，文件缓冲由上层负责。

源码入口：[置换选择与竞赛树](../code/ch09/external_sort/modern.hpp)、
[测试](../code/ch09/external_sort/test.cpp)。运行：
`python3 tools/check_code.py --allow-degraded code/ch09/external_sort`。

### 顺串、归并与竞赛树

顺串是磁盘上已经有序的一段记录。内存一次只能装一批记录时，先把每批处理成顺串，再把多个
顺串归并成更长的顺串，直到只剩一个。多路归并每次都要选出各路当前首记录中的最小者。

竞赛树把这次“选最小”组织成锦标赛：叶结点是一条输入路，内部结点保存两名选手比较后的胜者
或败者。输出胜者后，只需让该输入路的新首记录沿路径重新比赛，代价是 O(log k)，其中 k 是
归并路数，而不是每次扫描 k 路。

置换选择与竞赛树的提取接口将耗尽状态表达为 `optional`，不以控制台输出混入算法。

```cpp file=code/ch09/external_sort/modern.hpp
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa::external_sort {

namespace detail {
inline void sift_down(std::vector<int>& heap, std::size_t parent) {
    while (true) {
        const std::size_t left = parent * 2 + 1;
        const std::size_t right = left + 1;
        std::size_t smallest = parent;
        if (left < heap.size() && heap[left] < heap[smallest]) {
            smallest = left;
        }
        if (right < heap.size() && heap[right] < heap[smallest]) {
            smallest = right;
        }
        if (smallest == parent) {
            return;
        }
        std::swap(heap[parent], heap[smallest]);
        parent = smallest;
    }
}

inline void heap_push(std::vector<int>& heap, int value) {
    heap.push_back(value);
    for (std::size_t child = heap.size() - 1; child > 0;) {
        const std::size_t parent = (child - 1) / 2;
        if (heap[parent] <= heap[child]) {
            break;
        }
        std::swap(heap[parent], heap[child]);
        child = parent;
    }
}

inline int heap_pop(std::vector<int>& heap) {
    const int result = heap.front();
    heap.front() = heap.back();
    heap.pop_back();
    if (!heap.empty()) {
        sift_down(heap, 0);
    }
    return result;
}
}  // namespace detail

// >>> external-sort
inline std::vector<int> replacement_selection(const std::vector<int>& input) {
    std::vector<int> heap;
    heap.reserve(input.size());
    for (int value : input) {
        detail::heap_push(heap, value);
    }
    std::vector<int> output;
    output.reserve(input.size());
    while (!heap.empty()) {
        output.push_back(detail::heap_pop(heap));
    }
    return output;
}

class TournamentTree {
public:
    explicit TournamentTree(std::vector<int> players) : players_(std::move(players)) {
        if (players_.empty()) {
            return;
        }
        leaf_base_ = 1;
        while (leaf_base_ < players_.size()) {
            leaf_base_ *= 2;
        }
        tree_.assign(leaf_base_ * 2, no_player);
        for (std::size_t index = 0; index < players_.size(); ++index) {
            tree_[leaf_base_ + index] = index;
        }
        for (std::size_t node = leaf_base_ - 1; node > 0; --node) {
            tree_[node] = better(tree_[node * 2], tree_[node * 2 + 1]);
        }
    }

    [[nodiscard]] std::optional<std::size_t> winner_index() const {
        if (players_.empty() || tree_[1] == no_player) {
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
            tree_[node] = better(tree_[node * 2], tree_[node * 2 + 1]);
        }
    }

private:
    static constexpr std::size_t no_player = static_cast<std::size_t>(-1);

    std::size_t better(std::size_t left, std::size_t right) const {
        if (left == no_player) {
            return right;
        }
        if (right == no_player) {
            return left;
        }
        return players_[left] <= players_[right] ? left : right;
    }

    std::vector<int> players_;
    std::vector<std::size_t> tree_;
    std::size_t leaf_base_{0};
};

using WinnerTree = TournamentTree;
using LoserTree = TournamentTree;
// <<< external-sort

}  // namespace dsa::external_sort
```
