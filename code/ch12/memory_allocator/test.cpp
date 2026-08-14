#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

using dsa::advanced::BoundaryAllocator;
using dsa::advanced::Fit;
using Layout = std::vector<std::pair<std::size_t, std::size_t>>;

/// 造出教科书里那种空闲区分布：五个**大小不同**的空洞，中间用已分配块隔开，
/// 这样它们不会合并。只有在这种布局上，三种策略才会挑出不同的块。
///
/// 布局（偏移 : 大小）——`#` 是占位的已分配块：
///   0:#100  100:free500  600:#50  650:free200  850:#50
///   900:free300  1200:#50  1250:free600  1850:#50
BoundaryAllocator holes_of_different_sizes() {
    BoundaryAllocator a(1900);
    const std::size_t p1 = *a.allocate(100, Fit::First);   // 占位
    const std::size_t h1 = *a.allocate(500, Fit::First);   // 将成为空洞 500
    const std::size_t p2 = *a.allocate(50, Fit::First);
    const std::size_t h2 = *a.allocate(200, Fit::First);   // 空洞 200
    const std::size_t p3 = *a.allocate(50, Fit::First);
    const std::size_t h3 = *a.allocate(300, Fit::First);   // 空洞 300
    const std::size_t p4 = *a.allocate(50, Fit::First);
    const std::size_t h4 = *a.allocate(600, Fit::First);   // 空洞 600
    const std::size_t p5 = *a.allocate(50, Fit::First);
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    a.release(h1);
    a.release(h2);
    a.release(h3);
    a.release(h4);
    return a;
}

/// **本单元的正题。** 同一组空闲区、同一个请求，三种策略必须挑出不同的块。
/// 把最佳适应和最坏适应的判据对调，这三条里至少两条会红。
void test_three_strategies_pick_different_blocks() {
    const Layout expected{{100, 500}, {650, 200}, {900, 300}, {1250, 600}};
    check(holes_of_different_sizes().free_blocks() == expected, "12.2.3 造出四个大小不同的空洞");

    // 请求 212：够大的空洞是 500、300、600（200 那块装不下）。
    BoundaryAllocator first = holes_of_different_sizes();
    check(first.allocate(212, Fit::First) == std::optional<std::size_t>{100},
          "12.2.3 首次适应挑最靠前的够用块（500 那个）");

    BoundaryAllocator best = holes_of_different_sizes();
    check(best.allocate(212, Fit::Best) == std::optional<std::size_t>{900},
          "12.2.3 最佳适应挑刚好够用的最小块（300 那个）");

    BoundaryAllocator worst = holes_of_different_sizes();
    check(worst.allocate(212, Fit::Worst) == std::optional<std::size_t>{1250},
          "12.2.3 最坏适应挑当前最大的块（600 那个）");

    // 剩下的碎片大小也不同：这正是三种策略的代价差别。
    check(first.free_blocks()[0] == std::pair<std::size_t, std::size_t>{312, 288},
          "12.2.3 首次适应在 500 那块里留下 288 的碎片");
    check(best.free_blocks()[2] == std::pair<std::size_t, std::size_t>{1112, 88},
          "12.2.3 最佳适应留下的碎片最小（88）——也最难再用");
    check(worst.free_blocks()[3] == std::pair<std::size_t, std::size_t>{1462, 388},
          "12.2.3 最坏适应留下的碎片最大（388）——还大到能再分一次");
}

/// 首次适应「快」的全部含义：扫得少。
void test_first_fit_scans_least() {
    BoundaryAllocator first = holes_of_different_sizes();
    (void)first.allocate(212, Fit::First);
    const std::size_t first_steps = first.last_scan_steps();

    BoundaryAllocator best = holes_of_different_sizes();
    (void)best.allocate(212, Fit::Best);
    check(first_steps < best.last_scan_steps(),
          "12.2.3 首次适应找到就停，最佳适应必须扫完整张表");
}

void test_split_and_exact_fit() {
    BoundaryAllocator a(100);
    check(a.block_count() == 1 && a.free_bytes() == 100, "12.2.3 初始是一整块空闲区");

    const auto p = a.allocate(30, Fit::First);
    check(p == std::optional<std::size_t>{0}, "12.2.3 从头分配");
    check(a.free_blocks() == Layout({{30, 70}}), "12.2.3 分裂之后剩下 70");
    check(a.block_count() == 2, "12.2.3 分裂产生一个新块");

    // 请求恰好等于块大小：不该再分裂出一个 0 字节的块。
    const auto q = a.allocate(70, Fit::First);
    check(q == std::optional<std::size_t>{30}, "12.2.3 恰好用完剩下的空闲区");
    check(a.block_count() == 2 && a.free_bytes() == 0, "12.2.3 大小相等时不分裂");
    check(a.largest_free_block() == 0, "12.2.3 没有空闲块了");

    check(!a.allocate(1, Fit::First).has_value(), "12.2.3 空间不足返回 nullopt");
    check(!a.allocate(1, Fit::Best).has_value(), "12.2.3 最佳适应同样");
    check(!a.allocate(1, Fit::Worst).has_value(), "12.2.3 最坏适应同样");
}

/// 合并要看左右两侧。只合一侧的实现能通过「一个空洞」的测试，却会在这里露馅。
void test_coalescing_on_both_sides() {
    BoundaryAllocator a(90);
    const auto left = *a.allocate(30, Fit::First);
    const auto middle = *a.allocate(30, Fit::First);
    const auto right = *a.allocate(30, Fit::First);
    check(a.free_bytes() == 0, "12.2.3 三块分光");

    a.release(left);
    check(a.free_blocks() == Layout({{0, 30}}), "12.2.3 释放最左边一块");
    a.release(right);
    check(a.free_blocks() == Layout({{0, 30}, {60, 30}}),
          "12.2.3 释放最右边一块，两个空洞不相邻、不能合并");
    check(a.free_bytes() == 60 && a.largest_free_block() == 30,
          "12.2.3 空闲 60 字节，但最大的一块只有 30——这就是外部碎片");
    check(!a.allocate(45, Fit::First).has_value(), "12.2.3 空闲总量够、单块不够，分配失败");

    // 释放中间那块：左右都是空闲，必须三块并成一块。
    a.release(middle);
    check(a.free_blocks() == Layout({{0, 90}}), "12.2.3 释放中间块，左右一起合并成整块");
    check(a.block_count() == 1 && a.largest_free_block() == 90, "12.2.3 合并后回到一整块");
    check(a.allocate(90, Fit::First).has_value(), "12.2.3 合并之后大请求又能满足了");
}

void test_release_contract() {
    BoundaryAllocator a(100);
    const auto p = *a.allocate(40, Fit::First);
    check(a.release(p), "12.2.3 释放已分配的块返回 true");
    check(!a.release(p), "12.2.3 重复释放返回 false，不是二次归还");
    check(!a.release(9999), "12.2.3 释放不存在的偏移返回 false");
    check(a.free_bytes() == 100, "12.2.3 重复释放没有把空闲量算重");

    bool threw = false;
    try {
        (void)a.allocate(0, Fit::First);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "12.2.3 请求 0 字节抛 invalid_argument");

    threw = false;
    try {
        BoundaryAllocator empty(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "12.2.3 容量为 0 的分配器被拒绝");
}

/// 反复分配/释放：块表不能只增不减，空闲量必须每轮都回到满值。
/// 上一版在这里会越界——合并之后没有归还元数据槽位。
void test_repeated_cycles_do_not_leak_metadata() {
    BoundaryAllocator a(1000);
    bool always_full = true;
    for (int round = 0; round < 5000; ++round) {
        const auto p = a.allocate(10, Fit::First);
        if (!p) {
            always_full = false;
            break;
        }
        a.release(*p);
        if (a.free_bytes() != 1000 || a.block_count() != 1) {
            always_full = false;
            break;
        }
    }
    check(always_full, "12.2.3 5000 轮分配/释放：每轮都合并回一整块，块表不膨胀");

    // 交错持有多块再全部释放，最终也必须并回一整块。
    std::vector<std::size_t> held;
    for (int i = 0; i < 20; ++i) {
        held.push_back(*a.allocate(50, Fit::First));
    }
    check(a.free_bytes() == 0, "12.2.3 20 块占满");
    for (std::size_t i = 0; i < held.size(); i += 2) {
        a.release(held[i]);   // 先释放偶数块，制造相间的空洞
    }
    check(a.free_block_count() == 10, "12.2.3 相间释放产生 10 个空洞");
    for (std::size_t i = 1; i < held.size(); i += 2) {
        a.release(held[i]);   // 再释放奇数块，空洞应当全部并起来
    }
    check(a.free_blocks() == Layout({{0, 1000}}), "12.2.3 全部释放后并回一整块");
    check(a.block_count() == 1, "12.2.3 块表回到一项");
}
}  // namespace

int main() {
    test_three_strategies_pick_different_blocks();
    test_first_fit_scans_least();
    test_split_and_exact_fit();
    test_coalescing_on_both_sides();
    test_release_contract();
    test_repeated_cycles_do_not_leak_metadata();
    std::printf("BoundaryAllocator: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
