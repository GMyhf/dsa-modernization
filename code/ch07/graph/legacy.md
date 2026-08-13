# 原书写法 → 问题 → 现代写法：第 7 章图

覆盖代码7.1–7.4、算法7.5–7.11；原文范围 `dsa_raw.md:5780-6591`。现代 `Graph` 采用
邻接矩阵，使 DFS/BFS、拓扑、最短路和 MST 共享同一可验证的边语义。

## 清单映射

代码7.1/7.2 的图与边 ADT 对应 `Graph`/`Edge`；代码7.3/7.4 的矩阵和邻接表语义由
`add_edge` 的有向/无向选择承载。算法7.5–7.11 分别为 `dfs`、`bfs`、`topological_sort`、
`dijkstra`、`floyd`、`prim`、`kruskal`。

## 原书可复核问题

1. **代码7.1 不是完整类定义。** `bool IsEdge(Edge oneEdge)` 缺函数体开花括号，随后又有
   孤立分号；多个声明没有闭合花括号。OCR 损伤必须与图 ADT 设计分开记录。
2. **代码7.2 多处标识符被空格切断。** `num Vertex = num Vert` 与 `~ Graph()` 都不是可编译
   C++；它还用裸数组却没给复制控制，默认拷贝会双重释放 Mark/Indegree。
3. **代码7.3 的矩阵申请有全角分号，且边权 0 被同时当成“无边”。** 这让零权边无法表示；
   现代用 `infinity` 表示无边，允许零权边，并在 Dijkstra 前拒绝负权。
4. **代码7.4 链表节点没有析构与所有权说明。** 更新边、删除边和图析构的组合无法从原文证明
   不泄漏。现代先采用 RAII 矩阵；稀疏邻接表是同一 API 的可替换表示，不假称 OCR 片段已安全。
5. **算法7.6 / 7.9 缺结束标记。** 2026-08-13 人工定界（T-008），不改 `dsa_raw.md`：
   - **算法7.6** 在 `6176` 开。代码写到 `6189` 的 `if( G. Mark[ G. ToVertex(e) ] = = UNVISITED) {`
     后被一张图打断（6191），接着是「广度优先搜索实质上与深度优先相同」的收束段，
     `6195` 已是新小节「7.4.3 拓扑排序」。函数体后半（访问邻接点、入队、循环结束）
     被 OCR 连同结束标记一起吃掉。切片收到 **6189**：之后不是本算法的语句。
   - **算法7.9** 在 `6401` 开。三重循环与松弛写到 `6433` 的孤立 `1`（本应是 `}`），
     下一行是残缺标记 `.9结束】`（6434），再下一行是 `## 7.6 最小生成树`。
     切片收到 **6434**：这是结束标记被吃剩的残片，其后为主题切换。
6. **算法7.8/7.10/7.11 把失败打印到 cout 或留下裸数组。** 非连通图、负权和有环拓扑都是
   正常结果状态，现代返回 optional 或 infinity，不混入控制台。

## 真实编译器证据

代码7.2 的标识符空格可缩为：

```text
$ printf 'int main(){int num Vertex=0;}\n' | clang++ -std=c++17 -x c++ - -c
<stdin>:1:20: error: expected ';' at end of declaration
```

代码7.3 的全角分号同样会报告：`error: character <U+FF1B> not allowed in an identifier`。

## 验证

```text
$ python3 tools/check_code.py code/ch07/graph --allow-degraded
Graph: 40 项断言，0 失败
```

五个源点的 Dijkstra 逐项与 Floyd 对拍；Prim/Kruskal 比较 n-1 条边和总权 7；环、非连通、
非法顶点与负权均有回归。DFS 是递归，深图存在 Stack Overflow Risk；macOS ASan 未启动，完整
递归与内存审计待 Claude 补跑。
