// 最小堆与 Huffman 树 —— 教学版。原书【代码5.11】【代码5.12】。
//
// 一个文件、两个类、能直接编译运行，给「第一次读这一节」的人看。
//
//   MinHeap      完全二叉树用**数组**存：下标 i 的孩子是 2i+1 和 2i+2，父亲是 (i-1)/2。
//                不需要任何指针，这正是这一节最漂亮的地方。
//   HuffmanTree  反复「取两个最小的合并」，用最小堆来取——这是堆的第一个真实用途。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则、扩容不考虑异常、Huffman 构造不做溢出与失败清理；
//   工程版  五法则、对元素类型的 static_assert、构造中途失败时逐个回收裸结点、
//           权重相加的溢出检查。
// 两份都在闸门里真编译真运行。先读这一份，5.5a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>

// ---------------------------------------------------------------------------
// 最小堆
//
// 堆是一棵**完全二叉树**（除最后一层外每层排满，最后一层靠左连续），
// 且每个结点都不大于它的孩子。完全二叉树可以按层次次序压进一个数组，
// 于是父子关系变成下标算术：
//
//   下标 i 的左孩子  2i + 1
//   下标 i 的右孩子  2i + 2
//   下标 i 的父亲    (i - 1) / 2       （i > 0）
//
// 一根指针都不用。
// ---------------------------------------------------------------------------
template <typename T>
class MinHeap {
public:
    using size_type = std::size_t;

    explicit MinHeap(size_type initial_capacity = 8)
        : data_(new T[initial_capacity]), capacity_(initial_capacity), size_(0) {}

    ~MinHeap() { delete[] data_; }

    // 三法则：管着 new 出来的数组，拷贝必须自己写。
    MinHeap(const MinHeap& other)
        : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    MinHeap& operator=(const MinHeap& other) {
        if (this == &other) {
            return *this;
        }
        T* fresh = new T[other.capacity_];
        for (size_type i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];
        }
        delete[] data_;
        data_ = fresh;
        capacity_ = other.capacity_;
        size_ = other.size_;
        return *this;
    }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }

    // 插入：先放到数组末尾（也就是完全二叉树的最后一个位置），
    // 再一路和父亲比较、必要时上浮。树高是 log n，所以代价是 O(log n)。
    void insert(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_] = value;
        sift_up(size_);
        ++size_;
    }

    // 取走最小的那个（就是根，下标 0）。空堆返回空 optional。
    //
    // 手法是固定的：把**最后一个**元素搬到根上，长度减一，然后让它一路下沉。
    // 为什么是最后一个？因为只有拿掉最后一个位置，剩下的才仍然是一棵完全二叉树。
    std::optional<T> remove_min() {
        if (empty()) {
            return std::nullopt;
        }
        T smallest = data_[0];
        --size_;
        if (size_ > 0) {
            data_[0] = data_[size_];
            sift_down(0);
        }
        return smallest;
    }

private:
    // 上浮：只要比父亲小就换上去。
    void sift_up(size_type index) {
        while (index > 0) {
            size_type parent = (index - 1) / 2;
            if (!(data_[index] < data_[parent])) {
                break;                       // 已经不小于父亲，位置对了
            }
            T tmp = data_[index];
            data_[index] = data_[parent];
            data_[parent] = tmp;
            index = parent;
        }
    }

    // 下沉：和两个孩子里较小的那个比，比它大就换下去。
    // **必须和较小的那个换**——跟较大的换会破坏「父亲不大于两个孩子」。
    void sift_down(size_type index) {
        for (;;) {
            size_type left = index * 2 + 1;
            size_type right = left + 1;
            size_type smallest = index;
            if (left < size_ && data_[left] < data_[smallest]) {
                smallest = left;
            }
            if (right < size_ && data_[right] < data_[smallest]) {
                smallest = right;
            }
            if (smallest == index) {
                return;                      // 父亲已经最小，停
            }
            T tmp = data_[index];
            data_[index] = data_[smallest];
            data_[smallest] = tmp;
            index = smallest;
        }
    }

    void grow() {
        size_type next = (capacity_ == 0) ? 1 : capacity_ * 2;
        T* fresh = new T[next];
        for (size_type i = 0; i < size_; ++i) {
            fresh[i] = data_[i];
        }
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }

    T* data_;
    size_type capacity_;
    size_type size_;
};

// ---------------------------------------------------------------------------
// Huffman 树
//
// 构造规则只有一句：**反复取出权最小的两棵树，合并成一棵新树放回去**，
// 直到只剩一棵。「取最小的」正是最小堆的拿手好戏，两节内容在这里接上了。
// ---------------------------------------------------------------------------
class HuffmanTree {
public:
    struct Node {
        int weight;
        Node* left;
        Node* right;
    };

    HuffmanTree() : root_(nullptr) {}

    HuffmanTree(const int* weights, std::size_t count) : root_(nullptr) {
        if (count == 0) {
            return;
        }
        if (weights == nullptr) {
            throw std::invalid_argument("HuffmanTree: 权重数组是空指针");
        }

        // **先把参数全查一遍，再动手 new。** 顺序反过来的话，
        // 在第 k 个权重上发现非法值时前 k-1 个结点已经建好了，
        // 抛出去就全漏了——LeakSanitizer 会当场把它报出来（作者第一版正是如此）。
        for (std::size_t i = 0; i < count; ++i) {
            if (weights[i] < 0) {
                throw std::invalid_argument("HuffmanTree: 权重不能为负");
            }
        }

        MinHeap<ByWeight> heap;
        for (std::size_t i = 0; i < count; ++i) {
            heap.insert(ByWeight{new Node{weights[i], nullptr, nullptr}});   // 每个权重先做成一棵单结点树
        }

        while (heap.size() > 1) {
            Node* left = heap.remove_min()->node;      // 最小的
            Node* right = heap.remove_min()->node;     // 次小的
            Node* parent = new Node{left->weight + right->weight, left, right};
            heap.insert(ByWeight{parent});
        }
        root_ = heap.remove_min()->node;
    }

    ~HuffmanTree() { destroy(root_); }

    // 这棵树不支持拷贝：结点是裸指针，深拷贝要写一整套，而 Huffman 树建好就只读。
    // 明确 `= delete` 好过让编译器悄悄生成一个会二次释放的版本。
    HuffmanTree(const HuffmanTree&) = delete;
    HuffmanTree& operator=(const HuffmanTree&) = delete;

    const Node* root() const { return root_; }

    // 根的权重就是所有叶子权重之和。
    int total_weight() const { return root_ == nullptr ? 0 : root_->weight; }

    // 带权路径长度(WPL)：每个叶子的权重乘以它的深度，再求和。
    // Huffman 树的意义就在于它让这个数最小。
    int weighted_path_length() const { return wpl(root_, 0); }

private:
    // 放进堆里的是「一棵树的根指针」，比较的是它的权重。
    struct ByWeight {
        Node* node;
        bool operator<(const ByWeight& other) const {
            return node->weight < other.node->weight;
        }
    };

    static void destroy(Node* node) {
        if (node == nullptr) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    static int wpl(const Node* node, int depth) {
        if (node == nullptr) return 0;
        if (node->left == nullptr && node->right == nullptr) {
            return node->weight * depth;      // 叶子
        }
        return wpl(node->left, depth + 1) + wpl(node->right, depth + 1);
    }

    Node* root_;
};
