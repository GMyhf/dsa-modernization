// 顺序栈 ArrayStack —— 原书【代码3.1】【代码3.2】【算法3.3】的现代化实现。
//
// 遵循 collab/DECISION_LOG.md 的 D-001 风格公约：C++17；不拿 std::stack 替换；
// 存储结构是本节教学内容，所以用裸 T* 加**显式五法则**；容器内不做任何 I/O；
// 空状态返回 std::optional，越界/溢出抛标准异常。
//
// 保留原书要教的东西：这仍然是一个手写的、基于数组的栈，扩容仍然是算法3.3 的
// 「满了就翻倍」策略。修掉的是工程上的错（逐条见 legacy.md）。
#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dsa {

// >>> class-head
/// 基于数组的栈（后进先出）。
///
/// 与原书 arrStack 的差别：容量耗尽时自动翻倍（原书要么溢出报错，要么靠算法3.3
/// 手工换成扩容版本）；不打印任何东西；空栈上的 pop/top 返回空 optional，
/// 而不是靠「出参 + bool」双通道返回。
template <typename T>
class ArrayStack {
    // 原书用一个成员函数既非纯虚、析构又非 virtual 的空基类 Stack<T> 来表达「抽象」。
    // 那样的基类给不了多态，还带来「通过基类指针删除派生对象」的未定义行为。
    // C++17 里表达「T 要满足什么」的直接工具是 static_assert + 类型特征：
    // 编译期检查、不付虚表代价，而且错误信息就停在实例化处。
    static_assert(std::is_default_constructible<T>::value,
                  "ArrayStack<T>: T 必须可默认构造（底层 new T[n] 会构造整块槽位）");
    static_assert(std::is_move_assignable<T>::value,
                  "ArrayStack<T>: T 必须可移动赋值（push/pop 需要移动元素）");
    static_assert(std::is_copy_assignable<T>::value || std::is_nothrow_move_assignable<T>::value,
                  "ArrayStack<T>: 不可复制的 T 必须可无异常移动赋值（扩容保持强异常保证）");
    static_assert(!std::is_reference<T>::value, "ArrayStack<T>: T 不能是引用类型");

public:
    using value_type = T;
    using size_type = std::size_t;
    // <<< class-head

    /// 默认构造出一个容量为 0 的空栈——**可用、可析构**。
    /// 原书的无参构造只写了 top = -1，mSize 与 st 都没初始化，
    /// 析构时 delete[] 一个不确定指针（legacy.md 缺陷 3）。
    ArrayStack() noexcept = default;

    explicit ArrayStack(size_type capacity)
        : capacity_(capacity), data_(capacity ? new T[capacity] : nullptr) {}

    // >>> rule-of-five
    // 三/五法则：原书只写了析构函数，没有拷贝构造与拷贝赋值，
    // 于是 `arrStack<int> b = a;` 之后两个对象持有同一根指针，各析构一次 → 二次释放。
    //
    // 这里用的是**裸指针**，所以这五个函数是承重的，不是仪式——
    // 少写一个，ASan 立刻能复现出崩溃（见 legacy.md 缺陷 4 的实测输出）。
    ArrayStack(const ArrayStack& other)
        : capacity_(other.capacity_),
          top_index_(other.top_index_),
          data_(other.capacity_ ? new T[other.capacity_] : nullptr) {
        for (size_type i = 0; i < top_index_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    /// 拷贝并交换：先构造完整副本再与自身交换，天然自赋值安全，
    /// 且拷贝失败时原对象不受影响。
    ArrayStack& operator=(const ArrayStack& other) {
        if (this != &other) {
            ArrayStack copy(other);
            swap(copy);
        }
        return *this;
    }

    ArrayStack(ArrayStack&& other) noexcept { swap(other); }

    ArrayStack& operator=(ArrayStack&& other) noexcept {
        if (this != &other) {
            ArrayStack moved(std::move(other));  // other 交出所有权
            swap(moved);                         // 自己原来的缓冲区随 moved 析构释放
        }
        return *this;
    }

    ~ArrayStack() { delete[] data_; }
    // <<< rule-of-five

    void swap(ArrayStack& other) noexcept {
        using std::swap;
        swap(capacity_, other.capacity_);
        swap(top_index_, other.top_index_);
        swap(data_, other.data_);
    }

    // >>> push
    /// 入栈。容量不足时按算法3.3 的策略翻倍。
    /// 强异常保证：搬迁在新缓冲区上完成，中途抛异常则原栈原封不动。
    void push(const T& item) {
        ensure_capacity();
        data_[top_index_] = item;
        ++top_index_;
    }

    void push(T&& item) {
        ensure_capacity();
        data_[top_index_] = std::move(item);
        ++top_index_;
    }
    // <<< push

    // >>> pop
    /// 出栈。空栈返回 std::nullopt——原书是「返回 false + 往 cout 打一行中文」，
    /// 调用方既没法在库里复用，也容易忽略返回值。
    [[nodiscard]] std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        --top_index_;
        return std::optional<T>(std::move(data_[top_index_]));
    }

    /// 读栈顶但不弹出，返回**副本**。空栈返回 std::nullopt，不是未定义行为。
    /// 要求 T 可拷贝；move-only 元素请用 peek()。
    [[nodiscard]] std::optional<T> top() const {
        if (empty()) {
            return std::nullopt;
        }
        return std::optional<T>(data_[top_index_ - 1]);
    }

    /// 只读观望栈顶：不拷贝、不弹出。空栈返回 nullptr。
    ///
    /// 与 top() 的分工——top() 给你一份可以带走的副本（安全，但要求 T 可拷贝，
    /// 而且确实拷了一次）；peek() 零拷贝，move-only 元素也能用，代价是
    /// **返回的指针在下一次 push / pop / clear 之后即失效**（扩容会换掉整块缓冲区）。
    /// 生命周期由调用方负责，这一点必须写在文档里，不能靠使用者猜。
    [[nodiscard]] const T* peek() const noexcept {
        return empty() ? nullptr : &data_[top_index_ - 1];
    }
    // <<< pop

    [[nodiscard]] bool empty() const noexcept { return top_index_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return top_index_; }
    [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

    /// 按下标读取，从栈底数起。越界抛 std::out_of_range（D-001 第 3 条）。
    [[nodiscard]] const T& at(size_type index) const {
        if (index >= top_index_) {
            throw std::out_of_range("ArrayStack::at: 下标越界");
        }
        return data_[index];
    }

    /// 清空。只把逻辑长度归零；已分配的容量留着复用，与原书 clear() 语义一致。
    void clear() noexcept { top_index_ = 0; }

private:
    // >>> grow
    static constexpr size_type kInitialCapacity = 4;

    void ensure_capacity() {
        if (top_index_ < capacity_) {
            return;
        }
        constexpr size_type kMax = std::numeric_limits<size_type>::max();
        if (capacity_ > kMax / 2) {
            throw std::overflow_error("ArrayStack: 容量翻倍会溢出");
        }
        const size_type next = capacity_ == 0 ? kInitialCapacity : capacity_ * 2;
        T* fresh = new T[next];
        try {
            for (size_type i = 0; i < top_index_; ++i) {
                // 这里是**赋值**而不是构造，所以不能用 std::move_if_noexcept：
                // 它检查的是移动**构造**是否 noexcept，而可抛的移动赋值会在搬迁
                // 中途把原栈的元素掏空——红队 T-002 实测复现过（见 legacy.md 缺陷 11）。
                //
                // 判据必须落在「移动赋值抛不抛」这一个维度上：
                //   移动赋值 noexcept → 移动。不可能抛，强异常保证不受影响，也不白白深拷贝。
                //   否则             → 复制。拷贝赋值取 const&，抛了也动不了原栈；
                //                      上面的 static_assert 保证走到这里的 T 一定可复制赋值。
                if constexpr (std::is_nothrow_move_assignable<T>::value) {
                    fresh[i] = std::move(data_[i]);
                } else {
                    fresh[i] = data_[i];
                }
            }
        } catch (...) {
            // 裸指针的代价：RAII 版本不用写这一段。搬迁失败要自己收拾新缓冲区，
            // 且此时还没动 data_/capacity_，所以原栈完好——这就是强异常保证。
            delete[] fresh;
            throw;
        }
        delete[] data_;
        data_ = fresh;
        capacity_ = next;
    }
    // <<< grow

    size_type capacity_{0};
    size_type top_index_{0};  // 栈中元素个数，同时也是下一个空位的下标
    T* data_{nullptr};
};

template <typename T>
void swap(ArrayStack<T>& a, ArrayStack<T>& b) noexcept {
    a.swap(b);
}

}  // namespace dsa
