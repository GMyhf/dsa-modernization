// 顺序栈 ArrayStack —— 原书【代码3.1】【代码3.2】【算法3.3】的现代化实现。
//
// 遵循 collab/DECISION_LOG.md 的 D-001 风格公约：C++17；不拿 std::stack 替换；
// 存储结构是本节教学内容，所以用裸 T* 加**显式五法则**；容器内不做任何 I/O；
// 空状态返回 std::optional，越界/溢出抛标准异常。
//
// 保留原书要教的东西：这仍然是一个手写的、基于数组的栈，扩容仍然是算法3.3 的
// 「满了就翻倍」策略。修掉的是工程上的错（逐条见 legacy.md）。
//
// **存储层（T-004，2026-08-18）**：这一版把「分配存储」和「构造对象」拆开了。
// 教学版 teaching.hpp 用的是 `new T[n]`——一行就有数组，适合 3.1.2 讲清楚
// 「顺序栈就是一块连续数组」。代价在 legacy.md 缺陷 12/13 里量过：
// 那一行会把整块槽位**全部默认构造**，而且 pop/clear 之后死元素还活着。
// 工程版改用「裸存储 + placement new + 显式析构」：槽位里只有真正入栈的对象。
// 这正是 D-001 第 2 条说的「存储管理本身就是这一节的课」——
// 换成 unique_ptr 或 vector，这一课就没了。
#pragma once

#include <cstddef>
#include <limits>
#include <new>
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
public:
    using value_type = T;
    using size_type = std::size_t;
    // <<< class-head

    // 书稿不印：工程契约，不是 3.1 节的课。
    //
    // **这里少了一条**：教学版要求 `T` 可默认构造，因为 `new T[n]` 会构造整块槽位。
    // 换成裸存储之后这条要求消失了——栈本来就没有理由要求元素能默认构造
    // （legacy.md 缺陷 12 有编译错误原文）。
    static_assert(std::is_move_constructible<T>::value || std::is_copy_constructible<T>::value,
                  "ArrayStack<T>: T 必须能移动或复制构造（push 要把元素构造进槽位）");
    static_assert(std::is_nothrow_move_constructible<T>::value
                      || std::is_copy_constructible<T>::value,
                  "ArrayStack<T>: 扩容要保持强异常保证——移动构造要么不抛，要么 T 可复制构造");
    static_assert(!std::is_reference<T>::value, "ArrayStack<T>: T 不能是引用类型");

    /// 默认构造出一个容量为 0 的空栈——**可用、可析构**。
    /// 原书的无参构造只写了 top = -1，mSize 与 st 都没初始化，
    /// 析构时 delete[] 一个不确定指针（legacy.md 缺陷 3）。
    ArrayStack() noexcept = default;

    /// 预留容量。**只分配存储，不构造任何对象**——所以这里没有 T 的构造函数被调用。
    explicit ArrayStack(size_type capacity)
        : capacity_(capacity), data_(allocate(capacity)) {}

    // >>> rule-of-five
    // 三/五法则：原书只写了析构函数，没有拷贝构造与拷贝赋值，
    // 于是 `arrStack<int> b = a;` 之后两个对象持有同一根指针，各析构一次 → 二次释放。
    //
    // 这里用的是**裸指针**，所以这五个函数是承重的，不是仪式——
    // 少写一个，ASan 立刻能复现出崩溃（见 legacy.md 缺陷 4 的实测输出）。
    /// 逐个**拷贝构造**进新槽位。构造到一半抛异常时，已经建好的那几个要自己析构掉——
    /// `new T[n]` 版本不用写这一段（数组 new 会替你回滚），裸存储要自己负责。
    ArrayStack(const ArrayStack& other)
        : capacity_(other.capacity_), top_index_(0), data_(allocate(other.capacity_)) {
        try {
            for (; top_index_ < other.top_index_; ++top_index_) {
                construct_at(top_index_, other.data_[top_index_]);
            }
        } catch (...) {
            destroy_range(data_, top_index_);
            deallocate(data_, capacity_);
            throw;
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

    /// 先析构还活着的元素，再还存储。顺序反了就是对已释放内存调析构。
    ~ArrayStack() {
        destroy_range(data_, top_index_);
        deallocate(data_, capacity_);
    }
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
    /// 注意是**构造**而不是赋值：那个槽位此前根本没有对象。
    /// 构造失败时 top_index_ 还没加，栈原样不动。
    void push(const T& item) {
        ensure_capacity();
        construct_at(top_index_, item);
        ++top_index_;
    }

    void push(T&& item) {
        ensure_capacity();
        construct_at(top_index_, std::move(item));
        ++top_index_;
    }
    // <<< push

    // >>> pop
    /// 出栈。空栈返回 std::nullopt——原书是「返回 false + 往 cout 打一行中文」，
    /// 调用方既没法在库里复用，也容易忽略返回值。
    [[nodiscard]]
    std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        // 三步的次序是承重的：先把值搬出来（可能抛），成功了才缩短逻辑长度，
        // 最后**显式析构**那个槽位。若把 --top_index_ 提到前面，移动一抛，
        // 那个元素就既不在栈里、也没人析构它了。
        std::optional<T> value(std::move(data_[top_index_ - 1]));
        --top_index_;
        data_[top_index_].~T();
        return value;
    }

    /// 读栈顶但不弹出，返回**副本**。空栈返回 std::nullopt，不是未定义行为。
    /// 要求 T 可拷贝；move-only 元素请用 peek()。
    std::optional<T> top() const {
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
    const T* peek() const noexcept {
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

    /// 清空：**逐个析构**元素，容量留着复用。
    ///
    /// 教学版这里只写 `size_ = 0`，元素还活着——1000 个各持 200 字节的元素
    /// clear() 之后仍占着 200 KB，谁也够不着（legacy.md 缺陷 13 的实测数字）。
    void clear() noexcept {
        destroy_range(data_, top_index_);
        top_index_ = 0;
    }

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
        T* fresh = allocate(next);
        size_type built = 0;
        try {
            for (; built < top_index_; ++built) {
                // 搬迁现在是**构造**，于是 std::move_if_noexcept 恰好就是对的工具：
                // 它问的正是「移动**构造**抛不抛」，而这里执行的正是移动构造。
                //
                // 2026-08-12 第一版把这件事做在**赋值**语义上，两者的异常规格可以不同，
                // 红队 T-002 就是从这个缝里打进来的（legacy.md 缺陷 11），
                // 当时只能手写「看移动赋值抛不抛」的判据。换成裸存储后这道题消失了：
                //   移动构造 noexcept → 移动，不抛，也不白白深拷贝；
                //   否则             → 复制，抛了也动不了原栈（上面的 static_assert 保证可复制构造）。
                construct_at_in(fresh, built, std::move_if_noexcept(data_[built]));
            }
        } catch (...) {
            // 裸存储的代价：搬迁失败要自己析构已经建好的那几个、再还掉新存储。
            // 此时 data_/capacity_ 一个字节都没动，所以原栈完好——这就是强异常保证。
            destroy_range(fresh, built);
            deallocate(fresh, next);
            throw;
        }
        destroy_range(data_, top_index_);
        deallocate(data_, capacity_);
        data_ = fresh;
        capacity_ = next;
    }
    // <<< grow

    // >>> storage
    // 存储层：分配存储 / 构造对象 / 析构对象 / 归还存储，四件事分开。
    //
    // 用 `::operator new(bytes, align_val_t)` 而不是 `new T[n]`：后者会顺手把
    // 整块槽位默认构造一遍。**代价是对齐要自己管**——`new T[n]` 替你保证的
    // 对齐，这里必须显式传 alignof(T)，否则 `alignas(64)` 的元素会落在错地方。
    // 这是裸存储换来的自由所对应的那份义务。
    [[nodiscard]] static T* allocate(size_type count) {
        if (count == 0) {
            return nullptr;
        }
        if (count > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::overflow_error("ArrayStack: 请求的字节数溢出");
        }
        void* raw = ::operator new(count * sizeof(T), std::align_val_t{alignof(T)});
        return static_cast<T*>(raw);
    }

    static void deallocate(T* block, size_type count) noexcept {
        if (block == nullptr) {
            return;
        }
        ::operator delete(static_cast<void*>(block), count * sizeof(T),
                          std::align_val_t{alignof(T)});
    }

    /// 在第 index 个槽位上就地构造一个 T。槽位在此之前是**生存储，不是对象**。
    template <typename U>
    void construct_at(size_type index, U&& value) {
        construct_at_in(data_, index, std::forward<U>(value));
    }

    template <typename U>
    static void construct_at_in(T* block, size_type index, U&& value) {
        ::new (static_cast<void*>(block + index)) T(std::forward<U>(value));
    }

    /// 逆序析构 [0, count) 这些元素。只析构，不还存储。
    static void destroy_range(T* block, size_type count) noexcept {
        for (size_type i = count; i-- > 0;) {
            block[i].~T();
        }
    }
    // <<< storage

    size_type capacity_{0};
    size_type top_index_{0};  // 栈中元素个数，同时也是下一个空位的下标
    T* data_{nullptr};        // 指向一块**生存储**：只有 [0, top_index_) 里住着对象
};

template <typename T>
void swap(ArrayStack<T>& a, ArrayStack<T>& b) noexcept {
    a.swap(b);
}

}  // namespace dsa
