# 第8章 内部排序

排序单元覆盖插入、Shell、选择、堆、冒泡、快速、归并、计数、基数和索引排序。所有实现接受有符号整数，测试含重复和负数。

```cpp file=code/ch08/sorting/modern.hpp
// 第 8 章内部排序：原书【算法8.1】至【代码8.17】的可读、可运行实现。
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa::sorting {

// >>> sorting

// 算法8.1：直接插入排序。相等元素不越过彼此，故稳定。
inline void insertion_sort(std::vector<int>& values) {
    for (std::size_t index = 1; index < values.size(); ++index) {
        const int value = values[index];
        std::size_t hole = index;
        while (hole != 0 && value < values[hole - 1]) {
            values[hole] = values[hole - 1];
            --hole;
        }
        values[hole] = value;
    }
}

// 算法8.2：增量每次减半的 Shell 排序。
inline void shell_sort(std::vector<int>& values) {
    for (std::size_t gap = values.size() / 2; gap != 0; gap /= 2) {
        for (std::size_t index = gap; index < values.size(); ++index) {
            const int value = values[index];
            std::size_t hole = index;
            while (hole >= gap && value < values[hole - gap]) {
                values[hole] = values[hole - gap];
                hole -= gap;
            }
            values[hole] = value;
        }
    }
}

// 算法8.3：直接选择排序。
inline void selection_sort(std::vector<int>& values) {
    for (std::size_t first = 0; first < values.size(); ++first) {
        std::size_t minimum = first;
        for (std::size_t index = first + 1; index < values.size(); ++index) {
            if (values[index] < values[minimum]) minimum = index;
        }
        using std::swap;
        swap(values[first], values[minimum]);
    }
}

// 算法8.4：手写最大堆筛选与堆排序，不委托 std::make_heap/sort_heap。
inline void sift_down(std::vector<int>& values, std::size_t root, std::size_t count) {
    while (root * 2 + 1 < count) {
        std::size_t child = root * 2 + 1;
        if (child + 1 < count && values[child] < values[child + 1]) ++child;
        if (values[root] >= values[child]) return;
        using std::swap;
        swap(values[root], values[child]);
        root = child;
    }
}

inline void heap_sort(std::vector<int>& values) {
    for (std::size_t root = values.size() / 2; root != 0; --root) {
        sift_down(values, root - 1, values.size());
    }
    for (std::size_t end = values.size(); end > 1; --end) {
        using std::swap;
        swap(values[0], values[end - 1]);
        sift_down(values, 0, end - 1);
    }
}

// 算法8.5：带“本趟无交换即结束”优化的冒泡排序。
inline void bubble_sort(std::vector<int>& values) {
    for (std::size_t end = values.size(); end > 1; --end) {
        bool changed = false;
        for (std::size_t index = 1; index < end; ++index) {
            if (values[index] < values[index - 1]) {
                using std::swap;
                swap(values[index], values[index - 1]);
                changed = true;
            }
        }
        if (!changed) return;
    }
}

inline std::size_t partition(std::vector<int>& values, std::size_t first, std::size_t last) {
    const int pivot = values[last - 1];
    std::size_t boundary = first;
    for (std::size_t index = first; index + 1 < last; ++index) {
        if (values[index] < pivot) {
            using std::swap;
            swap(values[boundary], values[index]);
            ++boundary;
        }
    }
    using std::swap;
    swap(values[boundary], values[last - 1]);
    return boundary;
}

inline void quick_sort_range(std::vector<int>& values, std::size_t first, std::size_t last) {
    if (last - first < 2) return;
    const std::size_t middle = partition(values, first, last);
    quick_sort_range(values, first, middle);
    quick_sort_range(values, middle + 1, last);
}

// 算法8.6：手写快排。
inline void quick_sort(std::vector<int>& values) { quick_sort_range(values, 0, values.size()); }

// 算法8.7：小分区转插入排序、优先递归短侧以限制栈深。
inline void quick_sort_optimized_range(std::vector<int>& values, std::size_t first, std::size_t last) {
    while (last - first > 16) {
        const std::size_t middle = partition(values, first, last);
        if (middle - first < last - middle - 1) {
            quick_sort_optimized_range(values, first, middle);
            first = middle + 1;
        } else {
            quick_sort_optimized_range(values, middle + 1, last);
            last = middle;
        }
    }
    for (std::size_t index = first + 1; index < last; ++index) {
        const int value = values[index];
        std::size_t hole = index;
        while (hole != first && value < values[hole - 1]) {
            values[hole] = values[hole - 1];
            --hole;
        }
        values[hole] = value;
    }
}

inline void quick_sort_optimized(std::vector<int>& values) {
    quick_sort_optimized_range(values, 0, values.size());
}

inline void merge_ranges(std::vector<int>& values, std::vector<int>& buffer,
                         std::size_t first, std::size_t middle, std::size_t last) {
    std::size_t left = first;
    std::size_t right = middle;
    std::size_t output = first;
    while (left < middle && right < last) {
        buffer[output++] = values[right] < values[left] ? values[right++] : values[left++];
    }
    while (left < middle) buffer[output++] = values[left++];
    while (right < last) buffer[output++] = values[right++];
    for (std::size_t index = first; index < last; ++index) values[index] = buffer[index];
}

inline void merge_sort_range(std::vector<int>& values, std::vector<int>& buffer,
                             std::size_t first, std::size_t last) {
    if (last - first < 2) return;
    const std::size_t middle = first + (last - first) / 2;
    merge_sort_range(values, buffer, first, middle);
    merge_sort_range(values, buffer, middle, last);
    merge_ranges(values, buffer, first, middle, last);
}

// 算法8.8：两路归并排序。
inline void merge_sort(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    merge_sort_range(values, buffer, 0, values.size());
}

// 算法8.9：已有序时跳过 merge；小分区改用插入排序。
inline void merge_sort_optimized_range(std::vector<int>& values, std::vector<int>& buffer,
                                       std::size_t first, std::size_t last) {
    if (last - first <= 16) {
        for (std::size_t index = first + 1; index < last; ++index) {
            const int value = values[index];
            std::size_t hole = index;
            while (hole != first && value < values[hole - 1]) { values[hole] = values[hole - 1]; --hole; }
            values[hole] = value;
        }
        return;
    }
    const std::size_t middle = first + (last - first) / 2;
    merge_sort_optimized_range(values, buffer, first, middle);
    merge_sort_optimized_range(values, buffer, middle, last);
    if (values[middle] < values[middle - 1]) merge_ranges(values, buffer, first, middle, last);
}

inline void merge_sort_optimized(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    merge_sort_optimized_range(values, buffer, 0, values.size());
}

// 算法8.10：桶式（计数）排序，支持负数但不适合巨大稀疏值域。
inline void counting_sort(std::vector<int>& values) {
    if (values.empty()) return;
    int low = values[0];
    int high = values[0];
    for (int value : values) { if (value < low) low = value; if (high < value) high = value; }
    const auto range = static_cast<unsigned long long>(static_cast<long long>(high) - low + 1);
    if (range > 10'000'000ULL) throw std::invalid_argument("counting sort value range is too sparse");
    std::vector<std::size_t> counts(static_cast<std::size_t>(range), 0);
    for (int value : values) ++counts[static_cast<std::size_t>(value - low)];
    std::size_t output = 0;
    for (std::size_t bucket = 0; bucket < counts.size(); ++bucket) {
        while (counts[bucket]-- != 0) values[output++] = static_cast<int>(bucket) + low;
    }
}

// 代码8.12：固定容量 FIFO，是基数排序的桶而非通用 STL queue 替身。
template <typename T>
class StaticQueue {
public:
    explicit StaticQueue(std::size_t capacity) : data_(capacity), capacity_(capacity) {}
    [[nodiscard]] bool push(const T& value) {
        if (size_ == capacity_) return false;
        data_[(front_ + size_++) % capacity_] = value;
        return true;
    }
    [[nodiscard]] std::optional<T> pop() {
        if (size_ == 0) return std::nullopt;
        T value = data_[front_];
        front_ = (front_ + 1) % capacity_;
        --size_;
        return value;
    }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
private:
    std::vector<T> data_;
    std::size_t capacity_{0};
    std::size_t front_{0};
    std::size_t size_{0};
};

// 算法8.11：LSD 基数排序。翻转符号位使二补码有符号 int 按无符号序排序。
inline void radix_sort(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    for (unsigned shift = 0; shift < 32; shift += 8) {
        std::size_t counts[256]{};
        for (int value : values) {
            const auto key = static_cast<std::uint32_t>(value) ^ 0x80000000U;
            ++counts[(key >> shift) & 0xffU];
        }
        std::size_t offset = 0;
        for (std::size_t& count : counts) { const std::size_t old = count; count = offset; offset += old; }
        for (int value : values) {
            const auto key = static_cast<std::uint32_t>(value) ^ 0x80000000U;
            buffer[counts[(key >> shift) & 0xffU]++] = value;
        }
        values.swap(buffer);
    }
}

// 算法8.13：以显式桶队列演示顺序收集的基数排序。
inline void radix_sort_linked_style(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    for (unsigned shift = 0; shift < 32; shift += 8) {
        std::vector<StaticQueue<int>> buckets;
        buckets.reserve(256);
        for (std::size_t bucket = 0; bucket < 256; ++bucket) buckets.emplace_back(values.size());
        for (int value : values) {
            const auto key = static_cast<std::uint32_t>(value) ^ 0x80000000U;
            (void)buckets[(key >> shift) & 0xffU].push(value);
        }
        std::size_t output = 0;
        for (auto& bucket : buckets) while (auto value = bucket.pop()) buffer[output++] = *value;
        values.swap(buffer);
    }
}

// 算法8.14：排序索引，不移动原记录。
inline std::vector<std::size_t> insertion_index_sort(const std::vector<int>& values) {
    std::vector<std::size_t> indexes(values.size());
    for (std::size_t i = 0; i < indexes.size(); ++i) indexes[i] = i;
    for (std::size_t i = 1; i < indexes.size(); ++i) {
        const std::size_t index = indexes[i];
        std::size_t hole = i;
        while (hole != 0 && values[index] < values[indexes[hole - 1]]) { indexes[hole] = indexes[hole - 1]; --hole; }
        indexes[hole] = index;
    }
    return indexes;
}

// 算法8.15：沿置换环把索引顺序落实为记录顺序。
inline void adjust_by_index(std::vector<int>& values, std::vector<std::size_t>& indexes) {
    for (std::size_t first = 0; first < values.size(); ++first) {
        if (indexes[first] == first) continue;
        std::size_t current = first;
        const int saved = values[first];
        while (indexes[current] != first) {
            const std::size_t source = indexes[current];
            values[current] = values[source];
            indexes[current] = current;
            current = source;
        }
        values[current] = saved;
        indexes[current] = current;
    }
}

// 代码8.16：可复现随机数据；代码8.17：单调时钟计时。
inline std::vector<int> random_values(std::size_t count, int upper_bound, unsigned seed = 1) {
    std::mt19937 engine(seed);
    std::uniform_int_distribution<int> distribution(0, upper_bound - 1);
    std::vector<int> values(count);
    for (int& value : values) value = distribution(engine);
    return values;
}

class Stopwatch {
public:
    void start() noexcept { started_ = std::chrono::steady_clock::now(); }
    [[nodiscard]] double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    }
private:
    std::chrono::steady_clock::time_point started_{};
};

// <<< sorting

}  // namespace dsa::sorting
```
