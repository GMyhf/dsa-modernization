# 12.4.2 平衡的二叉搜索树 / 12.4.3 伸展树

## 原书给了什么

原书 12.4.2 与 12.4.3 只有文字和图示。第 12 章的 `【算法】`/`【代码】` 清单一共只有
算法12.1、算法12.2 两条，都归 `code/ch12/optimal_bst`。所以本单元不认领任何原书清单，
靠 `beyond_book` 说明来历（D-008），105 的等式不变。

## 参考资料里的那份 AVL 用不了

课程资料 `ref_数据结构与算法A 2021秋/SourceCodes/Chap12_AdvDS/AdvTree/` 下有 AVLTree 和
SplayTree，但都不能直接用：

```text
$ cd "ref_数据结构与算法A 2021秋/SourceCodes/Chap12_AdvDS/AdvTree/AVLTree"
$ g++ -std=c++17 -fsyntax-only Example.cpp
AVLNode.h:45:20: error: redeclaration of ‘avlNode<T>::avlNode(T, avlNode<T>*, avlNode<T>*, int)’
                 may not have default arguments [-fpermissive]
```

模板成员函数在**类外定义处又给了一次默认参数**，C++ 不允许。同目录的 SplayTree 只有
`Splay.h` 和 `splay.cpp`，连 `Example.cpp` 都没有，跑不起来。所以这个单元从零写。

## 实现要点

- **AVL 的四种旋转**按「新结点插在哪个方向」分派：LL 右旋、RR 左旋、LR 先左后右、
  RL 先右后左。**删除的判据不同**：删除没有新键，只能按子树的平衡因子决定方向——
  这是最容易照抄插入代码写错的一处。
- **`inorder()` 是检验旋转的关键接口**。旋转改变树形，但 BST 的中序序列**必须一字不差**；
  没有这个接口，「旋转有没有写错」只能靠看。原来的测试没有它，所以书稿里
  「测试检查中序有序」那句话当时是不成立的，本轮一并补上。
- **伸展树的 `contains` 不是 `const`**：查找会把命中的键旋到根，树形因此改变。
  接口如实反映这一点，而不是用 `mutable` 把它藏起来。
- 子树用 `std::unique_ptr`。树高是 $O(\log n)$，递归释放不构成栈风险——
  这正是 D-001 §2b 判据表里「树」那一行的情形，与第 2 章链式结构不同。

## 可复现的证据

```text
$ cd code/ch12/balanced_trees
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
      -fsanitize=address,undefined -fno-sanitize-recover=all test.cpp -o /tmp/bt
$ /tmp/bt
BalancedTrees: 36 项断言，0 失败
```

**变异自检**（三处，都见红）：

| 把实现改回哪种错法 | 后果 |
| --- | --- |
| 去掉 LR 的第一步预旋转 | `FAIL: LR：3,1,2 先左旋再右旋` |
| 删除后不再平衡 | 段错误，退出码 139 |
| 伸展的最后一步不旋转 | 4 条红，含「命中的键被旋到根」和两条中序序列断言 |

## 验证边界

- 36 项断言，`-Werror` + ASan/UBSan 与 `-O2` 双档通过。
- 随机压测：60 轮 × 300 步随机插删，逐键与 `std::set` 对拍，并检查中序序列、计数
  和 **AVL 高度上界 $h \le 1.4405\log_2(n+2)-0.3277$**；伸展树另有 40 轮插入 + 全量查找对拍。
  另用一份独立的压测程序跑过 80400 次检查，零不一致。
- **不在范围内**：伸展树**没有删除**；两棵树都只支持 `int` 键，没有做成模板；
  没有实现红黑树（第 11.5 节只作对照讨论）。
- 伸展树的 $O(\log n)$ 是**均摊**代价，单次操作可能退化到 $O(n)$；本单元没有测量单次最坏耗时。
- `inorder()` 与析构对树高递归。AVL 有高度上界所以安全；**伸展树没有高度上界**，
  病态访问序列下可能很深，这一点没有实测数字。
