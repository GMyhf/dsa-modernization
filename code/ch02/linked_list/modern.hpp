// 链表 LinkedList —— 原书【代码2.6】【代码2.7】【算法2.8】至【算法2.11】【代码2.12】的现代化实现。
//
// 保留链表要教的东西：结点分散存储；带头结点；tail_ 使 append 为 O(1)；
// 按位置查找仍须循链 O(n)，插入/删除在定位后只改常数条链接。现代化的是
// 所有权、五法则、异常处理与接口形状；容器内部不做 I/O。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dsa {

// >>> node-types
/// 原书【代码2.6】的单链结点：数据域与指向后继的链接域。
///
/// 实际容器把它藏在私有实现里，避免调用方任意改坏链；这里保留独立类型，
/// 因为后续栈、队列可复用同一种结点形状。
template <typename T>
struct SinglyLink {
    T data;
    SinglyLink* next{nullptr};

    template <typename U>
    explicit SinglyLink(U&& value, SinglyLink* successor = nullptr)
        : data(std::forward<U>(value)), next(successor) {}
};

/// 原书【代码2.12】的双链结点：额外保存前驱链接。
template <typename T>
struct DoublyLink {
    T data;
    DoublyLink* next{nullptr};
    DoublyLink* prev{nullptr};

    template <typename U>
    explicit DoublyLink(U&& value, DoublyLink* predecessor = nullptr, DoublyLink* successor = nullptr)
        : data(std::forward<U>(value)), next(successor), prev(predecessor) {}
};
// <<< node-types

// >>> class-head
/// 带头结点、尾指针的单链表。
///
/// 原书的 `setPos(-1)` 返回头结点；本实现把这个实现细节留在 predecessor_at，
/// 对外位置统一为 [0, size()]。按值查找返回 optional，位置错误抛 out_of_range。
template <typename T>
class LinkedList {
    struct NodeBase {
        NodeBase* next{nullptr};
    };
    struct Node final : NodeBase {
        T value;

        template <typename U>
        explicit Node(U&& item, NodeBase* successor = nullptr)
            : NodeBase{successor}, value(std::forward<U>(item)) {}
    };

public:
    using value_type = T;
    using size_type = std::size_t;
// <<< class-head

    LinkedList() noexcept = default;

    LinkedList(const LinkedList& other) {
        try {
            for (const T& value : other) {
                append(value);
            }
        } catch (...) {
            // 构造函数抛出时析构函数不会运行；已接入的结点须在这里自行回收。
            clear();
            throw;
        }
    }

    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            LinkedList copy(other);
            swap(copy);
        }
        return *this;
    }

    LinkedList(LinkedList&& other) noexcept { take_from(other); }

    LinkedList& operator=(LinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            take_from(other);
        }
        return *this;
    }

    ~LinkedList() { clear(); }

    void swap(LinkedList& other) noexcept {
        using std::swap;
        swap(head_.next, other.head_.next);
        swap(tail_, other.tail_);
        swap(size_, other.size_);
        fix_sentinel_tail(other);
        other.fix_sentinel_tail(*this);
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return size_; }

    /// 清除实际结点，保留嵌入对象内的头结点。不会分配，因此 noexcept。
// >>> clear
    void clear() noexcept {
        NodeBase* current = head_.next;
        while (current != nullptr) {
            NodeBase* following = current->next;
            delete static_cast<Node*>(current);
            current = following;
        }
        head_.next = nullptr;
        tail_ = &head_;
        size_ = 0;
    }
// <<< clear

    // >>> access
    [[nodiscard]] const T& at(size_type pos) const {
        return node_at_const(pos)->value;
    }

    [[nodiscard]] T& at(size_type pos) {
        return node_at(pos)->value;
    }
    // <<< access

    // >>> find
    [[nodiscard]] std::optional<size_type> find(const T& value) const {
        size_type pos = 0;
        for (const T& current : *this) {
            if (current == value) {
                return pos;
            }
            ++pos;
        }
        return std::nullopt;
    }
    // <<< find

    // >>> insert
    /// 尾插直接经 tail_ 接链，O(1)。不能转调 insert(size_)，后者必须循链定位前驱。
    void append(const T& value) { append_impl(value); }
    void append(T&& value) { append_impl(std::move(value)); }

    void insert(size_type pos, const T& value) { insert_impl(pos, value); }
    void insert(size_type pos, T&& value) { insert_impl(pos, std::move(value)); }
    // <<< insert

    // >>> remove
    T remove(size_type pos) {
        NodeBase* predecessor = predecessor_at(pos);
        NodeBase* removed = predecessor->next;
        if (removed == nullptr) {
            throw std::out_of_range("LinkedList::remove: 下标越界");
        }
        // 先移动出值；若 T 的移动构造抛，链接尚未改变，容器仍完整。
        T value = std::move(static_cast<Node*>(removed)->value);
        predecessor->next = removed->next;
        if (removed == tail_) {
            tail_ = predecessor;
        }
        delete static_cast<Node*>(removed);
        --size_;
        return value;
    }
    // <<< remove

    class iterator {
    public:
        T& operator*() const { return static_cast<Node*>(current_)->value; }
        iterator& operator++() {
            current_ = current_->next;
            return *this;
        }
        bool operator!=(const iterator& other) const noexcept { return current_ != other.current_; }

    private:
        explicit iterator(NodeBase* current) : current_(current) {}
        NodeBase* current_;
        friend class LinkedList;
    };

    class const_iterator {
    public:
        const T& operator*() const { return static_cast<const Node*>(current_)->value; }
        const_iterator& operator++() {
            current_ = current_->next;
            return *this;
        }
        bool operator!=(const const_iterator& other) const noexcept { return current_ != other.current_; }

    private:
        explicit const_iterator(const NodeBase* current) : current_(current) {}
        const NodeBase* current_;
        friend class LinkedList;
    };

    [[nodiscard]] iterator begin() noexcept { return iterator(head_.next); }
    [[nodiscard]] iterator end() noexcept { return iterator(nullptr); }
    [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(head_.next); }
    [[nodiscard]] const_iterator end() const noexcept { return const_iterator(nullptr); }

private:
    [[nodiscard]] NodeBase* predecessor_at(size_type pos) {
        if (pos > size_) {
            throw std::out_of_range("LinkedList: 下标越界");
        }
        NodeBase* predecessor = &head_;  // 相当于原书 setPos(-1)
        for (size_type i = 0; i < pos; ++i) {
            predecessor = predecessor->next;
        }
        return predecessor;
    }

    [[nodiscard]] Node* node_at(size_type pos) {
        if (pos >= size_) {
            throw std::out_of_range("LinkedList::at: 下标越界");
        }
        NodeBase* current = head_.next;
        for (size_type i = 0; i < pos; ++i) {
            current = current->next;
        }
        return static_cast<Node*>(current);
    }

    [[nodiscard]] const Node* node_at_const(size_type pos) const {
        if (pos >= size_) {
            throw std::out_of_range("LinkedList::at: 下标越界");
        }
        const NodeBase* current = head_.next;
        for (size_type i = 0; i < pos; ++i) {
            current = current->next;
        }
        return static_cast<const Node*>(current);
    }

    template <typename U>
    void insert_impl(size_type pos, U&& value) {
        NodeBase* predecessor = predecessor_at(pos);
        // 先建结点再接链。分配或 T 构造抛异常时，任何链接和 size_ 都没有改变。
        Node* inserted = new Node(std::forward<U>(value), predecessor->next);
        predecessor->next = inserted;
        if (predecessor == tail_) {
            tail_ = inserted;
        }
        ++size_;
    }

    template <typename U>
    void append_impl(U&& value) {
        // 先建结点，成功后才写 tail_->next；分配或元素构造抛异常时原链不动。
        Node* appended = new Node(std::forward<U>(value));
        tail_->next = appended;
        tail_ = appended;
        ++size_;
    }

    void take_from(LinkedList& other) noexcept {
        head_.next = other.head_.next;
        tail_ = other.empty() ? &head_ : other.tail_;
        size_ = other.size_;
        other.head_.next = nullptr;
        other.tail_ = &other.head_;
        other.size_ = 0;
    }

    void fix_sentinel_tail(LinkedList& other) noexcept {
        if (tail_ == &other.head_) {
            tail_ = &head_;
        }
    }

    NodeBase head_{};  // 头结点，不承载 T，避免强加 T 必须默认构造的约束
    NodeBase* tail_{&head_};
    size_type size_{0};
};

template <typename T>
void swap(LinkedList<T>& left, LinkedList<T>& right) noexcept {
    left.swap(right);
}

}  // namespace dsa
