// 顺序栈 ArrayStack —— 教学版。
//
// 这一份是给「第一次读这一节」的人看的：一个文件、一个类、能直接编译运行。
// 它保留原书【代码3.2】【算法3.3】要教的全部内容——连续数组、栈顶在表尾、
// 满了就把容量翻倍——只把原书那几处会崩的写法换掉。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  遵守**三法则**（析构 + 拷贝构造 + 拷贝赋值），正确，但拷贝多一点；
//   工程版  在此之上补齐移动构造/移动赋值、强异常保证、编译期类型约束。
// 两份都在闸门里真编译真运行。先读这一份，3.1.2a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>

template <typename T>
class ArrayStack {
public:
    using value_type = T;
    using size_type = std::size_t;

    // 构造：先要一小块数组。容量不够时会自动翻倍，所以初值给多少都不影响正确性。
    explicit ArrayStack(size_type initial_capacity = 8)
        : data_(new T[initial_capacity]), capacity_(initial_capacity), size_(0) {}

    // 析构：数组是 new[] 来的，就得 delete[] 回去。
    ~ArrayStack() { delete[] data_; }

    // 拷贝构造：**必须自己写**。
    // 不写的话编译器生成的版本会把 data_ 这根指针照抄一份，于是两个栈指向同一块
    // 内存，各析构一次 —— 同一块内存被释放两次。原书 arrStack 正是漏了这个。
    ArrayStack(const ArrayStack& other)
        : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // 拷贝赋值：同理。注意三件事的顺序——先把新数组备好，再释放旧的，最后接管。
    ArrayStack& operator=(const ArrayStack& other) {
        if (this == &other) {   // 自己赋值给自己，什么都不用做
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

    // 入栈。满了就翻倍，所以不会有「栈满溢出」这回事。
    void push(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_] = value;
        ++size_;
    }

    // 出栈并把元素带回来。空栈返回空的 optional，不是错误，也不打印任何东西。
    std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        --size_;
        return data_[size_];
    }

    // 只看栈顶，不弹出。空栈同样返回空 optional。
    std::optional<T> top() const {
        if (empty()) {
            return std::nullopt;
        }
        return data_[size_ - 1];
    }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

    // 清空：把长度归零就行，已经申请的数组留着接着用。
    void clear() { size_ = 0; }

private:
    // 扩容：申请一块两倍大的，把老元素搬过去，再把老的还回去。
    // 每个元素在均摊意义下只被搬运常数次，所以 push 的摊还代价仍是 O(1)。
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
    size_type size_;      // 现在放了几个，同时也是下一个空位的下标
};
