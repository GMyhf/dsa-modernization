// 原书【代码5.11】【代码5.12】：手写最小堆与 Huffman 合并树。
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dsa {
// >>> min-heap
/// Compare 默认 std::less<T>，堆顶是「按 Compare 最小」的元素；
/// 传 std::greater<T> 就是最大堆（对顶堆求中位数要用它）。
template <typename T, typename Compare = std::less<T>>
class MinHeap {
public:
    static_assert(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value,
                  "MinHeap growth relies on non-throwing moves; use a noexcept-movable element type.");

    MinHeap() = default;
    MinHeap(const MinHeap& other) : data_(other.capacity_ ? new T[other.capacity_] : nullptr), size_(other.size_), capacity_(other.capacity_), compare_(other.compare_) {
        try { for (std::size_t i = 0; i < size_; ++i) data_[i] = other.data_[i]; }
        catch (...) { delete[] data_; throw; }
    }
    MinHeap& operator=(const MinHeap& other) { if (this != &other) { MinHeap copy(other); swap(copy); } return *this; }
    MinHeap(MinHeap&& other) noexcept { swap(other); }
    MinHeap& operator=(MinHeap&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            compare_ = other.compare_;
            other.data_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }
        return *this;
    }
    ~MinHeap() { delete[] data_; }
    void swap(MinHeap& other) noexcept { using std::swap; swap(data_, other.data_); swap(size_, other.size_); swap(capacity_, other.capacity_); swap(compare_, other.compare_); }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    /// 只看堆顶、不拷贝；空堆为 nullptr。指针在下一次 insert / remove_min 后失效（D-001 §3b）。
    [[nodiscard]] const T* peek() const noexcept { return size_ == 0 ? nullptr : &data_[0]; }
    void insert(const T& value) { ensure_capacity(); data_[size_] = value; sift_up(size_++); }
    void insert(T&& value) { ensure_capacity(); data_[size_] = std::move(value); sift_up(size_++); }
    [[nodiscard]] std::optional<T> remove_min() {
        if (empty()) return std::nullopt;
        T value = std::move(data_[0]);
        --size_;
        if (size_ == 0) return value;
        data_[0] = std::move(data_[size_]);
        sift_down(0);
        return value;
    }
private:
    void ensure_capacity() {
        if (size_ < capacity_) return;
        const std::size_t next = capacity_ == 0 ? 4 : capacity_ * 2;
        T* fresh = new T[next];
        // The class contract requires non-throwing move assignment, so the
        // migration loop cannot fail. Allocation failure is thrown before fresh exists.
        for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }
    void sift_up(std::size_t index) {
        while (index != 0 && compare_(data_[index], data_[(index - 1) / 2])) {
            using std::swap;
            swap(data_[index], data_[(index - 1) / 2]);
            index = (index - 1) / 2;
        }
    }
    void sift_down(std::size_t index) {
        for (;;) {
            const std::size_t left = index * 2 + 1;
            const std::size_t right = left + 1;
            std::size_t smallest = index;
            if (left < size_ && compare_(data_[left], data_[smallest])) smallest = left;
            if (right < size_ && compare_(data_[right], data_[smallest])) smallest = right;
            if (smallest == index) return;
            using std::swap;
            swap(data_[index], data_[smallest]);
            index = smallest;
        }
    }
    T* data_{nullptr};
    std::size_t size_{0};
    std::size_t capacity_{0};
    Compare compare_{};
};
// <<< min-heap

// >>> running-median
/// 对顶堆求动态中位数：较小的一半放进**最大堆** lower_，较大的一半放进**最小堆** upper_。
///
/// 不变式：lower_ 的每个元素 ≤ upper_ 的每个元素，且
///         lower_.size() == upper_.size() 或 lower_.size() == upper_.size() + 1。
/// 于是中位数永远是 lower_ 的堆顶。元素个数为偶数时取**下中位数**（第 n/2 小，从 1 数），
/// 不做两数平均——那需要 T 支持除法，而且整数平均会截断，调用方要平均就自己取两个堆顶。
/// insert 为 O(log n)，median 为 O(1)。
template <typename T>
class RunningMedian {
public:
    void insert(const T& value) {
        if (lower_.empty() || !(*lower_.peek() < value)) lower_.insert(value);
        else upper_.insert(value);
        rebalance();
    }
    /// 空时 std::nullopt。
    [[nodiscard]] std::optional<T> median() const {
        if (const T* top = lower_.peek()) return *top;
        return std::nullopt;
    }
    [[nodiscard]] std::size_t size() const noexcept { return lower_.size() + upper_.size(); }

private:
    /// 每次插入后两堆大小至多差 2，搬一个堆顶就回到不变式。
    void rebalance() {
        if (lower_.size() > upper_.size() + 1) upper_.insert(*lower_.remove_min());
        else if (upper_.size() > lower_.size()) lower_.insert(*upper_.remove_min());
    }
    MinHeap<T, std::greater<T>> lower_;   // 较小的一半，堆顶是其中最大的
    MinHeap<T> upper_;                    // 较大的一半，堆顶是其中最小的
};
// <<< running-median

// >>> huffman
class HuffmanTree {
    /// 树结点不拥有父指针；整棵树的所有权由 root_ 持有，合并期间由最小堆暂管。
    struct Node {
        int weight;
        Node* left{nullptr};
        Node* right{nullptr};
        explicit Node(int w) : weight(w) {}
    };
    /// 堆只按权重排序，不负责删除 node；异常路径必须显式回收这些裸指针。
    struct ByWeight {
        Node* node{nullptr};
        bool operator<(const ByWeight& other) const noexcept {
            return node->weight < other.node->weight;
        }
    };
public:
    HuffmanTree()=default;
    explicit HuffmanTree(const int* weights, std::size_t count) {
        if (count == 0) return;
        if (weights == nullptr) throw std::invalid_argument("non-empty Huffman input requires weights");
        MinHeap<ByWeight> heap;
        try {
            for (std::size_t i = 0; i < count; ++i) {
                if (weights[i] < 0) throw std::invalid_argument("Huffman weights must be non-negative");
                Node* leaf = new Node(weights[i]);
                try { heap.insert(ByWeight{leaf}); }
                catch (...) { delete leaf; throw; }
            }
            while (heap.size() > 1) {
                Node* left = heap.remove_min()->node;
                Node* right = heap.remove_min()->node;
                Node* parent = nullptr;
                try {
                    if (left->weight > std::numeric_limits<int>::max() - right->weight) {
                        throw std::overflow_error("Huffman weight sum overflows int");
                    }
                    parent = new Node(left->weight + right->weight);
                    parent->left = left;
                    parent->right = right;
                    heap.insert(ByWeight{parent});
                } catch (...) {
                    if (parent != nullptr) { parent->left = parent->right = nullptr; delete parent; }
                    destroy(left);
                    destroy(right);
                    throw;
                }
            }
            root_ = heap.remove_min()->node;
        } catch (...) {
            while (auto item = heap.remove_min()) destroy(item->node);
            throw;
        }
    }
    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator=(const HuffmanTree&) = delete;

    /// 移动只转移根指针，并立即清空源对象，避免两个对象重复释放同一棵树。
    HuffmanTree(HuffmanTree&& other) noexcept : root_(other.root_) {
        other.root_ = nullptr;
    }

    HuffmanTree& operator=(HuffmanTree&& other) noexcept {
        if (this != &other) {
            destroy(root_);
            root_ = other.root_;
            other.root_ = nullptr;
        }
        return *this;
    }

    ~HuffmanTree() { destroy(root_); }
    [[nodiscard]] int total_weight() const noexcept {
        return root_ ? root_->weight : 0;
    }
private:
    /// 释放整棵树，**不递归**、额外空间 O(1)：有左孩子就把它右旋到上面，没有就删掉当前结点走右边。
    /// 每次旋转让某个结点离开左链一次，总步数 O(n)。
    ///
    /// 为什么不用递归（T-075，Codex 复核抓出）：权重允许为 0，同权结点平局时堆会把树合并成一条链，
    /// 高度可以等于叶子数。实测 100 万个 0 权叶子，递归版析构在 -O2 下段错误、ASan 下 stack-overflow。
    /// 正权不会这样——高为 h 的 Huffman 树总权至少是斐波那契数 F(h+1)，int 放得下时 h 不超过约 45。
    static void destroy(Node* node) noexcept {
        while (node != nullptr) {
            if (node->left != nullptr) {
                Node* left = node->left;          // 右旋：左孩子升上来，当前结点挂到它的右边
                node->left = left->right;
                left->right = node;
                node = left;
            } else {
                Node* right = node->right;
                delete node;
                node = right;
            }
        }
    }

    Node* root_{nullptr};
};
// <<< huffman

// >>> huffman-wpl-batched
/// 一组同权叶子：weight 这个权出现了 count 次。
struct WeightGroup {
    std::uint64_t weight;
    std::uint64_t count;
};

/// 只求 Huffman 树的带权路径长度，不建树——叶子数可达 1e9 量级，结点根本放不下。
///
/// 依据：WPL 等于**所有内部结点的权之和**（每个叶子的权在它的每个祖先里各被加一次，
/// 祖先个数正是它的深度）。所以只要把每次合并出的新权累加起来。
/// 又因为同权的若干棵树谁先合并都一样，最小的一组有 c 棵时，一次就合并出 c/2 棵
/// 权为 2w 的树；c 为奇数时剩下的那一棵要和**下一小**的树合并，不能丢。
/// 堆里放的是「组」，一轮处理一个最小权的组。
///
/// 轮数：**只证明了平凡上界**——每轮至少造出一个内部结点，所以至多 n−1 轮（n = 叶子总数）；
/// n 可达 1e9，这个界没有实用价值。更紧的界**没有证明**。实测（T-075，Python 逐行移植计数）：
/// 6 个组、count 取 1/2/3/≈1e9 的对抗混合最坏 352 轮，约「每组 log₂n 轮」；
/// 1e9 叶子的单组 101 轮。早先写在这里的 O(k log k + log n) 被这组实测**否定**了，已撤掉。
///
/// 溢出界：设总权 W = Σ weight·count、叶子数 n = Σ count，则 WPL ≤ W·⌈log2 n⌉。
/// 该乘积小于 2^64 时一定不溢出；否则中途任何一次加法/乘法溢出都抛 std::overflow_error，
/// 不返回回绕后的错数。count 为 0 的组忽略；叶子数 ≤ 1 时 WPL 为 0。
inline std::uint64_t huffman_wpl_batched(const WeightGroup* groups, std::size_t group_count) {
    if (group_count != 0 && groups == nullptr) {
        throw std::invalid_argument("huffman_wpl_batched: non-empty input requires groups");
    }
    struct ByWeight {
        std::uint64_t weight, count;
        bool operator<(const ByWeight& other) const noexcept { return weight < other.weight; }
    };
    const auto add = [](std::uint64_t a, std::uint64_t b) {
        if (a > std::numeric_limits<std::uint64_t>::max() - b) throw std::overflow_error("Huffman WPL overflows uint64");
        return a + b;
    };
    const auto mul = [](std::uint64_t a, std::uint64_t b) {
        if (b != 0 && a > std::numeric_limits<std::uint64_t>::max() / b) throw std::overflow_error("Huffman WPL overflows uint64");
        return a * b;
    };

    MinHeap<ByWeight> heap;
    for (std::size_t i = 0; i < group_count; ++i) {
        if (groups[i].count != 0) heap.insert(ByWeight{groups[i].weight, groups[i].count});
    }
    std::uint64_t wpl = 0;
    while (auto smallest = heap.remove_min()) {
        ByWeight group = *smallest;
        while (heap.peek() != nullptr && heap.peek()->weight == group.weight) {   // 同权的组并成一组
            group.count = add(group.count, heap.remove_min()->count);
        }
        if (group.count == 1) {
            auto next = heap.remove_min();
            if (!next) break;                              // 只剩一棵树：它就是根
            const std::uint64_t merged = add(group.weight, next->weight);
            wpl = add(wpl, merged);
            heap.insert(ByWeight{merged, 1});
            if (next->count > 1) heap.insert(ByWeight{next->weight, next->count - 1});
        } else {
            const std::uint64_t pairs = group.count / 2;
            const std::uint64_t merged = mul(group.weight, 2);
            wpl = add(wpl, mul(merged, pairs));            // pairs 个内部结点，各权 2w
            if (group.count % 2 == 1) heap.insert(ByWeight{group.weight, 1});   // 奇数余下的一棵
            heap.insert(ByWeight{merged, pairs});
        }
    }
    return wpl;
}
// <<< huffman-wpl-batched
}
