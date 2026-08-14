// 原书【代码5.11】【代码5.12】：手写最小堆与 Huffman 合并树。
#pragma once
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dsa {
// >>> min-heap
template <typename T>
class MinHeap {
public:
    static_assert(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value,
                  "MinHeap growth relies on non-throwing moves; use a noexcept-movable element type.");

    MinHeap() = default;
    MinHeap(const MinHeap& other) : data_(other.capacity_ ? new T[other.capacity_] : nullptr), size_(other.size_), capacity_(other.capacity_) {
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
            other.data_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }
        return *this;
    }
    ~MinHeap() { delete[] data_; }
    void swap(MinHeap& other) noexcept { using std::swap; swap(data_, other.data_); swap(size_, other.size_); swap(capacity_, other.capacity_); }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
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
        while (index != 0 && data_[index] < data_[(index - 1) / 2]) {
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
            if (left < size_ && data_[left] < data_[smallest]) smallest = left;
            if (right < size_ && data_[right] < data_[smallest]) smallest = right;
            if (smallest == index) return;
            using std::swap;
            swap(data_[index], data_[smallest]);
            index = smallest;
        }
    }
    T* data_{nullptr};
    std::size_t size_{0};
    std::size_t capacity_{0};
};
// <<< min-heap

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
    /// 后序释放，先删子树再删父结点；Huffman 树高度受权重分布影响，仍需关注深度上界。
    static void destroy(Node* node) noexcept {
        if (node == nullptr) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    Node* root_{nullptr};
};
// <<< huffman
}
