// 第 5 章「先跑一遍」：用教学版 BinaryTree 与 BinarySearchTree 走一遍四种周游与增删查。
// 编译运行：
//   g++ -std=c++17 -I code/ch05/binary_tree code/ch05/binary_tree/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    // 建一棵样例树：A 的左孩子 B（孩子 D、E），右孩子 C（叶子）
    BinaryTree<char> d, e, b, c, root;
    d.create_leaf('D');
    e.create_leaf('E');
    b.create_tree('B', d, e);      // d、e 的所有权转移给 b，之后两者变空
    c.create_leaf('C');
    root.create_tree('A', b, c);

    std::cout << "先序: ";
    root.preorder([](char value) { std::cout << value; });
    std::cout << "\n中序: ";
    root.inorder([](char value) { std::cout << value; });
    std::cout << "\n后序: ";
    root.postorder([](char value) { std::cout << value; });
    std::cout << "\n层次: ";
    root.level_order([](char value) { std::cout << value; });
    std::cout << '\n';

    BinarySearchTree<int> tree;
    for (int key : {8, 3, 10, 1, 6, 14, 4, 7}) {
        (void)tree.insert(key);
    }
    std::cout << "BST 中序:";
    tree.inorder([](int key) { std::cout << ' ' << key; });   // 中序 = 升序
    std::cout << "\n含 6? " << (tree.contains(6) ? "是" : "否")
              << "  删 3 后含 3? ";
    (void)tree.remove(3);
    std::cout << (tree.contains(3) ? "是" : "否") << '\n';
}
