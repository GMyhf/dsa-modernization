#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dsa::fenwick {

// >>> fenwick
/// 动态前缀和：公共下标从 0 开始，区间采用 [left, right) 半开约定。
class FenwickTree {
public:
    explicit FenwickTree(std::size_t size)
        : values_(size, 0), tree_(size + 1, 0) {}

    std::size_t size() const noexcept { return values_.size(); }

    static std::size_t lowbit(std::size_t index) noexcept {
        return index & (~index + 1);
    }

    void add(std::size_t index, long long delta) {
        check_index(index);
        values_[index] += delta;
        for (std::size_t cursor = index + 1; cursor <= size(); cursor += lowbit(cursor)) {
            tree_[cursor] += delta;
        }
    }

    void set(std::size_t index, long long value) {
        check_index(index);
        add(index, value - values_[index]);
    }

    long long prefix_sum(std::size_t end) const {
        if (end > size()) {
            throw std::out_of_range("Fenwick prefix end out of range");
        }
        long long result = 0;
        for (std::size_t cursor = end; cursor != 0; cursor -= lowbit(cursor)) {
            result += tree_[cursor];
        }
        return result;
    }

    long long range_sum(std::size_t left, std::size_t right) const {
        if (left > right || right > size()) {
            throw std::out_of_range("Fenwick range out of range");
        }
        return prefix_sum(right) - prefix_sum(left);
    }

    long long value_at(std::size_t index) const {
        check_index(index);
        return values_[index];
    }

private:
    void check_index(std::size_t index) const {
        if (index >= size()) {
            throw std::out_of_range("Fenwick index out of range");
        }
    }

    std::vector<long long> values_;
    std::vector<long long> tree_;
};
// <<< fenwick

}  // namespace dsa::fenwick
