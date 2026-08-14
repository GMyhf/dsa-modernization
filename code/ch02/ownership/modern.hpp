#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace dsa::ownership {

// >>> recursive-chain
/// **教学反例。** 用 `std::unique_ptr` 把结点串成链，看起来最干净：没有 `delete`，
/// 没有析构函数，五法则一条都不用写。
///
/// 代价藏在编译器替你生成的析构里：`~RecursiveNode` 要析构 `next`，`next` 的析构又要
/// 析构它的 `next`……链有多长，栈就压多深。链表通常正是「元素很多」的结构，
/// 于是一次普通的析构就能把栈压穿——而且崩溃阈值随优化级别变，debug 崩、release 过。
///
/// 实测数字与复现命令见本单元的 `legacy.md`。
struct RecursiveNode {
    int value = 0;
    std::unique_ptr<RecursiveNode> next;
};

class RecursiveChain {
public:
    void push_front(int value) {
        auto node = std::make_unique<RecursiveNode>();
        node->value = value;
        node->next = std::move(head_);
        head_ = std::move(node);
        ++size_;
    }

    [[nodiscard]] std::vector<int> to_vector() const {
        std::vector<int> out;
        for (const RecursiveNode* cursor = head_.get(); cursor != nullptr;
             cursor = cursor->next.get()) {
            out.push_back(cursor->value);
        }
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    std::unique_ptr<RecursiveNode> head_;
    std::size_t size_ = 0;
    // 析构函数是编译器生成的——问题就出在这里：它是递归的，而且看不见。
};
// <<< recursive-chain

// >>> iterative-chain
/// 同一条链，所有权自己管。多写了一个析构和一个 `clear()`，换来的是
/// **栈深度与链长无关**：释放走循环，一次一个结点。
///
/// 这正是 `code/ch02/linked_list` 采用的写法。多出来的那几行不是仪式，
/// 是这个结构能不能处理大数据的分界线。
class IterativeChain {
public:
    IterativeChain() = default;
    IterativeChain(const IterativeChain& other) { copy_from(other); }
    IterativeChain(IterativeChain&& other) noexcept
        : head_(other.head_), size_(other.size_) {
        other.head_ = nullptr;
        other.size_ = 0;
    }
    IterativeChain& operator=(const IterativeChain& other) {
        if (this != &other) {
            IterativeChain copy(other);
            swap(copy);
        }
        return *this;
    }
    IterativeChain& operator=(IterativeChain&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            size_ = other.size_;
            other.head_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    ~IterativeChain() { clear(); }

    void swap(IterativeChain& other) noexcept {
        std::swap(head_, other.head_);
        std::swap(size_, other.size_);
    }

    void push_front(int value) {
        head_ = new Node{value, head_};
        ++size_;
    }

    /// 释放整条链。**循环，不是递归**——栈深度恒定，五百万个结点也不会压穿。
    void clear() noexcept {
        Node* cursor = head_;
        while (cursor != nullptr) {
            Node* following = cursor->next;
            delete cursor;
            cursor = following;
        }
        head_ = nullptr;
        size_ = 0;
    }

    [[nodiscard]] std::vector<int> to_vector() const {
        std::vector<int> out;
        for (const Node* cursor = head_; cursor != nullptr; cursor = cursor->next) {
            out.push_back(cursor->value);
        }
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    struct Node {
        int value = 0;
        Node* next = nullptr;
    };

    void copy_from(const IterativeChain& other) {
        // 逆序收集再逆序建链，保持元素顺序，并且同样不递归。
        const std::vector<int> values = other.to_vector();
        for (std::size_t i = values.size(); i > 0; --i) {
            push_front(values[i - 1]);
        }
    }

    Node* head_ = nullptr;
    std::size_t size_ = 0;
};
// <<< iterative-chain

}  // namespace dsa::ownership
