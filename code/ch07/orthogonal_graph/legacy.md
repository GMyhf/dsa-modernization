# 十字链表的传统教学表示

## 原书与截图的边界

原书 7.3.3 没有独立的 `【算法】` 或 `【代码】` 清单，只有图 7.15 与字段说明。
截图中的例子采用传统教材常见的 `ArcBox`、`VexNode`、`firstin`、`firstout` 和表头插入。
因此本单元不把截图伪称为原书逐字清单；`teaching.hpp` 是按该表示法写成的可运行教学版。

教学版故意保留裸链接，读者能直接看到一条弧分别接进出链和入链的两次赋值。
它不是工程版：结点通过 `new` 创建，释放顺序也必须由 `clear()` 明写出来。
但是教学版补了原始短例常省略的析构，并删除复制构造和复制赋值，不能浅拷贝。
否则两个图对象会持有同一批 `ArcBox`，析构时必然重复 `delete`。

## 现代版的变化

现代版仍把 `tailnextarc`、`headnextarc` 留成裸指针，因为它们表达的是两条交叉的非拥有链接。
唯一所有权集中在 `arcs_` 的 `std::unique_ptr<Arc>` 中：每条弧只拥有一次。
`remove_edge` 先从尾点出链摘除，再从头点入链摘除，最后删除唯一所有者。
若先释放而没有摘两条链，下一次反向遍历会解引用悬空指针。
若只摘其中一条，正向或反向查询会得到不一致的图。

## 可复现的证据

```text
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 teaching_test.cpp -o /tmp/orthogonal-teaching
$ /tmp/orthogonal-teaching
十字链表(原书式教学版)：50 项断言，0 失败
$ python3 tools/check_code.py code/ch07/orthogonal_graph --allow-degraded
code/ch07/orthogonal_graph  «十字链表：一条弧进入两条链»
  ✅ [release-O2] 十字链表：12 项断言，0 失败
  ✅ [release-O2/teaching] 十字链表(原书式教学版)：50 项断言，0 失败
```

变异自检：把 `remove_edge` 中的入链摘除赋值删掉，
`删除从 head 的入链摘除` 会失败；把教学版的 `headnextarc` 改成 `tailnextarc`，
`表头插入后 headnextarc 是 3、0` 会失败。两者分别保护两条链，
不是只在“从尾点出发”这一条路径上碰巧测到。

本机 ASan 空探针受 `sanitizer_malloc_mac.inc:189` 的环境问题限制，
上述降级检查不覆盖内存和未定义行为；Release 的行为断言不把这一限制伪装成通过。
