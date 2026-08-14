#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace dsa {

template <typename T>
class DoublyLinkedList {
    struct Node {
        T value;
        Node* prev{nullptr};
        Node* next{nullptr};
        template <typename U>
        explicit Node(U&& item) : value(std::forward<U>(item)) {}
    };

public:
    DoublyLinkedList() = default;
    DoublyLinkedList(const DoublyLinkedList& other) { for (const T& item : other) push_back(item); }
    DoublyLinkedList& operator=(const DoublyLinkedList& other) { if (this != &other) { DoublyLinkedList copy(other); swap(copy); } return *this; }
    DoublyLinkedList(DoublyLinkedList&& other) noexcept { take(other); }
    DoublyLinkedList& operator=(DoublyLinkedList&& other) noexcept { if (this != &other) { clear(); take(other); } return *this; }
    ~DoublyLinkedList() { clear(); }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void clear() noexcept { while (head_ != nullptr) { Node* next = head_->next; delete head_; head_ = next; } tail_ = nullptr; size_ = 0; }

    void push_front(const T& value) { insert_before(head_, value); }
    void push_front(T&& value) { insert_before(head_, std::move(value)); }
    void push_back(const T& value) { insert_before(nullptr, value); }
    void push_back(T&& value) { insert_before(nullptr, std::move(value)); }

    T pop_front() { return erase_node(head_); }
    T pop_back() { return erase_node(tail_); }
    void insert(std::size_t pos, const T& value) { if (pos > size_) throw std::out_of_range("DoublyLinkedList: index"); insert_before(pos == size_ ? nullptr : node_at(pos), value); }
    void insert(std::size_t pos, T&& value) { if (pos > size_) throw std::out_of_range("DoublyLinkedList: index"); insert_before(pos == size_ ? nullptr : node_at(pos), std::move(value)); }
    T erase(std::size_t pos) { return erase_node(node_at(pos)); }
    T& at(std::size_t pos) { return node_at(pos)->value; }
    const T& at(std::size_t pos) const { return node_at(pos)->value; }

    class iterator {
        Node* node_;
        explicit iterator(Node* node) : node_(node) {}
        friend class DoublyLinkedList;
    public:
        T& operator*() const { return node_->value; }
        iterator& operator++() { node_ = node_->next; return *this; }
        iterator& operator--() { node_ = node_->prev; return *this; }
        bool operator!=(const iterator& other) const noexcept { return node_ != other.node_; }
    };
    class const_iterator {
        const Node* node_;
        explicit const_iterator(const Node* node) : node_(node) {}
        friend class DoublyLinkedList;
    public:
        const T& operator*() const { return node_->value; }
        const_iterator& operator++() { node_ = node_->next; return *this; }
        bool operator!=(const const_iterator& other) const noexcept { return node_ != other.node_; }
    };
    iterator begin() noexcept { return iterator(head_); }
    iterator end() noexcept { return iterator(nullptr); }
    const_iterator begin() const noexcept { return const_iterator(head_); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }

    /// 已知结点位置时插入与删除都是 **O(1)**——不必像单链表那样先循链找前驱。
    /// 这是双链表多存一个 `prev` 指针换来的东西，也是本节唯一值得多花空间的理由。
    iterator insert(iterator pos, const T& value) { return iterator(insert_before(pos.node_, value)); }
    iterator insert(iterator pos, T&& value) { return iterator(insert_before(pos.node_, std::move(value))); }
    T erase(iterator pos) { return erase_node(pos.node_); }

    /// 交换两张表。只换三个成员即可——结点之间的链接一根都不用动，
    /// 因为每张表内部本来就是良构的：`head_->prev` 与 `tail_->next` 已经是 `nullptr`。
    void swap(DoublyLinkedList& other) noexcept { using std::swap; swap(head_, other.head_); swap(tail_, other.tail_); swap(size_, other.size_); }

private:
    Node* head_{nullptr}; Node* tail_{nullptr}; std::size_t size_{0};
    Node* node_at(std::size_t pos) const { if (pos >= size_) throw std::out_of_range("DoublyLinkedList: index"); Node* node = head_; for (std::size_t i=0; i<pos; ++i) node=node->next; return node; }
    // >>> algorithm-2-12-insert
    template <typename U>
    Node* insert_before(Node* pos, U&& value) {
        // 先构造结点；构造失败时原链完全未改变。
        Node* inserted = new Node(std::forward<U>(value));
        inserted->next = pos;
        inserted->prev = pos != nullptr ? pos->prev : tail_;
        if (inserted->prev != nullptr) {
            inserted->prev->next = inserted;
        } else {
            head_ = inserted;
        }
        if (pos != nullptr) {
            pos->prev = inserted;
        } else {
            tail_ = inserted;
        }
        ++size_;
        return inserted;
    }
    // <<< algorithm-2-12-insert
    // >>> algorithm-2-12-erase
    T erase_node(Node* node) {
        if (node == nullptr) throw std::out_of_range("DoublyLinkedList: empty");
        T value = std::move(node->value);
        if (node->prev != nullptr) node->prev->next = node->next;
        else head_ = node->next;
        if (node->next != nullptr) node->next->prev = node->prev;
        else tail_ = node->prev;
        delete node;
        --size_;
        return value;
    }
    // <<< algorithm-2-12-erase
    void take(DoublyLinkedList& other) noexcept { head_=other.head_; tail_=other.tail_; size_=other.size_; other.head_=other.tail_=nullptr; other.size_=0; }
};

}  // namespace dsa
