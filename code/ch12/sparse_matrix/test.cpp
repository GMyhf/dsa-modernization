#include "modern.hpp"

#include <cstdio>
#include <random>
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

using dsa::advanced::SparseMatrix;

std::vector<std::pair<std::size_t, int>> row_of(const SparseMatrix& m, std::size_t row) {
    std::vector<std::pair<std::size_t, int>> out;
    m.for_each_in_row(row, [&out](std::size_t col, int value) { out.emplace_back(col, value); });
    return out;
}

std::vector<std::pair<std::size_t, int>> column_of(const SparseMatrix& m, std::size_t col) {
    std::vector<std::pair<std::size_t, int>> out;
    m.for_each_in_column(col, [&out](std::size_t row, int value) { out.emplace_back(row, value); });
    return out;
}

/// 书里那张 4×5 的例子：只有 3 个非零元。
SparseMatrix sample() {
    SparseMatrix m(4, 5);
    m.set(2, 3, 7);
    m.set(0, 1, 4);
    m.set(2, 0, 9);
    return m;
}

void test_stores_only_nonzeros() {
    const SparseMatrix m = sample();
    check(m.rows() == 4 && m.cols() == 5, "12.1.3 矩阵形状");
    check(m.nonzeros() == 3, "12.1.3 只存非零元");
    check(m.get(2, 3) == 7 && m.get(0, 1) == 4 && m.get(2, 0) == 9, "12.1.3 取回非零元");
    check(m.get(1, 1) == 0 && m.get(3, 4) == 0, "12.1.3 没存的位置读作 0");
}

/// 两条链都必须有序，而且**同一个结点同时在两条链上**——这是十字链表的定义。
void test_both_chains_are_sorted() {
    const SparseMatrix m = sample();
    using Pairs = std::vector<std::pair<std::size_t, int>>;
    check(row_of(m, 2) == Pairs({{0, 9}, {3, 7}}), "12.1.3 行链按列号升序");
    check(row_of(m, 0) == Pairs({{1, 4}}), "12.1.3 单元素的行");
    check(row_of(m, 1).empty() && row_of(m, 3).empty(), "12.1.3 全零行是空链");

    check(column_of(m, 0) == Pairs({{2, 9}}), "12.1.3 列链按行号升序");
    check(column_of(m, 3) == Pairs({{2, 7}}), "12.1.3 另一条列链");
    check(column_of(m, 1) == Pairs({{0, 4}}), "12.1.3 第 1 列");
    check(column_of(m, 2).empty() && column_of(m, 4).empty(), "12.1.3 全零列是空链");

    // 插入次序不影响两条链的有序性。
    SparseMatrix backwards(3, 3);
    for (const int col : {2, 0, 1}) {
        backwards.set(1, static_cast<std::size_t>(col), col + 1);
    }
    check(row_of(backwards, 1) == Pairs({{0, 1}, {1, 2}, {2, 3}}), "12.1.3 乱序插入后行链仍有序");
    for (const int row : {2, 0, 1}) {
        backwards.set(static_cast<std::size_t>(row), 0, row + 10);
    }
    check(column_of(backwards, 0).size() == 3, "12.1.3 乱序插入后列链收齐");
    check(column_of(backwards, 0)[0].first == 0 && column_of(backwards, 0)[2].first == 2,
          "12.1.3 列链仍按行号升序");
}

/// 按列扫**只走该列**，不扫全表。这正是列链存在的理由，也是唯一能量出来的证据。
void test_column_scan_is_local() {
    SparseMatrix m(200, 200);
    for (std::size_t i = 0; i < 200; ++i) {
        m.set(i, i, static_cast<int>(i) + 1);   // 对角线
    }
    m.set(5, 7, 100);
    m.set(9, 7, 200);
    check(m.nonzeros() == 202, "12.1.3 对角线加两个非零元");

    m.reset_steps();
    const auto seventh = column_of(m, 7);
    check(seventh.size() == 3, "12.1.3 第 7 列有 3 个非零元");
    // 走过的结点数就是这一列的长度；换成扫全表会是 202。
    check(m.steps() == 3, "12.1.3 按列扫只走该列的 3 个结点，不是全表 202 个");
}

void test_update_and_erase() {
    SparseMatrix m = sample();
    m.set(2, 3, 42);
    check(m.get(2, 3) == 42 && m.nonzeros() == 3, "12.1.3 改已有非零元的值，不新增结点");

    m.set(2, 3, 0);
    check(m.get(2, 3) == 0 && m.nonzeros() == 2, "12.1.3 置零就从矩阵里摘掉");
    using Pairs = std::vector<std::pair<std::size_t, int>>;
    check(row_of(m, 2) == Pairs({{0, 9}}), "12.1.3 摘除后行链接上了");
    check(column_of(m, 3).empty(), "12.1.3 摘除后列链也接上了——两条链都要摘");

    m.set(1, 1, 0);
    check(m.nonzeros() == 2, "12.1.3 给本来就是零的位置写 0，什么都不做");

    // 摘掉链头与链尾各一次。
    SparseMatrix chain(1, 4);
    for (std::size_t col = 0; col < 4; ++col) {
        chain.set(0, col, static_cast<int>(col) + 1);
    }
    chain.set(0, 0, 0);
    check(row_of(chain, 0).size() == 3 && row_of(chain, 0)[0].first == 1, "12.1.3 摘掉行链的头");
    chain.set(0, 3, 0);
    check(row_of(chain, 0).size() == 2 && row_of(chain, 0)[1].first == 2, "12.1.3 摘掉行链的尾");
}

void test_bounds_and_move() {
    SparseMatrix m(2, 2);
    for (const auto& [row, col] : {std::pair<std::size_t, std::size_t>{2, 0}, {0, 2}}) {
        bool threw = false;
        try {
            m.set(row, col, 1);
        } catch (const std::out_of_range&) {
            threw = true;
        }
        check(threw, "12.1.3 下标越界抛 out_of_range");
    }
    bool threw = false;
    try {
        (void)m.get(9, 9);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "12.1.3 读越界同样抛");

    threw = false;
    try {
        m.for_each_in_column(9, [](std::size_t, int) {});
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "12.1.3 扫不存在的列抛 out_of_range");

    m.set(0, 0, 5);
    SparseMatrix moved = std::move(m);
    check(moved.get(0, 0) == 5 && moved.nonzeros() == 1, "12.1.3 移动构造");
    SparseMatrix target(1, 1);
    target = std::move(moved);
    check(target.get(0, 0) == 5, "12.1.3 移动赋值");
}

/// 与一张普通二维数组对拍：十字链表只是换了存法，读出来的矩阵必须一模一样。
void test_matches_a_dense_matrix() {
    std::mt19937 rng(20260814);
    std::uniform_int_distribution<int> value(-9, 9);
    std::uniform_int_distribution<std::size_t> pick(0, 29);

    SparseMatrix sparse(30, 30);
    std::vector<std::vector<int>> dense(30, std::vector<int>(30, 0));
    for (int step = 0; step < 4000; ++step) {
        const std::size_t r = pick(rng);
        const std::size_t c = pick(rng);
        const int v = value(rng);
        sparse.set(r, c, v);
        dense[r][c] = v;
    }

    std::size_t expected = 0;
    bool same = true;
    for (std::size_t r = 0; r < 30; ++r) {
        for (std::size_t c = 0; c < 30; ++c) {
            same = same && sparse.get(r, c) == dense[r][c];
            if (dense[r][c] != 0) {
                ++expected;
            }
        }
    }
    check(same, "12.1.3 4000 次随机写之后，逐元与二维数组一致");
    check(sparse.nonzeros() == expected, "12.1.3 非零元计数与二维数组一致");

    // 两条链扫出来的内容也必须与二维数组一致。
    bool rows_ok = true;
    bool cols_ok = true;
    for (std::size_t i = 0; i < 30; ++i) {
        std::vector<int> by_row(30, 0);
        sparse.for_each_in_row(i, [&by_row](std::size_t col, int v) { by_row[col] = v; });
        rows_ok = rows_ok && by_row == dense[i];

        std::vector<int> by_col(30, 0);
        sparse.for_each_in_column(i, [&by_col](std::size_t row, int v) { by_col[row] = v; });
        for (std::size_t r = 0; r < 30; ++r) {
            cols_ok = cols_ok && by_col[r] == dense[r][i];
        }
    }
    check(rows_ok, "12.1.3 逐行扫与二维数组一致");
    check(cols_ok, "12.1.3 逐列扫与二维数组一致——行链列链指的是同一批结点");
}

/// 一行里塞很多非零元：析构走循环，不能压穿栈。
///
/// 按**降序**插入，每次都落在链头，建表是 O(n)。升序插入则每次都要走完整条行链，
/// 是 O(n²)——单链表的固有代价，不是这里要测的东西。
void test_long_row_does_not_blow_the_stack() {
    constexpr std::size_t width = 200000;
    SparseMatrix wide(1, width);
    for (std::size_t col = width; col > 0; --col) {
        wide.set(0, col - 1, 1);
    }
    check(wide.nonzeros() == width, "12.1.3 一行 20 万个非零元");
    check(wide.get(0, width - 1) == 1, "12.1.3 行尾可读");
    check(wide.get(0, 0) == 1, "12.1.3 行首可读");
    // 作用域结束时逐行迭代释放。换成 unique_ptr 串链，这里就会栈溢出。
}
}  // namespace

int main() {
    test_stores_only_nonzeros();
    test_both_chains_are_sorted();
    test_column_scan_is_local();
    test_update_and_erase();
    test_bounds_and_move();
    test_matches_a_dense_matrix();
    test_long_row_does_not_blow_the_stack();
    std::printf("SparseMatrix: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
