// 二叉树与二叉搜索树 —— 教学版。
// 原书【代码5.1】【代码5.2】【算法5.3】【算法5.7】【算法5.9】【算法5.10】。
//
// 一个文件、两个类、能直接编译运行，给「第一次读这一节」的人看。
//
//   BinaryTree         二叉链表：一个结点，两根指向孩子的链接；深度优先与层次周游。
//   BinarySearchTree   在二叉树上加一条「左小右大」的约束，于是查找变成一路往下走。
//
// **递归是这一章的正题**，所以周游、释放、深拷贝这里全部写成递归——形状和定义一样，
// 一眼就能对上。代价是递归深度等于树高：退化成一条链的树（比如按升序插入 BST）
// 会把运行栈压穿。工程版把释放和深拷贝改成了迭代，见 5.x「进阶（选读）」。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则 + 全递归；
//   工程版  五法则、迭代释放（右旋拉直）、迭代深拷贝、强异常保证、非递归周游。
#pragma once

#include <cstddef>

// ---------------------------------------------------------------------------
// 二叉树（二叉链表）
// ---------------------------------------------------------------------------
template <typename T>
class BinaryTree {
public:
    // 【代码5.1】二叉树结点：一个数据域 + 左右两根链接。
    // 没有孩子就是 nullptr——这比原书用一个"空结点"表示要省事得多。
    struct Node {
        T value;
        Node* left;
        Node* right;
    };

    BinaryTree() : root_(nullptr) {}

    ~BinaryTree() { clear(); }

    // 三法则：树管着一堆 new 出来的结点，拷贝必须自己写（而且必须是**深**拷贝）。
    BinaryTree(const BinaryTree& other) : root_(clone(other.root_)) {}

    BinaryTree& operator=(const BinaryTree& other) {
        if (this == &other) {
            return *this;
        }
        Node* fresh = clone(other.root_);   // 先把新树建好
        clear();                            // 再拆掉旧树
        root_ = fresh;
        return *this;
    }

    bool empty() const { return root_ == nullptr; }
    const Node* root() const { return root_; }
    Node* root() { return root_; }

    // 造一棵新树：一个根，接上左右两棵子树。
    // 两棵子树的所有权**转移**给新树——传进来的那两棵随即变空，
    // 否则同一批结点会被两棵树各删一次。
    void create_tree(const T& value, BinaryTree& left, BinaryTree& right) {
        Node* fresh = new Node{value, left.root_, right.root_};
        left.root_ = nullptr;
        right.root_ = nullptr;
        clear();
        root_ = fresh;
    }

    void create_leaf(const T& value) {
        Node* fresh = new Node{value, nullptr, nullptr};
        clear();
        root_ = fresh;
    }

    // 【算法5.3】深度优先周游的三种次序。三个函数只差 visit 那一行的位置：
    //   前序 根左右 · 中序 左根右 · 后序 左右根
    template <typename Visitor>
    void preorder(Visitor visit) const { preorder_impl(root_, visit); }

    template <typename Visitor>
    void inorder(Visitor visit) const { inorder_impl(root_, visit); }

    template <typename Visitor>
    void postorder(Visitor visit) const { postorder_impl(root_, visit); }

    // 【算法5.7】层次周游：一层一层从左到右。
    // 深度优先靠栈（这里是递归用的运行栈），广度优先靠**队列**。
    template <typename Visitor>
    void level_order(Visitor visit) const {
        // 一条极简的链式队列，只在这个函数里用
        struct Pending {
            const Node* node;
            Pending* next;
        };
        Pending* front = nullptr;
        Pending* rear = nullptr;

        if (root_ != nullptr) {
            front = rear = new Pending{root_, nullptr};
        }
        while (front != nullptr) {
            Pending* item = front;               // 出队
            front = front->next;
            if (front == nullptr) {
                rear = nullptr;
            }
            const Node* node = item->node;
            delete item;

            visit(node->value);

            if (node->left != nullptr) {         // 左右孩子依次入队
                Pending* fresh = new Pending{node->left, nullptr};
                if (rear == nullptr) { front = rear = fresh; } else { rear->next = fresh; rear = fresh; }
            }
            if (node->right != nullptr) {
                Pending* fresh = new Pending{node->right, nullptr};
                if (rear == nullptr) { front = rear = fresh; } else { rear->next = fresh; rear = fresh; }
            }
        }
    }

    // 结点数与高度：两个最典型的「先算孩子、再算自己」的递归。
    std::size_t size() const { return count(root_); }
    std::size_t height() const { return depth(root_); }

    void clear() {
        destroy(root_);
        root_ = nullptr;
    }

private:
    // 【代码5.8】后序释放：**必须先删两个孩子，再删自己**。
    // 反过来先 delete node，node->left 就成了读已释放内存。
    static void destroy(Node* node) {
        if (node == nullptr) {
            return;
        }
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    // 深拷贝：形状和 destroy 一样，只是把"删"换成"建"。
    static Node* clone(const Node* node) {
        if (node == nullptr) {
            return nullptr;
        }
        return new Node{node->value, clone(node->left), clone(node->right)};
    }

    template <typename Visitor>
    static void preorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) return;
        visit(node->value);                 // 根
        preorder_impl(node->left, visit);   // 左
        preorder_impl(node->right, visit);  // 右
    }

    template <typename Visitor>
    static void inorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) return;
        inorder_impl(node->left, visit);    // 左
        visit(node->value);                 // 根
        inorder_impl(node->right, visit);   // 右
    }

    template <typename Visitor>
    static void postorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) return;
        postorder_impl(node->left, visit);  // 左
        postorder_impl(node->right, visit); // 右
        visit(node->value);                 // 根
    }

    static std::size_t count(const Node* node) {
        return node == nullptr ? 0 : 1 + count(node->left) + count(node->right);
    }

    static std::size_t depth(const Node* node) {
        if (node == nullptr) return 0;
        std::size_t l = depth(node->left);
        std::size_t r = depth(node->right);
        return 1 + (l > r ? l : r);
    }

    Node* root_;
};

// ---------------------------------------------------------------------------
// 二叉搜索树
//
// 约束只有一条：**左子树的键都小于根，右子树的键都大于根**。
// 有了它，查找就不必遍历全树——每比较一次就砍掉一半（前提是树是平衡的）。
// ---------------------------------------------------------------------------
template <typename T>
class BinarySearchTree {
public:
    struct Node {
        T value;
        Node* left;
        Node* right;
    };

    BinarySearchTree() : root_(nullptr) {}

    ~BinarySearchTree() { clear(); }

    BinarySearchTree(const BinarySearchTree& other) : root_(clone(other.root_)) {}

    BinarySearchTree& operator=(const BinarySearchTree& other) {
        if (this == &other) {
            return *this;
        }
        Node* fresh = clone(other.root_);
        clear();
        root_ = fresh;
        return *this;
    }

    bool empty() const { return root_ == nullptr; }

    // 【算法5.9】插入。一路比较着往下走，走到空位就把新结点挂上去。
    // 键已存在时返回 false——重复键是可预期状态，不是错误，所以不抛异常。
    bool insert(const T& value) {
        Node** link = &root_;               // 指向「新结点该挂在哪根指针上」
        while (*link != nullptr) {
            if (value < (*link)->value) {
                link = &(*link)->left;
            } else if ((*link)->value < value) {
                link = &(*link)->right;
            } else {
                return false;               // 已经有了
            }
        }
        *link = new Node{value, nullptr, nullptr};
        return true;
    }

    // 查找：同样一路往下走。树高是 h，代价就是 O(h)。
    bool contains(const T& value) const {
        const Node* current = root_;
        while (current != nullptr) {
            if (value < current->value) {
                current = current->left;
            } else if (current->value < value) {
                current = current->right;
            } else {
                return true;
            }
        }
        return false;
    }

    // 【算法5.10】删除。键不存在返回 false（幂等，不是错误）。
    //
    // 难点只有一个：被删结点有两个孩子时，谁来顶替它？
    // 答案是**中序前驱**——左子树里最大的那个。它顶上来之后，
    // 「左小右大」仍然成立，因为它比左子树其余的都大、比右子树全部都小。
    bool remove(const T& value) { return remove_impl(root_, value); }

    void clear() {
        destroy(root_);
        root_ = nullptr;
    }

    // 中序周游一棵 BST，得到的正是**升序**——这是「左小右大」的直接推论。
    template <typename Visitor>
    void inorder(Visitor visit) const { inorder_impl(root_, visit); }

private:
    static bool remove_impl(Node*& link, const T& value) {
        if (link == nullptr) {
            return false;
        }
        if (value < link->value) {
            return remove_impl(link->left, value);
        }
        if (link->value < value) {
            return remove_impl(link->right, value);
        }

        Node* removed = link;
        if (removed->left == nullptr) {          // 没有左孩子：右孩子直接顶上
            link = removed->right;
            delete removed;
            return true;
        }

        // 找中序前驱：从左孩子出发，一路向右走到底
        Node** predecessor_link = &removed->left;
        while ((*predecessor_link)->right != nullptr) {
            predecessor_link = &(*predecessor_link)->right;
        }
        Node* replacement = *predecessor_link;
        *predecessor_link = replacement->left;   // 前驱可能还有左孩子，先接走
        replacement->left = removed->left;
        replacement->right = removed->right;
        link = replacement;
        delete removed;
        return true;
    }

    static void destroy(Node* node) {
        if (node == nullptr) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    static Node* clone(const Node* node) {
        if (node == nullptr) return nullptr;
        return new Node{node->value, clone(node->left), clone(node->right)};
    }

    template <typename Visitor>
    static void inorder_impl(const Node* node, Visitor& visit) {
        if (node == nullptr) return;
        inorder_impl(node->left, visit);
        visit(node->value);
        inorder_impl(node->right, visit);
    }

    Node* root_;
};
