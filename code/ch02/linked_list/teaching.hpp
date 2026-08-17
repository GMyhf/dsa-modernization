// 单链表 LinkedList —— 教学版。原书【代码2.6】【代码2.7】【算法2.8】–【算法2.11】。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
// 保留链表要教的全部内容：结点分散存储、带头结点、尾指针让 append 保持 O(1)、
// 按位置查找必须循链 O(n)、插入删除在定位之后只改常数条链接。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则（析构 + 拷贝构造 + 拷贝赋值），头结点里放一个不用的 T；
//   工程版  补齐移动语义，并把头结点做成不含 T 的哨兵基类，
//           这样 T 就不必可默认构造——代价是多一层继承和一堆 static_cast。
// 两份都在闸门里真编译真运行。先读这一份，2.3a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>

template <typename T>
class LinkedList {
private:
    // 结点(node)：一个数据域，加一根指向后继的链接。原书【代码2.6】。
    // 它是实现细节，所以放在 private 里——调用方拿不到指针，就改不坏链。
    struct Node {
        T value;
        Node* next;
    };

public:
    using value_type = T;
    using size_type = std::size_t;

    // 头结点(head node)是一个**不存放数据**的哨兵结点，永远排在第一个真元素前面。
    // 它的作用是消掉「在表头插入 / 删除表头」这个特例：有了它，任何位置的插入
    // 都变成「找到前驱，改它的 next」，一套代码走遍全表。
    LinkedList() : head_(new Node), tail_(head_), size_(0) {
        head_->next = nullptr;
    }

    ~LinkedList() {
        clear();
        delete head_;      // 头结点是构造时 new 的，最后要还回去
    }

    // 三法则：这个类管着一串 new 出来的结点，拷贝必须自己写。
    LinkedList(const LinkedList& other) : head_(new Node), tail_(head_), size_(0) {
        head_->next = nullptr;
        for (Node* source = other.head_->next; source != nullptr; source = source->next) {
            append(source->value);
        }
    }

    LinkedList& operator=(const LinkedList& other) {
        if (this == &other) {
            return *this;
        }
        clear();
        for (Node* source = other.head_->next; source != nullptr; source = source->next) {
            append(source->value);
        }
        return *this;
    }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }

    // 删掉全部真元素，头结点留着。
    void clear() {
        Node* current = head_->next;
        while (current != nullptr) {
            Node* dying = current;
            current = current->next;
            delete dying;
        }
        head_->next = nullptr;
        tail_ = head_;         // 表空了，尾指针退回头结点
        size_ = 0;
    }

    // 在表尾追加。**因为存了 tail_，这里是 O(1)**；没有它就得每次从头走到尾。
    void append(const T& value) {
        Node* fresh = new Node;
        fresh->value = value;
        fresh->next = nullptr;
        tail_->next = fresh;
        tail_ = fresh;
        ++size_;
    }

    // 在位置 pos 插入，pos 可以等于 size()（追加到表尾）。
    // 两步：① 循链找到 pos 的**前驱**，O(n)；② 改两条链接，O(1)。
    // 这就是链表与顺序表的分工——链表的插入不搬元素，但定位要走。
    void insert(size_type pos, const T& value) {
        Node* predecessor = predecessor_at(pos);
        Node* fresh = new Node;
        fresh->value = value;
        fresh->next = predecessor->next;
        predecessor->next = fresh;
        if (predecessor == tail_) {   // 插在表尾，尾指针要跟上
            tail_ = fresh;
        }
        ++size_;
    }

    // 删除位置 pos 上的元素并返回它。同样是「先定位前驱，再改一条链接」。
    T remove(size_type pos) {
        if (pos >= size_) {
            throw std::out_of_range("LinkedList::remove: 下标越界");
        }
        Node* predecessor = predecessor_at(pos);
        Node* dying = predecessor->next;
        T value = dying->value;
        predecessor->next = dying->next;
        if (dying == tail_) {         // 删的是最后一个，尾指针退回前驱
            tail_ = predecessor;
        }
        delete dying;
        --size_;
        return value;
    }

    // 按位置取值：链表**不是**随机存取的，只能从头一个一个数过去，O(n)。
    // 这一条是 2.4 节拿链表和顺序表对比的关键。
    const T& at(size_type pos) const { return node_at(pos)->value; }
    T& at(size_type pos) { return node_at(pos)->value; }

    // 按内容查找，O(n)。找到返回位置，没找到返回空 optional。
    std::optional<size_type> find(const T& value) const {
        size_type pos = 0;
        for (Node* current = head_->next; current != nullptr; current = current->next) {
            if (current->value == value) {
                return pos;
            }
            ++pos;
        }
        return std::nullopt;
    }

    // 链表没有连续下标，range-for 要靠迭代器。这个迭代器就是一根被包起来的
    // 结点指针：`*it` 取值，`++it` 顺着 next 走一步，走到 nullptr 就是结束。
    class Iterator {
    public:
        explicit Iterator(Node* node) : node_(node) {}
        T& operator*() const { return node_->value; }
        Iterator& operator++() {
            node_ = node_->next;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }

    private:
        Node* node_;
    };

    Iterator begin() { return Iterator(head_->next); }   // 从第一个**真**元素开始
    Iterator end() { return Iterator(nullptr); }

private:
    // 返回位置 pos 的前驱结点。pos == 0 时前驱就是头结点——
    // 这正是头结点存在的意义：表头不再是特例。
    Node* predecessor_at(size_type pos) const {
        if (pos > size_) {
            throw std::out_of_range("LinkedList: 下标越界");
        }
        Node* predecessor = head_;
        for (size_type i = 0; i < pos; ++i) {
            predecessor = predecessor->next;
        }
        return predecessor;
    }

    Node* node_at(size_type pos) const {
        if (pos >= size_) {
            throw std::out_of_range("LinkedList::at: 下标越界");
        }
        Node* current = head_->next;
        for (size_type i = 0; i < pos; ++i) {
            current = current->next;
        }
        return current;
    }

    Node* head_;          // 头结点（哨兵，不存数据）
    Node* tail_;          // 最后一个结点；表空时等于 head_
    size_type size_;
};
