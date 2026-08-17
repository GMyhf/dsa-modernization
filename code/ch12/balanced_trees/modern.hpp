#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

namespace dsa::advanced {

// >>> avl-tree
/// AVL 树：每个结点的左右子树高度差不超过 1。
///
/// 平衡靠四种旋转维持。设失衡结点为 A，**新结点插在哪个方向**决定用哪一种：
///
/// | 情形 | 新结点位置 | 做法 |
/// | --- | --- | --- |
/// | LL | A 的左孩子的左子树 | 在 A 上右旋一次 |
/// | RR | A 的右孩子的右子树 | 在 A 上左旋一次 |
/// | LR | A 的左孩子的右子树 | 先在左孩子上左旋，再在 A 上右旋 |
/// | RL | A 的右孩子的左子树 | 先在右孩子上右旋，再在 A 上左旋 |
///
/// 子树用 `unique_ptr` 表示所有权：孩子唯一属于父结点，树高是 $O(\log n)$，
/// 递归释放不构成栈风险（判据见 2.3.1 节的「所有权工具怎么选」）。
class AvlTree {
public:
    void insert(int key) { root_ = insert(std::move(root_), key); }
    void erase(int key) { root_ = erase(std::move(root_), key); }

    [[nodiscard]] bool contains(int key) const {
        const Node* node = root_.get();
        while (node != nullptr) {
            if (key == node->key) {
                return true;
            }
            node = key < node->key ? node->left.get() : node->right.get();
        }
        return false;
    }

    /// 树高。空树为 0。AVL 的高度上界是 $1.4405\log_2(n+2)-0.3277$，测试按这条断言。
    [[nodiscard]] int height() const { return height_of(root_); }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// 中序周游。BST 的中序序列必须严格升序——旋转改变树形，**不能改变这个序列**，
    /// 所以它是检验旋转有没有写错的最直接的不变量。
    template <typename Visitor>
    void inorder(Visitor&& visit) const {
        inorder_impl(root_.get(), visit);
    }

private:
    struct Node {
        int key;
        int height{1};
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        explicit Node(int k) : key(k) {}
    };

    static int height_of(const std::unique_ptr<Node>& node) { return node ? node->height : 0; }

    static void refresh(Node* node) {
        node->height = 1 + std::max(height_of(node->left), height_of(node->right));
    }

    /// 平衡因子：右子树高 − 左子树高。绝对值超过 1 就要旋转。
    static int balance_factor(const std::unique_ptr<Node>& node) {
        return node ? height_of(node->right) - height_of(node->left) : 0;
    }

    static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> x) {
        auto y = std::move(x->right);
        x->right = std::move(y->left);
        refresh(x.get());
        y->left = std::move(x);
        refresh(y.get());
        return y;
    }

    static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> y) {
        auto x = std::move(y->left);
        y->left = std::move(x->right);
        refresh(y.get());
        x->right = std::move(y);
        refresh(x.get());
        return x;
    }

    /// 插入后自底向上回溯，第一个失衡的结点按上表选旋转。
    std::unique_ptr<Node> insert(std::unique_ptr<Node> node, int key) {
        if (!node) {
            ++size_;
            return std::make_unique<Node>(key);
        }
        if (key < node->key) {
            node->left = insert(std::move(node->left), key);
        } else if (key > node->key) {
            node->right = insert(std::move(node->right), key);
        } else {
            return node;  // 重复键不插入
        }
        refresh(node.get());
        const int balance = balance_factor(node);
        if (balance > 1) {                       // 右边高
            if (key < node->right->key) {        // RL：先把右孩子右旋
                node->right = rotate_right(std::move(node->right));
            }
            return rotate_left(std::move(node)); // RR / RL 的第二步
        }
        if (balance < -1) {                      // 左边高
            if (key > node->left->key) {         // LR：先把左孩子左旋
                node->left = rotate_left(std::move(node->left));
            }
            return rotate_right(std::move(node));// LL / LR 的第二步
        }
        return node;
    }

    /// 删除同样要回溯旋转。与插入的区别：这里按**子树的平衡因子**判方向，
    /// 不能再看「新键插在哪边」——删除没有新键。
    std::unique_ptr<Node> erase(std::unique_ptr<Node> node, int key) {
        if (!node) {
            return node;
        }
        if (key < node->key) {
            node->left = erase(std::move(node->left), key);
        } else if (key > node->key) {
            node->right = erase(std::move(node->right), key);
        } else {
            if (!node->left) {
                --size_;
                return std::move(node->right);
            }
            if (!node->right) {
                --size_;
                return std::move(node->left);
            }
            // 两个孩子都在：用右子树的最小键顶上来，再去右子树删掉那个键。
            const Node* successor = node->right.get();
            while (successor->left) {
                successor = successor->left.get();
            }
            node->key = successor->key;
            node->right = erase(std::move(node->right), node->key);
        }
        refresh(node.get());
        const int balance = balance_factor(node);
        if (balance > 1) {
            if (balance_factor(node->right) < 0) {
                node->right = rotate_right(std::move(node->right));
            }
            return rotate_left(std::move(node));
        }
        if (balance < -1) {
            if (balance_factor(node->left) > 0) {
                node->left = rotate_left(std::move(node->left));
            }
            return rotate_right(std::move(node));
        }
        return node;
    }

    template <typename Visitor>
    static void inorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) {
            return;
        }
        inorder_impl(node->left.get(), visit);
        visit(node->key);
        inorder_impl(node->right.get(), visit);
    }

    std::unique_ptr<Node> root_;
    std::size_t size_{0};
};
// <<< avl-tree

// >>> splay-tree
/// 伸展树：不存平衡因子，每次访问之后把被访问的键旋到根附近。
///
/// 「最近用过的下次更容易先碰到」，代价是**均摊** $O(\log n)$——单次操作可能很慢，
/// 但一串操作平摊下来不会差（摊还的含义见 2.2.2 节）。
class SplayTree {
public:
    void insert(int key) { insert_node(root_, key); }

    /// 查找会**改变树形**：命中后该键被旋到根，所以这个函数不是 const。
    [[nodiscard]] bool contains(int key) {
        splay(root_, key);
        return root_ && root_->key == key;
    }

    /// 根上的键。用它可以直接断言「刚访问过的键确实被搬到了根」。
    [[nodiscard]] int root_key() const { return root_ ? root_->key : 0; }

    [[nodiscard]] bool empty() const noexcept { return !root_; }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    template <typename Visitor>
    void inorder(Visitor&& visit) const {
        inorder_impl(root_.get(), visit);
    }

private:
    struct Node {
        int key;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        explicit Node(int k) : key(k) {}
    };

    static void rotate_left(std::unique_ptr<Node>& t) {
        auto r = std::move(t->right);
        t->right = std::move(r->left);
        r->left = std::move(t);
        t = std::move(r);
    }

    static void rotate_right(std::unique_ptr<Node>& t) {
        auto l = std::move(t->left);
        t->left = std::move(l->right);
        l->right = std::move(t);
        t = std::move(l);
    }

    /// 把 key（或最接近它的键）旋到 t 的根。
    /// 祖父—父—自己三代同向时先转祖父那一层（一字形），异向时转两次（之字形）。
    static void splay(std::unique_ptr<Node>& t, int key) {
        if (!t || t->key == key) {
            return;
        }
        if (key < t->key) {
            if (!t->left) {
                return;
            }
            if (key < t->left->key) {          // 左-左，一字形
                splay(t->left->left, key);
                rotate_right(t);
            } else if (key > t->left->key) {   // 左-右，之字形
                splay(t->left->right, key);
                if (t->left->right) {
                    rotate_left(t->left);
                }
            }
            if (t->left) {
                rotate_right(t);
            }
        } else {
            if (!t->right) {
                return;
            }
            if (key > t->right->key) {         // 右-右，一字形
                splay(t->right->right, key);
                rotate_left(t);
            } else if (key < t->right->key) {  // 右-左，之字形
                splay(t->right->left, key);
                if (t->right->left) {
                    rotate_right(t->right);
                }
            }
            if (t->right) {
                rotate_left(t);
            }
        }
    }

    void insert_node(std::unique_ptr<Node>& node, int key) {
        if (!node) {
            node = std::make_unique<Node>(key);
            ++size_;
            return;
        }
        splay(node, key);
        if (node->key == key) {
            return;  // 已经在树里
        }
        auto fresh = std::make_unique<Node>(key);
        if (key < node->key) {
            fresh->right = std::move(node);
            fresh->left = std::move(fresh->right->left);
        } else {
            fresh->left = std::move(node);
            fresh->right = std::move(fresh->left->right);
        }
        node = std::move(fresh);
        ++size_;
    }

    template <typename Visitor>
    static void inorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) {
            return;
        }
        inorder_impl(node->left.get(), visit);
        visit(node->key);
        inorder_impl(node->right.get(), visit);
    }

    std::unique_ptr<Node> root_;
    std::size_t size_{0};
};
// <<< splay-tree

}  // namespace dsa::advanced
