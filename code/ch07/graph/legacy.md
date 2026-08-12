# 第7章图
图用邻接矩阵统一承载原书矩阵和邻接表 API 的算法语义。DFS 保留递归，深图有 Stack Overflow Risk；BFS/拓扑使用工作队列。Dijkstra 拒绝负权；拓扑与 MST 对环/非连通图返回 optional，而不是打印或返回未初始化数组。
