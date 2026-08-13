#include "modern.hpp"

#include <iostream>
#include <utility>

int main() {
    dsa::BinaryTree<char> left_leaf;
    dsa::BinaryTree<char> right_leaf;
    dsa::BinaryTree<char> left;
    dsa::BinaryTree<char> right;
    dsa::BinaryTree<char> root;
    left_leaf.create_tree('D');
    right_leaf.create_tree('E');
    left.create_tree('B', std::move(left_leaf), std::move(right_leaf));
    right.create_tree('C');
    root.create_tree('A', std::move(left), std::move(right));

    std::cout << "先序: ";
    root.preorder([](char value) { std::cout << value; });
    std::cout << "\n中序: ";
    root.inorder([](char value) { std::cout << value; });
    std::cout << "\n后序: ";
    root.postorder([](char value) { std::cout << value; });
    std::cout << "\n层次: ";
    root.level_order([](char value) { std::cout << value; });
    std::cout << '\n';

    dsa::BinarySearchTree<int> tree;
    for (int key : {8, 3, 10, 1, 6, 14, 4, 7}) {
        (void)tree.insert(key);
    }
    std::cout << "BST 中序:";
    tree.inorder([](int key) { std::cout << ' ' << key; });
    std::cout << "\n含 6? " << (tree.contains(6) ? "是" : "否")
              << "  删 3 后含 3? ";
    (void)tree.remove(3);
    std::cout << (tree.contains(3) ? "是" : "否") << '\n';
}
