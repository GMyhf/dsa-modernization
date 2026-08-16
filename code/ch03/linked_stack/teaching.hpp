// 链式栈 LinkedStack —— 教学版。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
// 本节要教的是「同一个 ADT 换一种存储结构」：顺序栈用连续数组，链式栈用结点串联。
// 所以接口形状与 ArrayStack 刻意保持一致，两者才好拿来对比。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则（析构 + 拷贝构造 + 拷贝赋值），正确，但拷贝多一点；
//   工程版  在此之上补齐移动语义、拷贝失败时的清理、零拷贝的 peek()。
// 两份都在闸门里真编译真运行。先读这一份，3.1.3a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>

template <typename T>
class LinkedStack {
public:
    using value_type = T;
    using size_type = std::size_t;

    // 链式栈**不需要预设容量**，所以构造函数没有参数。
    // 原书的 `lnkStack(int defSize)` 是从顺序栈那边照抄过来的，那个参数一次都没用过。
    LinkedStack() : top_(nullptr), size_(0) {}

    ~LinkedStack() { clear(); }

    // 三法则：这个类自己管着一串 new 出来的结点，拷贝必须自己写。
    // 不写的话两个栈会共享同一串结点，各析构一次 → 二次释放。
    // 原书 lnkStack 有析构函数却没有这两个，与顺序栈是同一个错误。
    LinkedStack(const LinkedStack& other) : top_(nullptr), size_(0) {
        copy_from(other);
    }

    LinkedStack& operator=(const LinkedStack& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        copy_from(other);
        return *this;
    }

    // 入栈：造一个新结点，让它指向原来的栈顶，再让栈顶指向它。
    // **没有「栈满」这回事**——这正是链式栈相对顺序栈最大的差别。
    void push(const T& value) {
        Node* fresh = new Node;
        fresh->value = value;
        fresh->next = top_;
        top_ = fresh;
        ++size_;
    }

    // 出栈：把栈顶结点摘下来，取走它的值，再释放它。空栈返回空 optional。
    std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        Node* dying = top_;
        T value = dying->value;
        top_ = dying->next;
        delete dying;
        --size_;
        return value;
    }

    std::optional<T> top() const {
        if (empty()) {
            return std::nullopt;
        }
        return top_->value;
    }

    bool empty() const { return top_ == nullptr; }
    size_type size() const { return size_; }

    // 逐个释放结点。**用循环，不要用递归**——链长十万级时递归析构会把运行栈撑爆。
    // 第 5 章有实测数字。
    void clear() {
        while (top_ != nullptr) {
            Node* dying = top_;
            top_ = top_->next;
            delete dying;
        }
        size_ = 0;
    }

private:
    struct Node {
        T value;
        Node* next;
    };

    // 拷贝一串结点：原栈是「顶 → 底」，新栈也要按同样次序串起来，
    // 所以从原栈的顶开始走，每次把新结点接到上一个新结点的后面。
    void copy_from(const LinkedStack& other) {
        Node** tail = &top_;             // 指向「下一个新结点该挂在哪」
        for (Node* source = other.top_; source != nullptr; source = source->next) {
            Node* fresh = new Node;
            fresh->value = source->value;
            fresh->next = nullptr;
            *tail = fresh;
            tail = &fresh->next;
            ++size_;
        }
    }

    Node* top_;           // 栈顶结点；空栈时是 nullptr
    size_type size_;      // 结点个数
};
