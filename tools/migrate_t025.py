#!/usr/bin/env python3
"""Install the hand-reviewed T-025 implementation/test bindings."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Each tuple is (listing id, implementation symbol, behavioural test label).
BINDINGS = {
"ch01/adt": [
("算法1.1", "std::optional<std::size_t> best_source() const", "算法1.1 选出 B3"),
("代码1.2", "class RumorNetwork {", "算法1.1 retains lower duplicate edge")],
"ch02/array_list": [
("代码2.1", "class ArrayList {", "被移动方是有效的空表"),
("代码2.2", "ArrayList(const ArrayList& other)", "副本独立增长"),
("算法2.3", "const T& at(size_type index) const", "at/set 越界一律抛 out_of_range"),
("算法2.4", "std::optional<size_type> find(const T& value) const", "find 返回第一次出现的下标"),
("算法2.5", "T remove(size_type pos)", "删除后剩余元素左移就位")],
"ch02/linked_list": [
("代码2.6", "struct SinglyLink {", "代码2.6 单链结点保存数据和后继"),
("代码2.7", "class LinkedList {", "四次插入后长度正确"),
("算法2.8", "const T& at(size_type pos) const", "at 非 const 重载可修改元素"),
("算法2.9", "std::optional<size_type> find(const T& value) const", "算法2.9：每个位置都定位到正确的结点"),
("算法2.10", "void insert_impl(size_type pos, U&& value)", "头部、中间、尾部插入均保持顺序"),
("算法2.11", "T remove(size_type pos)", "删尾后连续 append 仍接在真尾部"),
("代码2.12", "struct DoublyLink {", "代码2.12 双链结点维护前驱和后继")],
"ch03/array_stack": [
("代码3.1", "class ArrayStack {", "默认构造是空栈"),
("代码3.2", "std::optional<T> pop()", "pop 出 3"),
("算法3.3", "void ensure_capacity()", "扩容次数是对数级（翻倍策略生效）")],
"ch03/expression_eval": [("算法3.5", "double evaluate_postfix(std::string_view expression)", "3 4 - = -1")],
"ch03/knapsack": [
("算法3.10", "knapsack_recursive(", "递归版与暴力枚举结论一致"),
("算法3.11", "knapsack_with_explicit_stack(", "显式栈版与暴力枚举结论一致"),
("算法3.12", "knapsack_optimized(", "优化版与暴力枚举结论一致")],
"ch03/linked_stack": [("代码3.4", "class LinkedStack {", "压三个后栈顶是最后压入的")],
"ch03/queue": [
("代码3.13", "class ArrayQueue {", "代码3.13 FIFO across wrap"),
("代码3.14", "bool full() const noexcept", "代码3.14 sacrificed slot detects full"),
("代码3.15", "class LinkedQueue {", "代码3.15 copied queue is independent")],
"ch03/recursion_and_stack": [
("算法3.6", "factorial_recursive(long long n)", "算法3.6：负数一律抛 invalid_argument"),
("算法3.7", "factorial_type factorial_driver(long long n)", "算法3.7 主程序驱动 factorial(4) 得到 24"),
("算法3.8", "factorial_iterative(long long n)", "三种实现在溢出边界上行为一致"),
("算法3.9", "factorial_with_explicit_stack(long long n)", "三种实现互相一致")],
"ch04/pattern_matching": [
("算法4.6", "naive_search(std::string_view text,", "算法4.6：朴素匹配的下标与标准库一致"),
("算法4.7", "build_next(std::string_view pattern)", "算法4.7：next 数组与书中图4.11"),
("算法4.8", "kmp_search(std::string_view text,", "算法4.8：KMP 的下标与标准库一致")],
"ch04/string_class": [
("代码4.1", "class String {", "默认构造是空串"),
("算法4.3", "String& concatenate(const char* s)", "concatenate 接在串尾"),
("算法4.4", "int compare(const String& other) const noexcept", "小于时 compare 为负"),
("算法4.5", "String substr(size_type pos, size_type len) const", "从中间抽取")],
"ch05/binary_tree": [
("代码5.1", "class BinaryTree {", "深拷贝不共享子树"),
("代码5.2", "void create_tree(U&& value", "make_empty 后为空树"),
("算法5.3", "void preorder(Visitor&& visit) const", "算法5.3 前序递归周游"),
("算法5.4", "void preorder_iterative(Visitor&& visit) const", "算法5.4 非递归前序周游"),
("算法5.5", "void inorder_iterative(Visitor&& visit) const", "算法5.5 非递归中序周游"),
("算法5.6", "void postorder_iterative(Visitor&& visit) const", "算法5.6 非递归后序周游"),
("算法5.7", "void level_order(Visitor&& visit) const", "算法5.7 层次周游"),
("代码5.8", "const Node* parent_of(const Node* wanted) const noexcept", "代码5.8 能找到父结点"),
("算法5.9", "bool insert(const T& value)", "算法5.9 插入唯一键"),
("算法5.10", "bool remove(const T& value)", "算法5.10 删除有左子树结点")],
"ch05/heap_huffman": [
("代码5.11", "class MinHeap {", "最小元素按序弹出"),
("代码5.12", "class HuffmanTree {", "Huffman 树总权重")],
"ch06/general_tree": [
("代码6.1", "class GeneralTree {", "代码6.1 deep copy source unchanged"),
("代码6.2", "GeneralTree(const GeneralTree& other)", "代码6.2 self assignment"),
("算法6.3", "void preorder(Visitor&& visitor) const", "算法6.3 preorder"),
("算法6.4", "void postorder(Visitor&& visitor) const", "算法6.4 postorder"),
("算法6.5", "void breadth_first(Visitor&& visitor) const", "算法6.5 breadth first"),
("代码6.6", "Node* insert_first(Node* parent, const T& value)", "代码6.6 insert first prepends"),
("代码6.7", "void delete_subtree(Node* node)", "代码6.7 delete first child reconnects"),
("代码6.8", "class DisjointSet {", "代码6.8 weighted union"),
("算法6.9", "std::size_t find(std::size_t index)", "算法6.9 compressed root"),
("算法6.10", "GeneralTree from_dual_tag(const DualTagNode* nodes", "算法6.10 先根周游还原出原序列")],
"ch07/adjacency_list": [("代码7.4", "class GraphList {", "7.4 邻接表存储量随 V+E 走")],
"ch07/graph": [
("代码7.1", "class Graph {", "代码7.1 vertex count"),
("代码7.2", "struct Edge {", "MST singleton empty"),
("代码7.3", "void add_edge(std::size_t from", "代码7.3 rejects negative Dijkstra weight"),
("算法7.5", "std::vector<std::size_t> dfs(std::size_t source) const", "算法7.5 DFS order"),
("算法7.6", "std::vector<std::size_t> bfs(std::size_t source) const", "算法7.6 BFS order"),
("算法7.7", "topological_sort() const", "算法7.7 topological endpoints"),
("算法7.8", "std::vector<int> dijkstra(std::size_t source) const", "算法7.8 unreachable stays infinity"),
("算法7.9", "std::vector<std::vector<int>> floyd() const", "算法7.9 uses intermediate vertices"),
("算法7.10", "std::optional<std::vector<Edge>> prim", "算法7.10 Prim has n-1 edges"),
("算法7.11", "std::optional<std::vector<Edge>> kruskal() const", "算法7.11 Kruskal has n-1 edges")],
"ch08/sorting": [
("算法8.1", "void insertion_sort(std::vector<int>& values)", "算法8.1 insertion"),
("算法8.2", "void shell_sort(std::vector<int>& values)", "算法8.2 shell"),
("算法8.3", "void selection_sort(std::vector<int>& values)", "算法8.3 selection"),
("算法8.4", "void heap_sort(std::vector<int>& values)", "算法8.4 heap"),
("算法8.5", "void bubble_sort(std::vector<int>& values)", "算法8.5 bubble"),
("算法8.6", "void quick_sort(std::vector<int>& values)", "算法8.6 quick"),
("算法8.7", "void quick_sort_optimized(std::vector<int>& values)", "算法8.7 quick optimized"),
("算法8.8", "void merge_sort(std::vector<int>& values)", "算法8.8 merge"),
("算法8.9", "void merge_sort_optimized(std::vector<int>& values)", "算法8.9 merge optimized"),
("算法8.10", "void counting_sort(std::vector<int>& values)", "算法8.10 counting"),
("算法8.11", "void radix_sort(std::vector<int>& values)", "算法8.11 radix"),
("代码8.12", "class StaticQueue {", "代码8.12 FIFO and empty optional"),
("算法8.13", "void radix_sort_linked_style(std::vector<int>& values)", "算法8.13 linked radix"),
("算法8.14", "insertion_index_sort(const std::vector<int>& values)", "算法8.14 returns sorted indexes"),
("算法8.15", "void adjust_by_index(std::vector<int>& values", "算法8.15 adjusts records by cycles"),
("代码8.16", "random_values(std::size_t count", "代码8.16 deterministic seed"),
("代码8.17", "class Stopwatch {", "代码8.17 monotonic elapsed time")],
"ch09/external_sort": [
("算法9.1", "replacement_selection(const std::vector<int>& input", "算法9.1 textbook input makes two runs"),
("代码9.2", "class WinnerTree {", "代码9.2 initial winner"),
("代码9.3", "class LoserTree {", "代码9.3 initial winner")],
"ch10/search_hash": [
("代码10.1", "class Item {", "代码10.1 Item getter and setter"),
("算法10.2", "sequential_search(const std::vector<int>& values", "算法10.2 missing key optional"),
("算法10.3", "binary_search(const std::vector<int>& sorted_values", "算法10.3 left boundary"),
("代码10.4", "bool erase(int value)", "代码10.4 keyed deletion status"),
("算法10.5", "bool insert(int value)", "算法10.5 unique insertion"),
("算法10.6", "IntSet intersection(const IntSet& other) const", "算法10.6 intersection commutative size"),
("算法10.7", "bool includes(const IntSet& other) const", "算法10.7 containment"),
("算法10.8", "std::size_t elf_hash(const std::string& text) noexcept", "算法10.8 ELFhash distinguishes nearby strings"),
("算法10.9", "class HashTable {", "算法10.9 table accounting"),
("算法10.10", "bool insert(int key)", "算法10.10 linear collision insertion"),
("算法10.11", "std::optional<std::size_t> find_slot(int key) const", "算法10.11 probes through tombstone"),
("算法10.12", "bool erase(int key)", "算法10.12 deletion creates tombstone"),
("算法10.13", "std::optional<std::size_t> insertion_slot(int key) const", "算法10.13 first tombstone reused")],
"ch12/optimal_bst": [
("算法12.1", "class ReusableNodePool {", "算法12.1 reuses released slot"),
("算法12.2", "OptimalBstResult optimal_bst(", "算法12.2 textbook total cost")],
}

for rel, rows in BINDINGS.items():
    manifest = ROOT / "code" / rel / "unit.json"
    data = json.loads(manifest.read_text(encoding="utf-8"))
    old_ids = [entry["id"] if isinstance(entry, dict) else entry for entry in data["listings"]]
    new_ids = [row[0] for row in rows]
    if old_ids != new_ids:
        raise SystemExit(f"{rel}: binding ids do not match manifest: {old_ids} != {new_ids}")
    data["listings"] = [{"id": i, "anchor": a, "test": t} for i, a, t in rows]
    manifest.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
