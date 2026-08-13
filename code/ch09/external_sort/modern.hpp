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

inline void heap_from(std::vector<int>& values) {
    if (values.size() < 2) {
        return;
    }
    for (std::size_t parent = values.size() / 2; parent != 0;) {
        --parent;
        sift_down(values, parent);
        if (parent == 0) {
            break;
        }
    }
}
}  // namespace detail

// >>> replacement-selection
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
// <<< replacement-selection

namespace detail {
struct TournamentOps {
    static constexpr std::size_t no_player = static_cast<std::size_t>(-1);

    static std::size_t next_power_of_two(std::size_t count) {
        std::size_t base = 1;
        while (base < count) {
            base *= 2;
        }
        return base;
    }

    static std::size_t better(const std::vector<int>& players, std::size_t left,
                              std::size_t right) {
        if (left == no_player) {
            return right;
        }
        if (right == no_player) {
            return left;
        }
        return players[left] <= players[right] ? left : right;
    }

    static std::size_t worse(const std::vector<int>& players, std::size_t left,
                             std::size_t right) {
        if (left == no_player) {
            return left;
        }
        if (right == no_player) {
            return right;
        }
        return players[left] <= players[right] ? right : left;
    }
};
}  // namespace detail

// >>> winner-tree
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
// <<< winner-tree

// >>> loser-tree
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
// <<< loser-tree

}  // namespace dsa::external_sort
