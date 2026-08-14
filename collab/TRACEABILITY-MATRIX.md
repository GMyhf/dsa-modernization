# 参考资料逐项追踪矩阵

本表用于区分三种情况：

- **参考题型**：题目来自 `ref_DSA` 或邹磊课程资料，答案按本书接口重新编写。
- **教材重设计**：题目根据张铭教材和本书章节目标重新组织，不声称来自参考资料原题。
- **代码证明**：对应实现有 `code/` 测试或书稿闸门实际运行记录。

“已修正”只表示问题已经在现代化教材、代码或勘误说明中吸收；对于原书图示/排版错误，不表示
新 PDF 仍逐字复刻并修改了原图。

## A. 《教材 1–6 章勘误表》

| 来源 | 题目/勘误编号 | 新书位置 | 是否已修正 | 测试或答案依据 |
| --- | --- | --- | --- | --- |
| 邹磊《教材 1–6 章勘误表》 | 错误 0：1.4.2 连续有序子数组长度条件 | `book/ch01-adt.md`；`book/勘误.md` | 是，已吸收 | 第 1 章改用半开区间并明确长度；属于文字/公式修正，非独立代码单元 |
| 同上 | 错误 1：“有序子数组”应为连续有序子数组 | `book/ch01-adt.md`；`book/勘误.md` | 是，已吸收 | 第 1 章文字定义；答案附录第 1 章第 3 题 |
| 同上 | 错误 2：参数条件应为 `a>1`、`b>1` | `book/勘误.md` | 部分适用 | 本书未复刻该原书公式代码；已在勘误摘要保留，不对应现有测试 |
| 同上 | 错误 3：第 2.3 节标点 | `book/勘误.md` | 不适用 | 新书重写该段，原标点不存在；无代码测试 |
| 同上 | 错误 4：算法 2.9 模板拼写、`p` 初始化 | `book/ch02-linear-list.md`；`code/ch02/linked_list/modern.hpp` | 是，已吸收 | `code/ch02/linked_list/test.cpp`：链表构造、遍历、插入删除、拷贝移动测试 |
| 同上 | 错误 5：`abs` 应为 `fabs` | `book/勘误.md` | 不适用 | 新书没有该原始浮点代码；无现行实现可测 |
| 同上 | 错误 6：习题递推式 `T(n)=2T(floor(n/2))+n` | `book/ch01-adt.md`；`book/习题与参考答案.md` | 是，已吸收 | 答案给出递推树结论 `Theta(n log n)`；属于数学答案，不是运行时测试 |
| 同上 | 错误 7：算法 3.11 `while` 条件 | `book/ch03-stack.md`；`code/ch03/knapsack/modern.hpp` | 是，已重写 | `code/ch03/knapsack/test.cpp`：递归、显式栈和优化版对拍 |
| 同上 | 错误 8：嵌套循环赋值次数说明 | `book/勘误.md` | 不适用 | 新书未保留该段原文；无现行测试 |
| 同上 | 错误 9：算法 2.9 `while` 后多余分号 | `book/ch02-linear-list.md`；`code/ch02/linked_list/modern.hpp` | 是，已吸收 | 现代实现重新编写；链表单元 Release-O2 通过 |
| 同上 | 错误 10：算法 4.6 返回 `j-pLen` | `book/ch04-string.md`；`code/ch04/pattern_matching/modern.hpp` | 是，已修正 | `code/ch04/pattern_matching/test.cpp` 与 `std::string::find` 对拍 |
| 同上 | 错误 11：KMP 文字说明 `Pi=Pk` | `book/ch04-string.md` | 是，已吸收 | next 表构造说明与 `build_next` 实现一致；测试覆盖随机模式 |
| 同上 | 错误 12：算法 4.7 `while(i<m-1)` / 删除多余 break | `book/ch04-string.md`；`modern.hpp#build-next` | 是，已重写 | 空模式、重复字符和随机模式测试 |
| 同上 | 错误 13：算法 4.8 返回 `j-pLen` | `book/ch04-string.md`；`modern.hpp#kmp` | 是，已修正 | KMP 与朴素算法逐项对拍，56 项断言 |
| 同上 | 错误 14：字符串长度和图示 size | `book/ch04-string.md` | 已重绘/吸收 | 新书用 `String` 实现和测试，不复刻原图中的旧 size 标注 |
| 同上 | 错误 15：Parent 函数缺 `return NULL` | `book/ch05-binary-tree.md`；`code/ch05/binary_tree/modern.hpp` | 是，已吸收 | BST/二叉树测试覆盖空树和不存在结点查询 |
| 同上 | 错误 16：图 6.6 G 子结点编号 | `book/ch06-tree.md` | 已重绘/吸收 | 新书以左孩子/右兄弟图和 `GeneralTree` 测试为准 |
| 同上 | 错误 17：图 6.6 C 子结点编号 | `book/ch06-tree.md` | 已重绘/吸收 | `code/ch06/general_tree/test.cpp`：树结构和周游断言 |

## B. `ref_DSA` 与邹磊课程资料中的题型

| 来源 | 题目/勘误编号 | 新书位置 | 是否已修正 | 测试或答案依据 |
| --- | --- | --- | --- | --- |
| `ref_DSA/homework/2.md` | 有序表原地删除重复元素 | `book/习题与参考答案.md` 第 2 章习题 2 | 已纳入参考题型 | 双指针答案：`O(n)` 时间、`O(1)` 额外空间；当前 `ArrayList` 单元测试覆盖插入删除，但未单独实现去重函数 |
| `ref_DSA/homework/2.md` | 单链表判环并找入口 | `book/习题与参考答案.md` 第 2 章习题 3 | 已纳入参考题型 | Floyd 快慢指针答案；当前链表代码未提供该独立 API，答案不是代码闸门证明 |
| `ref_DSA/homework/3.md` | 两栈实现队列/撤销恢复 | `book/习题与参考答案.md` 第 3 章习题 1、3 | 已纳入参考题型 | 摊还 `O(1)` 的双栈说明；队列单元测试证明的是本书循环队列/链式队列，不是双栈队列 |
| `ref_DSA/homework/3.md` | 栈式车站合法序列 | `book/习题与参考答案.md` 第 3 章习题 2 | 已纳入参考题型 | Catalan 数公式；无独立程序测试 |
| `ref_DSA/homework/4字符串.md` | next 数组、字符串消除、循环移动 | `book/习题与参考答案.md` 第 4 章习题 1–3 | 已纳入参考题型 | KMP 单元测试证明匹配实现；消除/循环移动仅给出答案思路 |
| `ref_DSA/homework/5二叉树.md` | 二叉树序列重建 | `book/习题与参考答案.md` 第 5 章习题 1 | 已纳入参考题型 | 递归切分先序/中序的答案；当前代码测试周游，但未提供通用重建 API |
| `ref_DSA/homework/5二叉树.md` | Huffman 199 结点叶子数、编码 | `book/习题与参考答案.md` 第 5 章习题 2、4 | 已纳入参考题型 | 满二叉树公式；Huffman 单元测试证明堆合并和权重检查，未固定某组编码的唯一形式 |
| `ref_DSA/homework/5二叉树.md` | 最小堆建堆和最大值位置 | `book/习题与参考答案.md` 第 5 章习题 3 | 已纳入参考题型 | 堆实现测试覆盖插入、删除最小值和扩容 |
| `ref_DSA/Final/07graph.md` | Dijkstra 是否产生 MST | `book/习题与参考答案.md` 第 7 章习题 2 | 已纳入参考题型 | 三顶点反例；Graph 单元测试分别验证 Dijkstra 和 Prim/Kruskal，不把二者混同 |
| `ref_DSA/Final/07graph.md` | DFS/BFS、拓扑、MST 复杂度 | `book/ch07-graph.md`；答案附录第 7 章 | 已吸收 | 当前矩阵实现测试通过；书稿明确矩阵 `O(V^2)`、Floyd `O(V^3)`，邻接表结论标为条件化说明 |
| `ref_DSA/homework/08内排序.md` | 快排划分、桶/基数排序、子集和排序 | `book/习题与参考答案.md` 第 8 章 | 部分纳入 | 快排/基数排序有 `code/ch08/sorting/test.cpp`；子集和题仅作为参考题型，未加入正文代码 |
| `ref_DSA/DSA-HW/Chapter9.md` | 置换选择和 loser tree 顺串 | `book/习题与参考答案.md` 第 9 章 | 已纳入参考题型 | `code/ch09/external_sort/test.cpp` 验证外部排序模拟；答案保留磁盘 I/O 代价说明 |
| `ref_DSA/homework/12高级数据结构.md` | 稀疏矩阵 CSR、Trie/Patricia 空间 | `book/ch12-advanced.md`；答案附录第 12 章 | 概念已纳入 | 第 12 章明确这些是概念导读；当前没有 CSR/Trie 完整实现或代码测试 |
| `ref_DSA/DSA-HW/Chapter10.md` | 堆排序与二分检索组合复杂度 | `book/习题与参考答案.md` 第 10 章 | 已纳入参考题型 | 复杂度按调用次数相乘；检索/散列单元测试不等于该组合题的独立证明 |

## C. 本书自行重设计的题目

| 来源 | 题目/勘误编号 | 新书位置 | 是否已修正 | 测试或答案依据 |
| --- | --- | --- | --- | --- |
| 张铭教材 + 本书重编 | 第 1 章股市传言起点选择 | `book/ch01-adt.md`；`code/ch01/adt` | 是 | Floyd 结果、B3 输出和 7 项断言 |
| 张铭教材 + 本书重编 | `optional` 表达检索失败 | 各章接口说明 | 是 | `-Werror` 构建和各单元空结果断言 |
| 张铭教材 + 本书重编 | RAII/五法则/移动语义重写 | 第 2、4、5、7、9 章 | 是 | 对应单元的拷贝、移动、异常路径测试；sanitizer 在当前 macOS 环境降级跳过 |
| 张铭教材 + 本书重编 | 散列表墓碑删除 | `book/ch10-search.md`；`code/ch10/search_hash` | 是 | 39 项断言，包含删除后继续查找和墓碑复用 |
| 张铭教材 + 本书重编 | 递归、显式栈、优化版背包对照 | `book/ch03-stack.md`；`code/ch03/knapsack` | 是 | 54 项断言，与暴力枚举对拍 |

## 使用规则

本矩阵不把“有参考答案”写成“已有代码验证”。以后新增题目或修正时，必须同时填写来源、新书
位置和依据；若没有可执行测试，应明确写“答案依据”或“概念依据”。

`book/习题与参考答案.md` 的每条核心答案现已直接标记 `邹磊课程答案`、`ref_DSA`、`本书自拟`
或 `本书代码验证`。如果参考答案存在可复现错误，新书保留来源并写明校正，不以“忠实引用”为由
保留错误答案。
