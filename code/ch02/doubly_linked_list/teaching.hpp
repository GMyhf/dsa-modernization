// 双链表 DoublyLinkedList —— 教学版。原书【代码2.12】的双链结点。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
//
// 双链表比单链表每个结点多存一根 `prev`。多出来的空间买到的是一件事：
// **已知结点位置时，插入和删除都是 O(1)**——不必像单链表那样先循链找前驱。
// 这是本节唯一值得多花那份空间的理由，全部教学内容都围绕它。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则，插入/删除各写成一段直白的改链；
//   工程版  补齐移动语义，并把「表头 / 表中 / 表尾」三种情形合并进一个
//           insert_before(pos)——省代码，但初读时那几个 nullptr 分支很绕。
// 两份都在闸门里真编译真运行。先读这一份，2.3a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <stdexcept>

template <typename T>
class DoublyLinkedList {
private:
    // 双链结点：数据 + 前驱链接 + 后继链接。
    struct Node {
        T value;
        Node* prev;
        Node* next;
    };

public:
    using value_type = T;
    using size_type = std::size_t;

    DoublyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~DoublyLinkedList() { clear(); }

    // 三法则：这个类管着一串 new 出来的结点，拷贝必须自己写。
    DoublyLinkedList(const DoublyLinkedList& other)
        : head_(nullptr), tail_(nullptr), size_(0) {
        for (Node* source = other.head_; source != nullptr; source = source->next) {
            push_back(source->value);
        }
    }

    DoublyLinkedList& operator=(const DoublyLinkedList& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        for (Node* source = other.head_; source != nullptr; source = source->next) {
            push_back(source->value);
        }
        return *this;
    }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }

    void clear() {
        Node* current = head_;
        while (current != nullptr) {
            Node* dying = current;
            current = current->next;
            delete dying;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    // 表头插入，O(1)。要改的链接一共三处：新结点的 next、原表头的 prev、head_。
    void push_front(const T& value) {
        Node* fresh = new Node;
        fresh->value = value;
        fresh->prev = nullptr;
        fresh->next = head_;
        if (head_ != nullptr) {
            head_->prev = fresh;
        } else {
            tail_ = fresh;      // 原来是空表，新结点同时也是表尾
        }
        head_ = fresh;
        ++size_;
    }

    // 表尾插入，O(1)。与 push_front 完全对称。
    void push_back(const T& value) {
        Node* fresh = new Node;
        fresh->value = value;
        fresh->prev = tail_;
        fresh->next = nullptr;
        if (tail_ != nullptr) {
            tail_->next = fresh;
        } else {
            head_ = fresh;      // 原来是空表，新结点同时也是表头
        }
        tail_ = fresh;
        ++size_;
    }

    T pop_front() {
        if (empty()) {
            throw std::out_of_range("DoublyLinkedList::pop_front: 表空");
        }
        return erase_node(head_);
    }

    T pop_back() {
        if (empty()) {
            throw std::out_of_range("DoublyLinkedList::pop_back: 表空");
        }
        return erase_node(tail_);
    }

    // 在位置 pos 插入。定位仍是 O(n)——双链表省掉的是「找前驱」，不是「找位置」。
    void insert(size_type pos, const T& value) {
        if (pos > size_) {
            throw std::out_of_range("DoublyLinkedList::insert: 位置非法");
        }
        if (pos == 0) {
            push_front(value);
            return;
        }
        if (pos == size_) {
            push_back(value);
            return;
        }
        Node* successor = node_at(pos);          // 新结点要插在它前面
        Node* predecessor = successor->prev;     // O(1)：prev 现成的，不用循链
        Node* fresh = new Node;
        fresh->value = value;
        fresh->prev = predecessor;
        fresh->next = successor;
        predecessor->next = fresh;
        successor->prev = fresh;
        ++size_;
    }

    T erase(size_type pos) { return erase_node(node_at(pos)); }

    const T& at(size_type pos) const { return node_at(pos)->value; }
    T& at(size_type pos) { return node_at(pos)->value; }

    // 迭代器：一根被包起来的结点指针。双链表的迭代器可以 `--`，单链表的不行——
    // 这也是那根 prev 买到的东西。
    class Iterator {
    public:
        explicit Iterator(Node* node) : node_(node) {}
        T& operator*() const { return node_->value; }
        Iterator& operator++() {
            node_ = node_->next;
            return *this;
        }
        Iterator& operator--() {
            node_ = node_->prev;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

    private:
        Node* node_;
    };

    Iterator begin() { return Iterator(head_); }
    Iterator end() { return Iterator(nullptr); }

private:
    // 摘掉一个已知的结点，O(1)。**这是双链表的看家本领**：
    // 单链表要做同一件事，得先从头走到它的前驱，O(n)。
    T erase_node(Node* node) {
        T value = node->value;
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            head_ = node->next;     // 删的是表头
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        } else {
            tail_ = node->prev;     // 删的是表尾
        }
        delete node;
        --size_;
        return value;
    }

    Node* node_at(size_type pos) const {
        if (pos >= size_) {
            throw std::out_of_range("DoublyLinkedList: 下标越界");
        }
        Node* current = head_;
        for (size_type i = 0; i < pos; ++i) {
            current = current->next;
        }
        return current;
    }

    Node* head_;
    Node* tail_;
    size_type size_;
};
