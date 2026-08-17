// 顺序表 ArrayList —— 教学版。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
// 它保留原书【代码2.1】【代码2.2】【算法2.3】【算法2.4】【算法2.5】要教的全部内容——
// 连续存储、按下标 O(1) 随机存取、插入/删除要搬 O(n) 个元素——
// 只把原书那几处编译不过或会崩的写法换掉。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  遵守**三法则**（析构 + 拷贝构造 + 拷贝赋值），正确，但拷贝多一点；
//   工程版  在此之上补齐移动语义、强异常保证、编译期类型约束。
// 两份都在闸门里真编译真运行。先读这一份，2.2a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>

template <typename T>
class ArrayList {
public:
    using value_type = T;
    using size_type = std::size_t;

    explicit ArrayList(size_type initial_capacity = 8)
        : data_(new T[initial_capacity]), capacity_(initial_capacity), size_(0) {}

    ~ArrayList() { delete[] data_; }

    // 三法则：自己管着 new 出来的数组，就得自己写拷贝构造和拷贝赋值。
    // 不写的话编译器会照抄指针，两个表指向同一块内存，各析构一次 → 二次释放。
    ArrayList(const ArrayList& other)
        : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    ArrayList& operator=(const ArrayList& other) {
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
    size_type capacity() const { return capacity_; }

    // 清空只把长度归零，已经申请的数组留着复用。
    void clear() { size_ = 0; }

    // 按下标读取，O(1)——这是顺序表相对链表的看家本领。
    // 下标非法是**调用方的错误**，不是可预期状态，所以抛异常而不是返回 optional。
    const T& at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("ArrayList::at: 下标越界");
        }
        return data_[index];
    }

    T& at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("ArrayList::at: 下标越界");
        }
        return data_[index];
    }

    void set(size_type index, const T& value) {
        if (index >= size_) {
            throw std::out_of_range("ArrayList::set: 下标越界");
        }
        data_[index] = value;
    }

    // 按内容查找，O(n)。找到返回下标，没找到返回空 optional。
    // 原书【算法2.3】用 `bool getPos(int& p, const T value)`：忘了看返回值，
    // 就会读到一个从没被写过的 p。这里「找没找到」是返回值类型的一部分。
    std::optional<size_type> find(const T& value) const {
        for (size_type i = 0; i < size_; ++i) {
            if (data_[i] == value) {
                return i;
            }
        }
        return std::nullopt;
    }

    // 在位置 pos 插入，pos 可以等于 size()（追加到表尾）。
    // 代价 O(n)：pos 之后的元素都要右移一位。这正是顺序表与链表要对比的地方。
    void insert(size_type pos, const T& value) {
        if (pos > size_) {
            throw std::out_of_range("ArrayList::insert: 插入位置非法");
        }
        if (size_ == capacity_) {
            grow();
        }
        for (size_type i = size_; i > pos; --i) {
            data_[i] = data_[i - 1];   // 从后往前搬，否则会自己覆盖自己
        }
        data_[pos] = value;
        ++size_;
    }

    void append(const T& value) { insert(size_, value); }

    // 删除 pos 上的元素并返回它。代价同样是 O(n)：后面的元素都要左移一位。
    T remove(size_type pos) {
        if (pos >= size_) {
            throw std::out_of_range("ArrayList::remove: 下标越界");
        }
        T removed = data_[pos];
        for (size_type i = pos; i + 1 < size_; ++i) {
            data_[i] = data_[i + 1];
        }
        --size_;
        return removed;
    }

    // 有了 begin/end，range-for 就能用了：for (auto& x : list) { ... }
    //
    // 原书是在类里放一个 `int position` 游标，配 setPos/next/prev 来依次处理元素。
    // 那种设计把「遍历到哪了」这个状态塞进了容器：const 对象没法遍历，
    // 两处代码不能同时遍历，嵌套遍历直接互相踩。游标挪到容器外面，这些问题一起消失。
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    // 扩容：申请两倍大的新数组，搬过去，再释放旧的。
    // 翻倍而不是加一，才能让 append 的摊还代价保持 O(1)。
    void grow() {
        size_type next = (capacity_ == 0) ? 1 : capacity_ * 2;
        T* fresh = new T[next];
        for (size_type i = 0; i < size_; ++i) {
            fresh[i] = data_[i];
        }
        delete[] data_;       // 先搬完再释放旧的，顺序反了就会读到已释放的内存
        data_ = fresh;
        capacity_ = next;
    }

    T* data_;             // 指向底层数组
    size_type capacity_;  // 数组能放多少个
    size_type size_;      // 现在放了几个
};
