// 链式栈 LinkedStack —— 原书【代码3.4】的现代化实现。
//
// 本节要教的是「同一个 ADT 换一种存储结构」：顺序栈用连续数组，链式栈用结点串联。
// 所以结点管理是教学内容，按 D-001 用裸指针加显式五法则，不换成智能指针。
// 接口形状与 ArrayStack 保持一致（D-001 §3b），两者才好拿来对比。
#pragma once

#include <cstddef>
#include <optional>
#include <utility>

namespace dsa {

// >>> class-head
/// 链式栈：结点分散在堆上，压栈只是接一个新结点，不需要连续空间、也不需要扩容。
///
/// 与原书 lnkStack 的差别：`top` 这个名字只留给成员函数（原书 `Link<T>* top`
/// 与 `bool top(T&)` 重名，导致整个类编译不过）；补齐五法则；不做任何 I/O；
/// 出栈返回 `std::optional<T>`。
template <typename T>
class LinkedStack {
    struct Node {
        T value;
        Node* next;

        template <typename U>
        Node(U&& item, Node* successor) : value(std::forward<U>(item)), next(successor) {}
    };

public:
    using value_type = T;
    using size_type = std::size_t;
    // <<< class-head

    /// 原书的构造函数是 `lnkStack(int defSize)`——链式栈**不需要预设容量**，
    /// 那个参数从头到尾没被用过，只是从顺序栈那边照抄过来的。这里去掉。
    LinkedStack() noexcept = default;

    // >>> rule-of-five
    // 原书有 `~lnkStack(){ clear(); }` 却没有拷贝构造与拷贝赋值：
    // 一次 `lnkStack<int> b = a;` 之后两个栈共享同一串结点，各自析构一次 → 二次释放。
    // 与顺序栈、顺序表、链表、字符串是同一个错误，本书第五次遇到它。
    LinkedStack(const LinkedStack& other) {
        // 先按原序收集，再逆序压回，避免递归拷贝（深链会爆栈，见 UNVERIFIED-RISKS.md）
        Node* source = other.top_;
        Node** tail = &top_;
        try {
            while (source != nullptr) {
                *tail = new Node(source->value, nullptr);
                tail = &(*tail)->next;
                ++size_;
                source = source->next;
            }
        } catch (...) {
            clear();  // 半截链必须自己收拾：构造函数抛出时析构函数不会运行
            throw;
        }
    }

    LinkedStack& operator=(const LinkedStack& other) {
        if (this != &other) {
            LinkedStack copy(other);
            swap(copy);
        }
        return *this;
    }

    LinkedStack(LinkedStack&& other) noexcept
        : top_(std::exchange(other.top_, nullptr)), size_(std::exchange(other.size_, 0)) {}

    LinkedStack& operator=(LinkedStack&& other) noexcept {
        if (this != &other) {
            clear();
            top_ = std::exchange(other.top_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    ~LinkedStack() { clear(); }
    // <<< rule-of-five

    void swap(LinkedStack& other) noexcept {
        std::swap(top_, other.top_);
        std::swap(size_, other.size_);
    }

    /// 逐个释放结点。**迭代而非递归**——链长十万级时递归析构会爆栈
    /// （第 5 章有实测数字，见 collab/UNVERIFIED-RISKS.md）。
    void clear() noexcept {
        while (top_ != nullptr) {
            Node* dying = top_;
            top_ = top_->next;
            delete dying;
        }
        size_ = 0;
    }

    // >>> push-pop
    /// 入栈：接一个新结点。**没有"栈满"这回事**——这正是链式栈相对顺序栈的差别，
    /// 原书顺序栈那边要判 `top == mSize - 1` 并打印"栈满溢出"。
    void push(const T& item) { top_ = new Node(item, top_); ++size_; }
    void push(T&& item) { top_ = new Node(std::move(item), top_); ++size_; }

    /// 出栈。空栈返回 std::nullopt（D-001 §3c：空是可预期状态，不抛异常）。
    [[nodiscard]] std::optional<T> pop() {
        if (top_ == nullptr) {
            return std::nullopt;
        }
        Node* dying = top_;
        std::optional<T> result(std::move(dying->value));
        top_ = dying->next;
        delete dying;
        --size_;
        return result;
    }

    /// 取栈顶副本；零拷贝的观望用 peek()（D-001 §3b）。
    [[nodiscard]] std::optional<T> top() const {
        return top_ == nullptr ? std::nullopt : std::optional<T>(top_->value);
    }

    [[nodiscard]] const T* peek() const noexcept {
        return top_ == nullptr ? nullptr : &top_->value;
    }
    // <<< push-pop

    [[nodiscard]] bool empty() const noexcept { return top_ == nullptr; }
    [[nodiscard]] size_type size() const noexcept { return size_; }

private:
    Node* top_{nullptr};
    size_type size_{0};
};

template <typename T>
void swap(LinkedStack<T>& a, LinkedStack<T>& b) noexcept { a.swap(b); }

}  // namespace dsa
