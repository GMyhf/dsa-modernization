// 队列 —— 教学版。原书【代码3.13】【代码3.14】【代码3.15】。
//
// 一个文件、两个类、能直接编译运行：
//   ArrayQueue   顺序队列（循环队列），元素放在一块固定大小的连续数组里；
//   LinkedQueue  链式队列，结点分散在堆上，队尾指针让入队保持 O(1)。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则（析构 + 拷贝构造 + 拷贝赋值），一行一句，正确但拷贝多一点；
//   工程版  在此之上补齐移动语义与 copy-and-swap。
// 两份都在闸门里真编译真运行。先读这一份，3.2.3a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>

// ---------------------------------------------------------------------------
// 顺序队列（循环队列）
//
// 队列两头都要动：入队动队尾，出队动队头。如果队头固定在下标 0，每次出队都要把
// 后面所有元素前移一位，O(n)。所以真正的做法是让队头也往后走——走到数组末尾就
// 绕回下标 0，数组被当成一个圈来用，这就是「循环队列」。
//
// 绕回之后有个麻烦：`front == rear` 既可能是空、也可能是满，分不开。
// 原书的办法是**牺牲一个槽位**：约定「rear 的下一格就是 front」时算满，
// 于是 n 个槽位最多装 n-1 个元素，两种状态就分得开了。本书照办。
// ---------------------------------------------------------------------------
template <typename T>
class ArrayQueue {
public:
    using value_type = T;
    using size_type = std::size_t;

    // capacity 是「最多能装几个元素」，所以内部要多要一个槽位。
    explicit ArrayQueue(size_type capacity)
        : slots_(capacity + 1), data_(new T[capacity + 1]), front_(0), rear_(0) {}

    ~ArrayQueue() { delete[] data_; }

    ArrayQueue(const ArrayQueue& other)
        : slots_(other.slots_), data_(new T[other.slots_]),
          front_(other.front_), rear_(other.rear_) {
        for (size_type i = front_; i != rear_; i = (i + 1) % slots_) {
            data_[i] = other.data_[i];
        }
    }

    ArrayQueue& operator=(const ArrayQueue& other) {
        if (this == &other) {
            return *this;
        }
        T* fresh = new T[other.slots_];
        for (size_type i = other.front_; i != other.rear_; i = (i + 1) % other.slots_) {
            fresh[i] = other.data_[i];
        }
        delete[] data_;
        data_ = fresh;
        slots_ = other.slots_;
        front_ = other.front_;
        rear_ = other.rear_;
        return *this;
    }

    bool empty() const { return front_ == rear_; }

    // 满的判据：rear 再往前走一格就撞上 front。那一格就是被牺牲掉的槽位。
    bool full() const { return (rear_ + 1) % slots_ == front_; }

    size_type size() const {
        return (rear_ >= front_) ? (rear_ - front_) : (slots_ - front_ + rear_);
    }

    // 入队。队满返回 false——顺序队列的容量是固定的，这是它与链式队列的核心差别。
    bool enqueue(const T& value) {
        if (full()) {
            return false;
        }
        data_[rear_] = value;
        rear_ = (rear_ + 1) % slots_;    // 走到末尾就绕回 0，取模不能漏
        return true;
    }

    // 出队。空队列返回空 optional，不是错误，也不打印任何东西。
    std::optional<T> dequeue() {
        if (empty()) {
            return std::nullopt;
        }
        T value = data_[front_];
        front_ = (front_ + 1) % slots_;
        return value;
    }

    // 看队头但不出队。
    std::optional<T> front() const {
        if (empty()) {
            return std::nullopt;
        }
        return data_[front_];
    }

    void clear() { front_ = rear_ = 0; }

private:
    size_type slots_;     // 数组格数 = 容量 + 1（多的那一格用来区分空和满）
    T* data_;
    size_type front_;     // 队头元素的下标
    size_type rear_;      // 下一个入队元素要写的下标
};

// ---------------------------------------------------------------------------
// 链式队列
//
// 结点分散在堆上，**没有「队满」这回事**。
// 除了队头指针，还要一个**队尾指针**：没有它，每次入队都得从队头走到尾，O(n)。
// 有了它，入队和出队都是 O(1)。
// ---------------------------------------------------------------------------
template <typename T>
class LinkedQueue {
public:
    using value_type = T;
    using size_type = std::size_t;

    LinkedQueue() : front_(nullptr), rear_(nullptr), size_(0) {}

    ~LinkedQueue() { clear(); }

    LinkedQueue(const LinkedQueue& other) : front_(nullptr), rear_(nullptr), size_(0) {
        for (Node* source = other.front_; source != nullptr; source = source->next) {
            enqueue(source->value);
        }
    }

    LinkedQueue& operator=(const LinkedQueue& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        for (Node* source = other.front_; source != nullptr; source = source->next) {
            enqueue(source->value);
        }
        return *this;
    }

    // 入队：新结点接到队尾。队列原来是空的话，它同时也是队头。
    void enqueue(const T& value) {
        Node* fresh = new Node;
        fresh->value = value;
        fresh->next = nullptr;
        if (rear_ == nullptr) {
            front_ = rear_ = fresh;
        } else {
            rear_->next = fresh;
            rear_ = fresh;
        }
        ++size_;
    }

    // 出队：摘下队头结点。摘完若队列空了，队尾指针也必须置空，
    // 否则它就成了一根指向已释放内存的野指针。
    std::optional<T> dequeue() {
        if (empty()) {
            return std::nullopt;
        }
        Node* dying = front_;
        T value = dying->value;
        front_ = dying->next;
        if (front_ == nullptr) {
            rear_ = nullptr;
        }
        delete dying;
        --size_;
        return value;
    }

    std::optional<T> front() const {
        if (empty()) {
            return std::nullopt;
        }
        return front_->value;
    }

    bool empty() const { return front_ == nullptr; }
    size_type size() const { return size_; }

    // 用循环释放，不要用递归——长队列的递归析构会把运行栈撑爆。
    void clear() {
        while (front_ != nullptr) {
            Node* dying = front_;
            front_ = dying->next;
            delete dying;
        }
        rear_ = nullptr;
        size_ = 0;
    }

private:
    struct Node {
        T value;
        Node* next;
    };

    Node* front_;
    Node* rear_;          // 少了它，入队就要每次从头走到尾
    size_type size_;
};
