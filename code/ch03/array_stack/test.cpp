// ArrayStack 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 每一条用例都对着 legacy.md 里的一条缺陷：**如果实现退回原书的写法，
// 这里必须有一条会红**。写新用例时请照这个标准，别写「顺手测一下」的用例。
#include "modern.hpp"

#include "support/fault_injection.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// T-004：ArrayStack 改用裸存储之后，它的分配走的是
// `::operator new(bytes, std::align_val_t)`，而不再是 `new T[n]`。
// 于是 support 里那个靠重载 `T::operator new[]` 注入 bad_alloc 的探针
// （`AllocationFailure`）在这里**再也不会被调用**——它挂在类上，
// 而我们调的是全局函数。所以改成在本翻译单元替换全局的对齐版 operator new。
//
// 实测确认这不会削弱 sanitizer：委托给 `std::aligned_alloc`/`std::free`，
// ASan 照样拦截，越界与 use-after-free 仍然带完整栈回溯地报出来
// （legacy.md 缺陷 14 附了一份真实的 heap-use-after-free 报告）。
namespace {
bool g_fail_next_allocation = false;
}

// 转调**未被替换的**普通 `::operator new`，再自己对齐，把原始指针藏在返回地址前面。
// 这样 ASan 照常记账（实测：越界与 use-after-free 仍带完整栈回溯报出）。
//
// 两个替换都标了 noinline。原因是 gcc 会把它们内联进 `ArrayStack::deallocate`，
// 然后拿**标准的**对齐 operator new/delete 语义去做配对分析，于是
// -Wmismatched-new-delete / -Warray-bounds 对一个**合法的全局替换**报红。
// 不内联就没有那个上下文，诊断也就不会误判。这里不用 #pragma 关诊断——
// 关掉的是整条规则，不内联只是挡住这一处误判。
#if defined(__GNUC__)
#define DSA_TEST_NOINLINE __attribute__((noinline))
#else
#define DSA_TEST_NOINLINE
#endif

DSA_TEST_NOINLINE void* operator new(std::size_t bytes, std::align_val_t alignment) {
    if (g_fail_next_allocation) {
        g_fail_next_allocation = false;
        throw std::bad_alloc();
    }
    const std::size_t align = static_cast<std::size_t>(alignment);
    void* base = ::operator new(bytes + align + sizeof(void*));
    const auto raw = reinterpret_cast<std::uintptr_t>(base) + sizeof(void*);
    const auto aligned = ((raw + align - 1) / align) * align;
    reinterpret_cast<void**>(aligned)[-1] = base;
    return reinterpret_cast<void*>(aligned);
}

DSA_TEST_NOINLINE void operator delete(void* p, std::align_val_t) noexcept {
    if (p != nullptr) {
        ::operator delete(reinterpret_cast<void**>(p)[-1]);
    }
}
DSA_TEST_NOINLINE void operator delete(void* p, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(p, alignment);
}

namespace {

using dsa::testing::Counted;
using dsa::testing::CountedConstruction;
using dsa::testing::ThrowingCopyConstruction;
using dsa::testing::ThrowingMoveConstruction;

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what);
    }
}

// 缺陷 6/7：原书 pop 用 `bool pop(T&)` 出参 + cout 提示；这里是 optional。
void test_lifo_order() {
    dsa::ArrayStack<int> s(2);
    s.push(1);
    s.push(2);
    s.push(3);
    check(s.size() == 3, "push 三次后 size==3");
    check(s.top() == 3, "top 是最后压入的");
    check(s.pop() == 3, "pop 出 3");
    check(s.pop() == 2, "pop 出 2");
    check(s.pop() == 1, "pop 出 1");
    check(!s.pop().has_value(), "空栈 pop 返回 nullopt");
    check(!s.top().has_value(), "空栈 top 返回 nullopt");
    check(s.empty(), "弹空后 empty()");
}

// 缺陷 3：原书无参构造只设 top=-1，mSize/st 未初始化，析构 delete[] 野指针。
void test_default_constructed_is_usable() {
    dsa::ArrayStack<int> s;  // 原书这一步之后，析构就是未定义行为
    check(s.empty() && s.size() == 0, "默认构造是空栈");
    check(s.capacity() == 0, "默认构造不预分配");
    check(!s.pop().has_value(), "默认构造的栈可以安全 pop");
    s.push(42);
    check(s.top() == 42, "默认构造的栈可以正常 push");
}

// 缺陷 4：原书没有拷贝构造/拷贝赋值 → 浅拷贝 → 二次释放（ASan 已实测复现，见 legacy.md）。
void test_copy_is_deep() {
    dsa::ArrayStack<int> a(4);
    a.push(1);
    a.push(2);
    dsa::ArrayStack<int> b = a;      // 原书：b.data_ == a.data_
    b.push(3);
    check(a.size() == 2, "改副本不影响原栈的 size");
    check(a.top() == 2, "改副本不影响原栈的栈顶");
    check(b.size() == 3 && b.top() == 3, "副本自身正确");

    dsa::ArrayStack<int> c(1);
    c.push(99);
    c = a;
    check(c.size() == 2 && c.top() == 2, "勘误E22 五法则：拷贝赋值得到独立副本，两个对象各自析构不二次释放");
    dsa::ArrayStack<int>& copy_alias = c;
    c = copy_alias;                   // 自赋值（经别名避开 -Wself-assign-overloaded）
    check(c.size() == 2 && c.top() == 2, "自赋值后仍然完好");
    // 三个对象在这里各自析构一次。原书写法在此处 double-free。
}

void test_move_semantics() {
    dsa::ArrayStack<int> a(4);
    a.push(7);
    dsa::ArrayStack<int> b = std::move(a);
    check(b.size() == 1 && b.top() == 7, "移动后新对象持有数据");
    check(a.empty(), "被移动方处于有效的空状态");
    a.push(8);  // 有效状态意味着可以继续用
    check(a.size() == 1, "被移动方仍可复用");

    dsa::ArrayStack<int> c(2);
    c.push(1);
    c = std::move(b);
    check(c.size() == 1 && c.top() == 7, "移动赋值取得对方的数据");
    // 自移动赋值不得自毁。绕一层引用是因为直接写 `c = std::move(c)`
    // 会被 -Wself-move 拦下（-Werror 下即编译失败）——而运行期真正会撞上这种情况的，
    // 恰恰是「两个引用/指针指向同一对象」这种编译器看不出来的写法。
    dsa::ArrayStack<int>& alias = c;
    c = std::move(alias);
    check(c.size() == 1, "自移动赋值后仍然完好");
}

// 缺陷 8：原书 `bool push(const T item)` 按值传参，move-only 类型根本用不了。
void test_move_only_element() {
    dsa::ArrayStack<std::unique_ptr<int>> s;
    for (int i = 0; i < 10; ++i) {
        s.push(std::make_unique<int>(i));
    }
    check(s.size() == 10, "move-only 元素可以入栈");
    // top() 返回副本，move-only 元素用不了；peek() 零拷贝，可以。
    const std::unique_ptr<int>* seen = s.peek();
    check(seen != nullptr && **seen == 9, "peek 可以观望 move-only 的栈顶元素");
    auto item = s.pop();
    check(item.has_value() && **item == 9, "move-only 元素可以出栈且值正确");
}

void test_peek_does_not_copy() {
    dsa::ArrayStack<Counted> s(4);
    s.push(Counted(1));
    s.push(Counted(2));

    Counted::reset();
    const Counted* p = s.peek();
    check(p != nullptr && p->v == 2, "peek 指向栈顶元素");
    check(Counted::copies == 0, "peek 一次拷贝都不做");

    Counted::reset();
    auto copy = s.top();
    check(copy.has_value() && copy->v == 2, "top 返回等值的副本");
    check(Counted::copies >= 1, "top 确实拷贝了——这正是 peek 存在的理由");

    check(s.size() == 2, "peek/top 都不改变栈");
}

void test_peek_on_empty_is_nullptr() {
    dsa::ArrayStack<int> s;
    check(s.peek() == nullptr, "空栈 peek 返回 nullptr，不是未定义行为");
    s.push(7);
    check(s.peek() != nullptr && *s.peek() == 7, "非空栈 peek 可用");
    (void)s.pop();
    check(s.peek() == nullptr, "弹空后 peek 又回到 nullptr");
}

// 契约的另一半：扩容会换掉整块缓冲区，之前 peek 到的指针随之失效。
// 这里不去解引用旧指针（那是未定义行为），只验证「重新 peek 拿到的是对的」。
void test_peek_after_growth_is_refetched() {
    dsa::ArrayStack<int> s(1);
    s.push(1);
    check(*s.peek() == 1, "扩容前 peek 正确");
    for (int i = 2; i <= 100; ++i) {
        s.push(i);  // 中途必然发生多次扩容
    }
    check(s.peek() != nullptr && *s.peek() == 100, "扩容后重新 peek 仍然正确");
    check(s.at(0) == 1, "扩容后栈底元素完好");
}

// 算法3.3：扩容策略。原书那段按印刷原样根本编译不过（`i` 未声明）。
void test_growth_preserves_contents() {
    dsa::ArrayStack<std::string> s(1);
    const int n = 1000;
    for (int i = 0; i < n; ++i) {
        s.push("item-" + std::to_string(i));
    }
    check(s.size() == static_cast<std::size_t>(n), "扩容后元素个数正确");
    check(s.capacity() >= s.size(), "容量不小于长度");
    bool all_ok = true;
    for (int i = n - 1; i >= 0; --i) {
        auto v = s.pop();
        all_ok = all_ok && v.has_value() && *v == "item-" + std::to_string(i);
    }
    check(all_ok, "扩容过程中 1000 个元素逐个原样保留");

    dsa::ArrayStack<int> g(1);
    std::size_t reallocations = 0, last = g.capacity();
    for (int i = 0; i < 64; ++i) {
        g.push(i);
        if (g.capacity() != last) {
            ++reallocations;
            last = g.capacity();
        }
    }
    // 翻倍策略下 64 次 push 只该重新分配 O(log n) 次；线性增长会是几十次。
    check(reallocations <= 8, "扩容次数是对数级（翻倍策略生效）");
}

// 缺陷 10 的正面验证：扩容中途抛异常，原栈必须原封不动（强异常保证）。
// 这是**故障注入**，不是推理——第 N 次拷贝赋值必抛。
void test_strong_exception_guarantee_on_growth() {
    dsa::ArrayStack<ThrowingCopyConstruction> s(4);
    ThrowingCopyConstruction::reset();
    for (int i = 0; i < 4; ++i) {
        s.push(ThrowingCopyConstruction(i));  // 填满，下一次 push 必然触发扩容
    }
    const auto size_before = s.size();
    const auto capacity_before = s.capacity();

    // 扩容要搬 4 个元素，让第 3 个搬迁失败。
    // T-004：注入点从「拷贝赋值」挪到了「拷贝构造」——裸存储版本搬迁执行的是构造。
    ThrowingCopyConstruction::reset(3);
    bool threw = false;
    try {
        s.push(ThrowingCopyConstruction(99));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ThrowingCopyConstruction::reset();

    check(threw, "扩容中途的异常如实抛出，没有被吞掉");
    check(s.size() == size_before, "失败后长度不变");
    check(s.capacity() == capacity_before, "失败后容量不变（没有半途换掉缓冲区）");
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "失败后原有元素逐个完好——强异常保证成立");

    // 失败之后栈仍然可用
    s.push(ThrowingCopyConstruction(4));
    check(s.size() == size_before + 1, "异常之后栈仍可继续使用");
}

// 红队 T-002：分配本身抛 bad_alloc 时，新存储尚未取得，旧栈也必须原样保留。
void test_bad_alloc_preserves_stack() {
    dsa::ArrayStack<int> s(2);
    s.push(1);
    s.push(2);
    const auto capacity_before = s.capacity();
    g_fail_next_allocation = true;
    bool threw = false;
    try {
        s.push(3);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    g_fail_next_allocation = false;
    check(threw, "扩容时分配存储的 bad_alloc 如实抛出");
    check(s.size() == 2 && s.capacity() == capacity_before, "bad_alloc 后长度与容量不变");
    check(s.at(0) == 1 && s.at(1) == 2, "bad_alloc 后原有元素完好");
}

// 红队 T-002 的继承者。原来的判据是「移动构造 noexcept 不能证明移动赋值不抛」，
// 因为搬迁走的是赋值。**T-004 之后搬迁走的是构造，这道题就消失了**：
// std::move_if_noexcept 问的正是「移动构造抛不抛」，与实际执行的动作对上了。
// 这条用例守住的是：遇到会抛的移动构造，扩容必须退回拷贝，一次移动都不许发生。
void test_throwing_move_construction_falls_back_to_copy() {
    dsa::ArrayStack<ThrowingMoveConstruction> s(4);
    ThrowingMoveConstruction::reset();
    for (int i = 0; i < 4; ++i) {
        s.push(ThrowingMoveConstruction(i));  // push 自己会走一次移动构造
    }
    const int moves_before = ThrowingMoveConstruction::moves;
    const int copies_before = ThrowingMoveConstruction::copies;
    s.push(ThrowingMoveConstruction(99));  // 这一次触发扩容，要搬 4 个元素
    const int moves_by_growth = ThrowingMoveConstruction::moves - moves_before - 1;  // 减掉 push 自己那次
    const int copies_by_growth = ThrowingMoveConstruction::copies - copies_before;

    check(s.size() == 5 && s.capacity() == 8, "移动构造可抛时扩容照样完成");
    check(moves_by_growth == 0, "移动构造会抛，搬迁一次都不许用它");
    check(copies_by_growth == 4, "4 个元素全部走拷贝构造搬过去");
    bool intact = true;
    for (std::size_t i = 0; i < 4; ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "移动构造可抛时扩容后旧元素仍完整");
}

// 红队 T-002 复核补充：修好强异常保证之后，**不能顺手把移动快路径也弄丢**。
// 判据必须落在「移动赋值抛不抛」上，而不是「可不可复制」——否则 std::string 这类
// 移动赋值本就 noexcept 的元素，每次扩容都变成深拷贝，摊还 O(1) 的教学点就打了折扣。
void test_growth_moves_when_move_construction_is_noexcept() {
    static_assert(std::is_nothrow_move_constructible<std::string>::value,
                  "std::string 的移动构造本就是 noexcept——这正是不该退化成深拷贝的理由");
    dsa::ArrayStack<CountedConstruction> s(1);
    CountedConstruction::reset();
    for (int i = 0; i < 64; ++i) {
        s.push(CountedConstruction(i));  // 每次 push 一次移动构造，扩容再搬 63 次
    }
    check(CountedConstruction::copies == 0, "移动构造 noexcept 的元素，扩容时一次都不该拷贝");
    check(CountedConstruction::moves > 64, "扩容搬迁确实走了移动");
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(intact, "走移动快路径后元素依然正确");
}

// >>> T-004 存储层
// 缺陷 12：`new T[n]` 会把整块槽位默认构造一遍——预留 1000 的容量就先造 1000 个对象。
// 裸存储版本只分配字节，**一个构造函数都不该被调用**。
void test_reserved_capacity_constructs_nothing() {
    CountedConstruction::reset();
    int before = CountedConstruction::copies + CountedConstruction::moves;
    dsa::ArrayStack<CountedConstruction> s(1000);
    check(s.capacity() == 1000, "预留容量生效");
    check(s.empty(), "预留容量不等于有元素");
    check(CountedConstruction::copies + CountedConstruction::moves == before,
          "预留 1000 的容量，一次元素构造都没有发生");
}

// 缺陷 12 的另一面：栈没有任何理由要求元素可默认构造。
// `new T[n]` 版本这里是一条 static_assert 编译错误（legacy.md 有原文）。
struct NoDefault {
    int v;
    explicit NoDefault(int x) : v(x) {}
};

void test_element_need_not_be_default_constructible() {
    static_assert(!std::is_default_constructible<NoDefault>::value,
                  "这个探针的全部意义就是不可默认构造");
    dsa::ArrayStack<NoDefault> s;
    s.push(NoDefault(1));
    s.push(NoDefault(2));
    check(s.size() == 2, "不可默认构造的元素可以入栈");
    check(s.at(0).v == 1 && s.at(1).v == 2, "元素值正确");
    auto top = s.pop();
    check(top.has_value() && top->v == 2, "也可以出栈");
}

// 缺陷 13：pop 与 clear 必须**真的析构**元素，否则死元素一直占着资源。
// 教学版实测：1000 个各持 200 字节的元素，clear() 之后仍有 1024 个活着、占 200 KB。
struct Owning {
    inline static int live = 0;
    char* p;
    Owning() : p(new char[16]) { ++live; }
    Owning(const Owning&) : p(new char[16]) { ++live; }
    Owning(Owning&& other) noexcept : p(other.p) {
        other.p = nullptr;
        ++live;
    }
    Owning& operator=(const Owning&) = delete;
    Owning& operator=(Owning&&) = delete;
    ~Owning() {
        delete[] p;
        --live;
    }
};

void test_pop_and_clear_destroy_elements() {
    {
        dsa::ArrayStack<Owning> s;
        for (int i = 0; i < 100; ++i) {
            s.push(Owning{});
        }
        check(Owning::live == 100, "满栈时活着的元素个数恰好等于 size，没有多余的槽内对象");
        (void)s.pop();
        check(Owning::live == 99, "pop 之后那个槽位被析构了");
        s.clear();
        check(Owning::live == 0, "clear 逐个析构元素，不是只把长度归零");
        check(s.capacity() >= 100, "clear 仍然保留已分配的容量");
        for (int i = 0; i < 3; ++i) {
            s.push(Owning{});
        }
        check(Owning::live == 3, "clear 之后重新 push，没有够不着的死元素残留");
    }
    check(Owning::live == 0, "栈析构后元素全部回收");
}

// 裸存储换来的自由，对应一份义务：对齐要自己管。
// `new T[n]` 替你保证的事，`::operator new(bytes)` 不管——所以 allocate() 显式传了
// alignof(T)。过对齐的元素落错地方在 x86 上往往「碰巧能跑」，UBSan 才会报。
struct alignas(64) OverAligned {
    int v{0};
    explicit OverAligned(int x) : v(x) {}
};

void test_over_aligned_element() {
    dsa::ArrayStack<OverAligned> s;
    for (int i = 0; i < 40; ++i) {  // 跨过好几次扩容
        s.push(OverAligned(i));
    }
    bool aligned = true;
    bool intact = true;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto address = reinterpret_cast<std::uintptr_t>(&s.at(i));
        aligned = aligned && (address % alignof(OverAligned) == 0);
        intact = intact && s.at(i).v == static_cast<int>(i);
    }
    check(aligned, "alignas(64) 的元素每一个都落在 64 字节边界上");
    check(intact, "过对齐元素跨扩容后值仍正确");
}
// <<< T-004 存储层

// 缺陷 7：容器不该做 I/O。原书 push/pop 失败时直接 cout 打中文提示。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    {
        dsa::ArrayStack<int> s(1);
        s.push(1);
        s.push(2);       // 触发扩容：原书这里打「栈满溢出」
        (void)s.pop();
        (void)s.pop();
        (void)s.pop();   // 空栈出栈：原书这里打「栈为空，不能执行出栈操作」
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "容器全程不向 cout/cerr 写任何东西");
}

// D-001 第 3 条：越界抛 std::out_of_range，不是未定义行为、也不是打印一行了事。
void test_at_throws_out_of_range() {
    dsa::ArrayStack<int> s(4);
    s.push(10);
    s.push(20);
    check(s.at(0) == 10 && s.at(1) == 20, "at 按栈底起的下标读取");
    bool threw = false;
    try {
        (void)s.at(2);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "越界读取抛 std::out_of_range");
}

void test_clear_keeps_capacity() {
    dsa::ArrayStack<int> s(8);
    s.push(1);
    s.push(2);
    const auto cap = s.capacity();
    s.clear();
    check(s.empty(), "clear 后为空");
    check(s.capacity() == cap, "clear 保留已分配容量");
}

}  // namespace

int main() {
    test_lifo_order();
    test_default_constructed_is_usable();
    test_copy_is_deep();
    test_move_semantics();
    test_move_only_element();
    test_peek_does_not_copy();
    test_peek_on_empty_is_nullptr();
    test_peek_after_growth_is_refetched();
    test_growth_preserves_contents();
    test_strong_exception_guarantee_on_growth();
    test_bad_alloc_preserves_stack();
    test_throwing_move_construction_falls_back_to_copy();
    test_growth_moves_when_move_construction_is_noexcept();
    test_reserved_capacity_constructs_nothing();
    test_element_need_not_be_default_constructible();
    test_pop_and_clear_destroy_elements();
    test_over_aligned_element();
    test_no_console_output();
    test_at_throws_out_of_range();
    test_clear_keeps_capacity();

    std::printf("ArrayStack: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
