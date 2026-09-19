# 原书写法 → 问题 → 现代写法：第 8 章内部排序

覆盖算法8.1–8.15、代码8.12、代码8.16/8.17；原文范围为
`dsa_raw.md:6784-7822`。实现没有用 `std::sort`、`std::make_heap` 或
`std::sort_heap` 替代本章教学对象；标准排序只允许留在测试对拍中。

## 清单与现代落点

| 原书清单 | 现代函数 / 类型 |
| --- | --- |
| 算法8.1 / 8.2 / 8.3 | `insertion_sort` / `shell_sort` / `selection_sort` |
| 算法8.4 / 8.5 | `heap_sort` + `sift_down` / `bubble_sort` |
| 算法8.6 / 8.7 | `quick_sort` / `quick_sort_optimized` |
| 算法8.8 / 8.9 | `merge_sort` / `merge_sort_optimized` |
| 算法8.10 / 8.11 / 8.13 | `counting_sort` / `radix_sort` / `radix_sort_linked_style` |
| 代码8.12 | `StaticQueue<T>` |
| 算法8.14 / 8.15 | `insertion_index_sort` / `adjust_by_index` |
| 代码8.16 / 8.17 | `random_values` / `Stopwatch` |

## 原书可复核问题

1. **算法8.1 的结构被 OCR 截断。** 原文 `dsa_raw.md:6792-6800` 的 `while` 与
   `for` 右花括号都变成孤立 `1`，且 `Array[j + 1] = TempRecord` 视觉上落在循环内。
   若把回填保留在循环内，逆序输入每移一次就把待插值提前写回，排序不变量被破坏。
   现代实现把“右移直到 hole”与“只回填一次”拆开；混合、已排序和极值三组数据守住它。
2. **算法8.6 / 8.7 的分区边界不可信。** OCR 中的 `i + +`、`j - -`、比较运算符和
   花括号大量损伤，不能把未闭合的教材伪代码当可执行 C++。现代实现用最后元素作 pivot，
   返回 pivot 的最终位置；左右递归严格为 `[first,pivot)` 和 `(pivot,last)`。
   本轮第一版曾误用双向分区，扩展测试在极值输入上以 exit `-11` 崩溃；改为该边界后通过。
3. **算法8.11 / 8.13 的桶队列所有权不明。** 原书静态队列与顺序基数排序的 OCR 片段把
   指针、`NULL`、队空判断混在一起。现代 `StaticQueue` 固定容量、空提取为 optional；
   LSD 基数排序逐字节稳定分配，翻转符号位以支持完整 `int` 范围。
4. **算法8.14 / 8.15 的索引调整有别名风险。** 原书把“索引表”和“记录数组”作为裸数组
   出参，循环调整时一旦未同步恢复 `IndexArray[j] = j`，后续循环会重复移动已归位记录。
   现代实现沿“目标位置取原位置”的置换环搬运记录，并将已处理项复原为恒等索引；测试断言最终索引是恒等置换。
5. **代码8.16 / 8.17 不可移植。** `srand/rand` 的序列和 `CLOCKS_PER_SEC 1000` 都不是
   跨平台计时契约。现代实现固定 `mt19937` 种子并使用 `steady_clock`。

## 真实编译器证据

原书代码8.16 的全角分号按 C++17 编译会被拒绝。复现命令与输出：

```text
$ printf 'int main(){int x=0； return x;}\n' | clang++ -std=c++17 -x c++ - -o /tmp/ch08-ocr
<stdin>:1:19: error: character <U+FF1B> not allowed in an identifier
```

原书代码8.17 同时有 `clock()；` 的全角分号与 `CLOCKS PER SEC`（缺下划线），
不能作为现代计时实现的源码。上述损伤按 OCR 记录，不把它误判为排序算法思想错误。

## 验证

```text
$ python3 tools/check_code.py code/ch08/sorting --allow-degraded
Sorting: 44 项断言，0 失败
```

`std::sort` 仅用于测试的 `std::is_sorted` 判定；实现文件没有排序 STL 委托。macOS ASan
空探针仍失败，故该验证仅覆盖 Release 运行，不覆盖 sanitizer。

## 作者代码包对照（2026-09-18）

作者代码包 `site_visit/DSCode_ZWZ200806_CPP/`——原书前言（`dsa_raw.md:195`）称之为「与本书配套的代码包」，
也是 2025 秋期末机考的考场资料——是第 8 章各排序的第二证人。下表每一行在 `collab/authorsrc.json` 的
findings 里都有一条带正则的结论，`tools/authorsrc.py --check` 在包的源码上逐条核对；
看包里原文：`python3 tools/authorsrc.py --listing 8.5`。
包与原书同为 2008 年 6 月，谁先谁后不可考：「包里没有」只说明**印出来的与作者的代码不一致**，不说明是排印时引入的。

| 结论 | 缺陷 | 包里 | 说明 |
| --- | --- | --- | --- |
| P01 | 包里 `MaxHeap` 的 `SiftDown` 越界 | **只在包里** | 判右孩子写 `j<CurrentSize`，应为 `j<CurrentSize-1`；n 为偶数时建堆读 `heapArray[n]`。原书没印 `MaxHeap`（算法8.4 只印调用它的 `sort`）；第 5 章 `MinHeap` 写对了 |
| P02 | 冒泡排序 `j>=i` | **只在包里** | i=0 时比较 `Array[0]` 与 `Array[-1]`；原书算法8.5 印的 `j > i` 是对的 |
| P05 | 桶排序包装写死 `max = 80` | **只在包里** | 驱动生成 `Random(32003)`，值 ≥ 80 即越界写 |
| P06 | 计时驱动不排序 | **只在包里** | `SortMain.h` 里 `//sort<int>(&array[i], listsize);` 被注释掉——包里 12 个排序程序从没在这个驱动里真正排过 |

四处缺陷都在原书之外：原书要么没印那段代码，要么印对了。它们之所以要记，是因为**这个包是考场资料**——
直接拿包里的 `MaxHeap` 或 `BubbleSort` 去交题的人，会在偶数长度或第一趟比较上读到数组外面。
`author_diff.cpp` 把包里 12 种排序逐个拿出来与本单元、与 `std::sort` 三方对拍（506 组输入）；
两处越界用「在数组外垫一格」的办法让 ASan 放行、再单独断言「垫一格大数时结果被污染」，把缺陷钉成可复现的证据：

```text
$ python3 tools/authorsrc.py --diff
  ✅ 排序对拍：12 种算法 × 各自的输入，共 506 组，作者版、本单元实现与 std::sort 三方一致  ← code/ch08/sorting/author_diff.cpp
```

不垫那一格时，第一次撞上的就是 `MaxHeap` 的越界读：

```text
==ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 4
    #0 in a_heap::MaxHeap<int>::SiftDown(int) ch08_Sort/HeapSort/MaxHeap.h:76
0x... is located 0 bytes after 8-byte region
```
