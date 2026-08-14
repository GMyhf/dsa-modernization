// 原书【代码3.13】【代码3.14】【代码3.15】的现代化队列实现。
#pragma once

#include <cstddef>
#include <optional>
#include <utility>

namespace dsa {

// >>> array-queue
template <typename T>
class ArrayQueue {
public:
    explicit ArrayQueue(std::size_t capacity) : slots_(capacity + 1), data_(slots_ ? new T[slots_] : nullptr) {}
    ArrayQueue(const ArrayQueue& other) : slots_(other.slots_), front_(other.front_), rear_(other.rear_), data_(slots_ ? new T[slots_] : nullptr) { for (std::size_t i = front_; i != rear_; i = (i + 1) % slots_) data_[i] = other.data_[i]; }
    ArrayQueue& operator=(const ArrayQueue& other) { if (this != &other) { ArrayQueue copy(other); swap(copy); } return *this; }
    ArrayQueue(ArrayQueue&& other) noexcept { swap(other); }
    ArrayQueue& operator=(ArrayQueue&& other) noexcept { if (this != &other) { ArrayQueue moved(std::move(other)); swap(moved); } return *this; }
    ~ArrayQueue() { delete[] data_; }
    void swap(ArrayQueue& other) noexcept { using std::swap; swap(slots_, other.slots_); swap(front_, other.front_); swap(rear_, other.rear_); swap(data_, other.data_); }
    [[nodiscard]] bool empty() const noexcept { return front_ == rear_; }
    [[nodiscard]] bool full() const noexcept { return slots_ != 0 && (rear_ + 1) % slots_ == front_; }
    [[nodiscard]] std::size_t size() const noexcept { return rear_ >= front_ ? rear_ - front_ : slots_ - front_ + rear_; }
    [[nodiscard]] bool enqueue(const T& value) { if (full()) return false; data_[rear_] = value; rear_ = (rear_ + 1) % slots_; return true; }
    [[nodiscard]] bool enqueue(T&& value) { if (full()) return false; data_[rear_] = std::move(value); rear_ = (rear_ + 1) % slots_; return true; }
    [[nodiscard]] std::optional<T> dequeue() { if (empty()) return std::nullopt; T value = std::move(data_[front_]); front_ = (front_ + 1) % slots_; return value; }
    [[nodiscard]] const T* front() const noexcept { return empty() ? nullptr : &data_[front_]; }
    void clear() noexcept { front_ = rear_ = 0; }
private: std::size_t slots_{0}, front_{0}, rear_{0}; T* data_{nullptr};
};
// <<< array-queue

// >>> linked-queue
template <typename T>
class LinkedQueue {
    struct Node { T value; Node* next{nullptr}; template <typename U> explicit Node(U&& value) : value(std::forward<U>(value)) {} };
public:
    LinkedQueue() = default;
    LinkedQueue(const LinkedQueue& other) { for (Node* n = other.front_; n != nullptr; n = n->next) enqueue(n->value); }
    LinkedQueue& operator=(const LinkedQueue& other) { if (this != &other) { LinkedQueue copy(other); swap(copy); } return *this; }
    LinkedQueue(LinkedQueue&& other) noexcept { swap(other); }
    LinkedQueue& operator=(LinkedQueue&& other) noexcept { if (this != &other) { LinkedQueue moved(std::move(other)); swap(moved); } return *this; }
    ~LinkedQueue() { clear(); }
    void swap(LinkedQueue& other) noexcept { using std::swap; swap(front_, other.front_); swap(rear_, other.rear_); swap(size_, other.size_); }
    [[nodiscard]] bool empty() const noexcept { return front_ == nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void enqueue(const T& value) { append(new Node(value)); }
    void enqueue(T&& value) { append(new Node(std::move(value))); }
    [[nodiscard]] std::optional<T> dequeue() {
        if (front_ == nullptr) return std::nullopt;
        Node* old = front_;
        front_ = old->next;
        if (front_ == nullptr) rear_ = nullptr;
        --size_;
        T value = std::move(old->value);
        delete old;
        return value;
    }
    [[nodiscard]] const T* front() const noexcept { return front_ == nullptr ? nullptr : &front_->value; }
    /// 循环释放而非递归析构，长队列也不会消耗与长度成正比的调用栈。
    void clear() noexcept {
        while (front_ != nullptr) {
            Node* old = front_;
            front_ = old->next;
            delete old;
        }
        rear_ = nullptr;
        size_ = 0;
    }
private:
    void append(Node* node) noexcept {
        if (rear_ == nullptr) front_ = rear_ = node;
        else { rear_->next = node; rear_ = node; }
        ++size_;
    }
    Node* front_{nullptr};
    Node* rear_{nullptr};
    std::size_t size_{0};
};
// <<< linked-queue
}  // namespace dsa
