# 12.3 Trie 结构和 Patricia 树

## 原书给了什么

原书 12.3 只有文字和示意图，**没有 `【算法】` / `【代码】` 清单**（第 12 章的 105 条里只有
算法12.1、算法12.2，都归 `code/ch12/optimal_bst`）。参考资料里也没有：

```text
$ grep -ril "trie\|patricia" "ref_数据结构与算法A 2021秋/SourceCodes" ref_DSA \
      --include='*.cpp' --include='*.h' --include='*.c'
（无输出）
```

所以这个单元是从零写的，`unit.json` 的 listings 为空，由 `beyond_book` 说明来历。
原书在本节确立的四点全部保留：

- Trie 按关键码的**第 i 个字符**在第 i 层分支，公共前缀在树里只存一次。
- 查找时间与**关键码长度**成正比，与表里有多少词基本无关。
- 最长前缀匹配：走到走不动为止，回退到最近的词尾（路由表查找就是这个动作）。
- Patricia 把「只有一个孩子」的结点压缩掉，内部结点只记「跳过几位再比」。

## 现代实现的取舍

- **字母表限定 `'a'..'z'`**，正好覆盖原书的 can/car/cat/do 例子。越界字符抛
  `std::invalid_argument`——那是调用方用错接口，不是「查不到」这种预期空结果（D-001 第 3 条）。
- **孩子用 `std::unique_ptr`**。这里所有权是唯一的，和 `code/ch12/gen_list` 的共享子表
  完全不同：广义表必须手写引用计数才能教清楚回收，Trie 不需要，硬套反而制造仪式感。
- **`passing` 计数**让 `count_with_prefix` 是 `O(前缀长度)` 而不是遍历子树。重复插入必须把
  这一路计数退回去，否则前缀计数会虚高——测试专门盯这一条。
- **删除会真正回收结点**。只承载被删词的结点逐层摘掉，`node_count()` 因此会下降；
  不做这一步的 Trie 只增不减。
- **Patricia 按位工作**：关键码当作字节位串（高位在前），越过长度的位读作 0，所以键里不能有
  `'\0'`。走到叶之后必须和叶上的完整关键码**再比一次**——路上只看了少数几位，这次比较不能省。

## 可复现的证据

```text
$ cd code/ch12/trie
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
      -fsanitize=address,undefined -fno-sanitize-recover=all test.cpp -o /tmp/tr
$ /tmp/tr
Trie: 358 项断言，0 失败
```

上面表格里的三处变异都真的跑过，例如「Patricia 到叶后不比完整关键码」
（`return leaf->key == key;` 改成 `return leaf != nullptr;`）：

```text
  FAIL: 12.3 Patricia 前缀不是词
  FAIL: 12.3 更长的串不是词
  FAIL: 12.3 完全不相干的串
  FAIL: 12.3 前缀链外的串查不到
```

## 验证边界

- 358 项断言，`-Werror` + ASan/UBSan 与 `-O2` 双档通过。
- 数字对得上原书的例子：can/car/cat/do 共 11 个字符，Trie 只用 **7 个结点**（公共前缀 `ca`
  只存一次），Patricia 进一步压到 **3 个内部结点**。
- **变异自检**（三处，都见红）：

  | 把实现改回哪种错法 | 哪条断言变红 |
  | --- | --- |
  | 重复插入时不退回 `passing` | `12.3 重复插入不虚增前缀计数` |
  | Patricia 到叶后不比完整关键码 | `12.3 Patricia 前缀不是词` 等 4 条 |
  | 删除后不摘结点 | `12.3 结点也全部回收，Trie 不是只增不减` |

- **不在范围内**：Trie 只支持小写字母，不是通用字节 Trie；没有做原书提到的 IP 路由那种
  按位 Trie 的实际路由表语义。`keys_with_prefix` 与 Patricia 的析构对树高递归，
  病态深键会有栈风险（与第 5 章递归周游同源）。Patricia 只实现插入和查找，**没有删除**。
