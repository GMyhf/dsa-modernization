#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace dsa::advanced {

// >>> sparse-matrix
/// 十字链表存稀疏矩阵：**每个非零元同时挂在一条行链和一条列链上**。
///
/// 这句话是本节的全部内容。行链让「按行扫」变成顺着 `right` 走，列链让「按列扫」变成
/// 顺着 `down` 走——不必为了取一列而遍历整个矩阵。代价是每个结点多一个指针，
/// 以及插入时要在**两条**链上各定位一次。
///
/// | | 整块二维数组 | 三元组线性表 | 十字链表 |
/// | --- | --- | --- | --- |
/// | 存储量 | $mn$ | $O(t)$ | $O(t)$，每个元多一个指针 |
/// | 取 $a_{ij}$ | $O(1)$ | $O(\log t)$（有序表二分） | $O(\text{该行非零元数})$ |
/// | 按列扫一遍 | $O(m)$ | $O(t)$，要滤掉别的列 | $O(\text{该列非零元数})$ |
/// | 插入一个非零元 | $O(1)$ | $O(t)$，要挪动后面的项 | $O(\text{行内} + \text{列内定位})$ |
///
/// 结点所有权归容器：链是裸指针，析构**迭代**释放。这里不能用 `unique_ptr` 串链——
/// 一行里非零元一多，递归析构就会压穿栈（D-001 §2b，实测数字见 legacy.md）。
class SparseMatrix {
public:
    SparseMatrix(std::size_t rows, std::size_t cols)
        : row_heads_(rows, nullptr), col_heads_(cols, nullptr) {}

    SparseMatrix(const SparseMatrix&) = delete;
    SparseMatrix& operator=(const SparseMatrix&) = delete;
    SparseMatrix(SparseMatrix&& other) noexcept { take(other); }
    SparseMatrix& operator=(SparseMatrix&& other) noexcept {
        if (this != &other) {
            clear();
            take(other);
        }
        return *this;
    }
    ~SparseMatrix() { clear(); }

    [[nodiscard]] std::size_t rows() const noexcept { return row_heads_.size(); }
    [[nodiscard]] std::size_t cols() const noexcept { return col_heads_.size(); }
    [[nodiscard]] std::size_t nonzeros() const noexcept { return count_; }

    /// 写入。`value == 0` 表示把这个位置变回零元，也就是从两条链上摘掉。
    ///
    /// 定位是**局部**的：只在第 `row` 行和第 `col` 列上各走一段，
    /// 不碰其他行列。这正是十字链表与「每次重建整张列索引」的区别。
    void set(std::size_t row, std::size_t col, int value) {
        check(row, col);
        Node* row_prev = nullptr;
        Node* cursor = row_heads_[row];
        while (cursor != nullptr && cursor->col < col) {
            ++steps_;
            row_prev = cursor;
            cursor = cursor->right;
        }
        const bool exists = cursor != nullptr && cursor->col == col;

        if (exists && value != 0) {
            cursor->value = value;  // 就地改值，两条链一根都不用动
            return;
        }
        if (exists) {
            unlink(row, col, row_prev, cursor);
            return;
        }
        if (value == 0) {
            return;  // 本来就是零元，什么都不用做
        }

        Node* col_prev = nullptr;
        Node* down_cursor = col_heads_[col];
        while (down_cursor != nullptr && down_cursor->row < row) {
            ++steps_;
            col_prev = down_cursor;
            down_cursor = down_cursor->down;
        }

        Node* fresh = new Node{row, col, value, cursor, down_cursor};
        if (row_prev != nullptr) {
            row_prev->right = fresh;
        } else {
            row_heads_[row] = fresh;
        }
        if (col_prev != nullptr) {
            col_prev->down = fresh;
        } else {
            col_heads_[col] = fresh;
        }
        ++count_;
    }

    [[nodiscard]] int get(std::size_t row, std::size_t col) const {
        check(row, col);
        for (const Node* p = row_heads_[row]; p != nullptr && p->col <= col; p = p->right) {
            ++steps_;
            if (p->col == col) {
                return p->value;
            }
        }
        return 0;  // 没有存的位置就是零，不是错误
    }

    /// 按行扫：顺着 `right` 走一条链。
    template <typename Visitor>
    void for_each_in_row(std::size_t row, Visitor&& visit) const {
        if (row >= rows()) {
            throw std::out_of_range("SparseMatrix: row");
        }
        for (const Node* p = row_heads_[row]; p != nullptr; p = p->right) {
            ++steps_;
            visit(p->col, p->value);
        }
    }

    /// 按列扫：顺着 `down` 走一条链。**不必遍历整个矩阵**——列链存在的理由就是这个。
    template <typename Visitor>
    void for_each_in_column(std::size_t col, Visitor&& visit) const {
        if (col >= cols()) {
            throw std::out_of_range("SparseMatrix: column");
        }
        for (const Node* p = col_heads_[col]; p != nullptr; p = p->down) {
            ++steps_;
            visit(p->row, p->value);
        }
    }

    /// 走过多少个结点。教学计数器：用它验证「按列扫只走该列」而不是扫全表。
    [[nodiscard]] std::size_t steps() const noexcept { return steps_; }
    void reset_steps() const noexcept { steps_ = 0; }

private:
    struct Node {
        std::size_t row;
        std::size_t col;
        int value;
        Node* right;  // 行链：同一行的下一个非零元
        Node* down;   // 列链：同一列的下一个非零元
    };

    void check(std::size_t row, std::size_t col) const {
        if (row >= rows() || col >= cols()) {
            throw std::out_of_range("SparseMatrix: index");
        }
    }

    /// 从行链和列链上同时摘除。列链的前驱要现找——结点只存后继，不存前驱。
    void unlink(std::size_t row, std::size_t col, Node* row_prev, Node* target) {
        if (row_prev != nullptr) {
            row_prev->right = target->right;
        } else {
            row_heads_[row] = target->right;
        }
        Node* col_prev = nullptr;
        for (Node* p = col_heads_[col]; p != nullptr && p != target; p = p->down) {
            ++steps_;
            col_prev = p;
        }
        if (col_prev != nullptr) {
            col_prev->down = target->down;
        } else {
            col_heads_[col] = target->down;
        }
        delete target;
        --count_;
    }

    /// 逐行沿 `right` 迭代释放。**不是递归**——一行里非零元可以很多。
    void clear() noexcept {
        for (Node*& head : row_heads_) {
            Node* p = head;
            while (p != nullptr) {
                Node* next = p->right;
                delete p;
                p = next;
            }
            head = nullptr;
        }
        for (Node*& head : col_heads_) {
            head = nullptr;
        }
        count_ = 0;
    }

    void take(SparseMatrix& other) noexcept {
        row_heads_ = std::move(other.row_heads_);
        col_heads_ = std::move(other.col_heads_);
        count_ = other.count_;
        other.row_heads_.clear();
        other.col_heads_.clear();
        other.count_ = 0;
    }

    std::vector<Node*> row_heads_;
    std::vector<Node*> col_heads_;
    std::size_t count_ = 0;
    mutable std::size_t steps_ = 0;
};
// <<< sparse-matrix

}  // namespace dsa::advanced
