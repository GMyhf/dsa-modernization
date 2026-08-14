// 顺序表 ArrayList —— 原书【代码2.1】【代码2.2】【算法2.3】【算法2.4】【算法2.5】的现代化实现。
//
// 遵循 collab/DECISION_LOG.md 的 D-001 公约：C++17；不拿 std::vector 替换；
// 存储结构是本节教学内容，所以用裸 T* 加显式五法则；容器内不做任何 I/O；
// 越界抛标准异常；搬迁判据按 D-005（移动赋值是否 noexcept）。
//
// 保留原书要教的东西：连续存储、按下标 O(1) 随机存取、插入/删除要搬 O(n) 个元素。
// 这些正是顺序表与链表对比的全部依据，一个都没动。
#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dsa {

// >>> class-head
/// 顺序表（按顺序方式存储的线性表，又称向量）。
///
/// 与原书 arrList 的差别：容量不足时自动翻倍，而不是打印 "The list is overflow"
/// 然后返回 false；位置非法抛 std::out_of_range，而不是打印一行再返回 false；
/// 查找返回 std::optional<size_type>，而不是「出参 + bool」双通道。
template <typename T>
class ArrayList {
public:
    using value_type = T;
    using size_type = std::size_t;
    // <<< class-head

    static_assert(std::is_default_constructible<T>::value,
                  "ArrayList<T>: T 必须可默认构造（底层 new T[n] 会构造整块槽位）");
    static_assert(std::is_move_assignable<T>::value,
                  "ArrayList<T>: T 必须可移动赋值（插入/删除要搬动元素）");
    static_assert(std::is_copy_assignable<T>::value || std::is_nothrow_move_assignable<T>::value,
                  "ArrayList<T>: 不可复制的 T 必须可无异常移动赋值（扩容保持强异常保证）");
    static_assert(!std::is_reference<T>::value, "ArrayList<T>: T 不能是引用类型");

    ArrayList() noexcept = default;

    explicit ArrayList(size_type capacity)
        : capacity_(capacity), data_(capacity ? new T[capacity] : nullptr) {}

    ArrayList(const ArrayList& other)
        : capacity_(other.capacity_),
          size_(other.size_),
          data_(other.capacity_ ? new T[other.capacity_] : nullptr) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    ArrayList& operator=(const ArrayList& other) {
        if (this != &other) {
            ArrayList copy(other);
            swap(copy);
        }
        return *this;
    }

    ArrayList(ArrayList&& other) noexcept { swap(other); }

    ArrayList& operator=(ArrayList&& other) noexcept {
        if (this != &other) {
            ArrayList moved(std::move(other));
            swap(moved);
        }
        return *this;
    }

    ~ArrayList() { delete[] data_; }

    void swap(ArrayList& other) noexcept {
        using std::swap;
        swap(capacity_, other.capacity_);
        swap(size_, other.size_);
        swap(data_, other.data_);
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

    /// 清空。只把逻辑长度归零，保留已分配容量。
    /// 原书的 clear() 会 `delete[]` 之后重新 `new` 一整块——既没必要，
    /// 而且一旦 new 抛异常，对象就停在「指针已释放、长度已归零」的破碎状态。
    void clear() noexcept { size_ = 0; }

    // >>> access
    /// 按下标读取，O(1)。越界抛 std::out_of_range。
    /// 原书 getValue 用「出参 + bool」，越界时打印一行再返回 false。
    [[nodiscard]]
    const T& at(size_type index) const {
        check_index(index, "ArrayList::at");
        return data_[index];
    }

    T& at(size_type index) {
        check_index(index, "ArrayList::at");
        return data_[index];
    }

    /// 修改指定位置的值。越界抛 std::out_of_range。
    void set(size_type index, const T& value) {
        check_index(index, "ArrayList::set");
        data_[index] = value;
    }
    // <<< access

    // >>> find
    /// 按内容查找，O(n)。找到返回下标，没找到返回 std::nullopt。
    ///
    /// 不用原书【算法2.3】的 `bool getPos(int& p, const T value)`：那种写法下
    /// 「没找到」只是一个可以被忽略的 bool，忽略了就会读到从没被写过的 p。
    /// 这里「找没找到」是返回值类型的一部分，取值必须先判断；
    /// 加上 [[nodiscard]]，连丢弃返回值都编译不过。
    [[nodiscard]]
    std::optional<size_type> find(const T& value) const {
        for (size_type i = 0; i < size_; ++i) {
            if (data_[i] == value) {
                return i;
            }
        }
        return std::nullopt;
    }
    // <<< find

    // >>> insert
    /// 在位置 pos 插入元素，pos 可以等于 size()（即追加到表尾）。
    /// 位置非法抛 std::out_of_range；容量不足自动翻倍。
    ///
    /// 时间代价仍是 O(n)——pos 之后的元素都要右移一位。这是顺序表的固有代价，
    /// 也是第 2.3 节要拿它和链表对比的地方，没有被"优化"掉。
    void insert(size_type pos, const T& value) {
        make_gap(pos);
        data_[pos] = value;
        ++size_;
    }

    void insert(size_type pos, T&& value) {
        make_gap(pos);
        data_[pos] = std::move(value);
        ++size_;
    }

    void append(const T& value) { insert(size_, value); }
    void append(T&& value) { insert(size_, std::move(value)); }
    // <<< insert

    // >>> remove
    /// 删除位置 pos 上的元素并返回它。位置非法抛 std::out_of_range。
    ///
    /// 空表上删除必然越界，所以不需要原书那句单独的空表检查——
    /// 「表空」在这里不是一种可预期状态，而就是下标非法的一个特例。
    T remove(size_type pos) {
        check_index(pos, "ArrayList::remove");
        T removed = std::move(data_[pos]);
        for (size_type i = pos; i + 1 < size_; ++i) {
            data_[i] = std::move(data_[i + 1]);  // 左移一位，O(n)
        }
        --size_;
        return removed;
    }
    // <<< remove

    /// 用裸指针当迭代器，支持 range-for。
    ///
    /// 原书在类里放了一个 `int position` 游标，配 setPos/next/prev 来「依次处理元素」。
    /// 那种设计让遍历状态住进了容器：const 对象没法遍历，两处代码不能同时遍历，
    /// 嵌套遍历直接互相踩。把游标移到容器外面，这些问题一起消失。
    [[nodiscard]] T* begin() noexcept { return data_; }
    [[nodiscard]] T* end() noexcept { return data_ + size_; }
    [[nodiscard]] const T* begin() const noexcept { return data_; }
    [[nodiscard]] const T* end() const noexcept { return data_ + size_; }

private:
    static constexpr size_type kInitialCapacity = 4;

    void check_index(size_type index, const char* who) const {
        if (index >= size_) {
            throw std::out_of_range(who);
        }
    }

    /// 在 pos 处腾出一个空位：先保证容量，再把 [pos, size_) 整体右移一位。
    void make_gap(size_type pos) {
        if (pos > size_) {  // 等于 size_ 是合法的（追加到表尾）
            throw std::out_of_range("ArrayList::insert: 插入位置非法");
        }
        ensure_capacity();
        for (size_type i = size_; i > pos; --i) {
            data_[i] = std::move(data_[i - 1]);
        }
    }

    // >>> grow
    void ensure_capacity() {
        if (size_ < capacity_) {
            return;
        }
        constexpr size_type kMax = std::numeric_limits<size_type>::max();
        if (capacity_ > kMax / 2) {
            throw std::overflow_error("ArrayList: 容量翻倍会溢出");
        }
        const size_type next = capacity_ == 0 ? kInitialCapacity : capacity_ * 2;
        T* fresh = new T[next];
        try {
            for (size_type i = 0; i < size_; ++i) {
                // 判据同第 3 章（DECISION_LOG D-005）：看的是**移动赋值**抛不抛，
                // 不是 std::move_if_noexcept 检查的移动构造。两者可以不同，
                // 用错会让搬到一半的失败把原表掏空。
                if constexpr (std::is_nothrow_move_assignable<T>::value) {
                    fresh[i] = std::move(data_[i]);
                } else {
                    fresh[i] = data_[i];
                }
            }
        } catch (...) {
            delete[] fresh;
            throw;
        }
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }
    // <<< grow

    size_type capacity_{0};
    size_type size_{0};
    T* data_{nullptr};
};

template <typename T>
void swap(ArrayList<T>& a, ArrayList<T>& b) noexcept {
    a.swap(b);
}

}  // namespace dsa
