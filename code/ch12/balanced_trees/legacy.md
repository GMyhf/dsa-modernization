# AVL 与伸展树证据

原书相关内容以算法和图示为主，本单元补充可编译的 C++17 实现。

AVL 使用 `unique_ptr` 拥有左右子树，插入和删除后更新高度并执行 LL、RR、LR、RL 旋转。
伸展树同样由 `unique_ptr` 拥有子树，旋转函数通过移动所有权完成一字形和之字形调整。

可复现验证：

```text
$ python3 tools/check_code.py code/ch12/balanced_trees --allow-degraded
BalancedTrees: 4 checks, 0 failures
```

测试覆盖 AVL 高度、查找、删除以及伸展树访问后根结点变化。Sanitizer 受当前 macOS
`sanitizer_malloc_mac.inc:189` 空探针故障影响，Release-O2 已执行。

AVL 的插入序列覆盖外侧和内侧失衡，删除覆盖有两个孩子的结点。
测试还确认重复键不会增加结点，空树查找返回 false。
伸展树的访问测试确认命中的键会移动到根，连续访问不同键仍保持二叉搜索顺序。
左右旋转只移动拥有关系，不复制结点，也不调用 STL 树容器。
这些证据对应本单元的实现范围，不声称覆盖红黑树或持久化索引。
