# 原书写法 → 问题 → 现代写法：最小堆与 Huffman 树

覆盖：代码5.11、代码5.12。原文范围为 `dsa_raw.md:4306-4561`。

| 清单 | 原书意图 | 现代落点 |
| --- | --- | --- |
| 代码5.11 | 顺序表存储的小根堆及删除最小元 | `MinHeap<T>` |
| 代码5.12 | 用最小堆反复合并权值的 Huffman 树 | `HuffmanTree` |

## 可复现或可审计的问题

1. **固定数组边界没有接口契约**：原书 `heap[MaxSize]` 与 `last` 的下标关系依赖调用者；
   插入到满堆时没有可见的容量处理。现代 `MinHeap<T>` 按需扩容，空堆 `remove_min()` 返回
   `std::nullopt`，符合 D-001 §3c 的“提取操作以 optional 表达预期缺失”。
2. **数组所有权没有五法则**：原书析构释放 `heap`，但未定义复制构造和赋值；默认浅复制会导致
   两次 `delete[]`。现代类以深拷贝、copy-and-swap、自赋值检测和移动转移覆盖该路径；
   `test.cpp` 对复制、自赋值和移动后的空源堆断言。
3. **扩容的迁移前提必须明确**：新数组的逐项赋值若能抛异常，移动掉旧值会破坏强保证。现代实现
   仅接受“移动构造和移动赋值均不抛”的元素类型；该限制在 `static_assert` 中可见，与 D-005
   的数组扩容策略一致。
4. **Huffman 合并时的半成品树**：原书将结点指针交给堆，但没有说明 `new` 或堆插入失败时
   左右子树的归属。现代构造函数在失败路径先拆开并删除父结点，再递归销毁已取出的两棵子树，
   外层继续回收仍在堆中的树根。`weights == nullptr && count != 0`、负权值及 `int` 求和溢出
   都显式拒绝，避免解引用空指针或有符号溢出 UB。

## 复核命令

```sh
python3 tools/check_code.py code/ch05/heap_huffman --allow-degraded
```

本机的实测 Release 输出为 `HeapHuffman: 18 项断言，0 失败`。同次 ASan 空探针因
`sanitizer_malloc_mac.inc:189` 退出码 `-6`，因此该命令明确是降级结果，尚不覆盖泄漏与 UB。

## 原书编译证据与未覆盖路径

代码5.11 的 OCR 将析构形式识别为 `∼MinHeap()`（而非 ASCII `~MinHeap()`），并在部分数组
下标旁留下孤立 `1`。该字符直接进入 C++ 源会被识别为不同标识符，而不是析构函数：

```text
$ printf 'struct H { ∼H() {} };\n' | g++ -std=c++17 -x c++ -
<stdin>:1:12: error: expected unqualified-id before '\342' token
```

代码5.12 的类声明使用 `HuffmanTree (int w [ ], int n)`，构造过程中的 `new HuffmanNode`、
堆插入和左右子树归属均无失败路径说明；OCR 还把 `delete` 与相邻标识符粘连。现代实现显式
处理合并半成品和总权溢出。

`MinHeap::ensure_capacity` 曾有一个不可达的 `catch`：元素类型被 `static_assert` 限制为移动
构造和移动赋值都 `noexcept`，而分配失败发生在 `try` 外，所以迁移循环不会抛。该死代码已删，
继续维持这个受限但明确的类型契约，而不在本次扩展为 D-005 的可抛移动双判据。

`HuffmanTree` 建叶后的 `catch { delete leaf; }` 只会在堆扩容分配失败时可达。现有
`AllocationFailure` 只注入 `operator new[]`，不能覆盖单对象 `new Node`；本批不扩探针，
故此路径仍列为未验证，交由 sanitizer/故障注入专项处理。
