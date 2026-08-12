# 原书写法 → 问题 → 现代写法：第 10 章检索与散列

覆盖代码10.1、算法10.2/10.3、代码10.4、算法10.5–10.13；原文范围为
`dsa_raw.md:8693-9559`。每个检索失败都由 `optional` 或 `bool` 表达，容器内部不输出文字。

## 清单映射

| 原书清单 | 现代落点 |
| --- | --- |
| 代码10.1、算法10.2、算法10.3 | `Item<T>`、`sequential_search`、`binary_search` |
| 代码10.4、算法10.5–10.7 | `IntSet` 的插入、交集、包含和删除 |
| 算法10.8 | `elf_hash` |
| 算法10.9–10.13 | `HashTable` 的开放定址、插入、检索、墓碑删除和复用 |

## 原书可复核问题

1. **代码10.1 的成员名大小写不一致。** 原文声明 `T Key;`，构造函数却写
   `:key(value)`，C++ 区分大小写；`vector<Item<T>*> dataList;` 也没有 `std::` 或头文件。
   这不是现代风格争论，而是印刷文本不能直接编译。现代 `Item<T>` 以私有 `key_` 和 const
   getter 明确关键码语义。
2. **算法10.2 的监视哨会改写调用者数据。** 原书写 `dataList[0]->setKey(K)`，要求位置 0
   永远存在且可写，还会覆盖原先存放的值；空表时直接解引用越界。现代顺序检索不写输入。
3. **算法10.3 的下标体系混杂。** 文本说有效项是 1..length，但现代 `vector` 正常从 0 开始；
   OCR 还出现 `int low = 1., high = length`。现代采用 `[first,last)`，不做 `mid - 1` 的
   无符号下溢。
4. **算法10.9 析构写错释放形式。** `HT` 是 `T*` 数组，原文写 `delete HT;`，应为 `delete[]`；
   默认复制又会浅复制数组。现代哈希表让 `vector<Slot>` 独占槽位。
5. **算法10.10 / 10.11 无满表终止。** 原文前提“探查序列至少有一个空槽”，否则不成功检索
   可无限循环。现代探测最多 `capacity()` 步，满表插入返回 false。
6. **算法10.12 / 10.13 的墓碑语义必须整体保留。** 删除不能设为空，否则碰撞链后的键漏检；
   插入也不能看到首个墓碑就立即写入，否则可能漏掉后方的重复键。现代测试分别验证“越过
   墓碑继续查到 6/11”与“找到真正空槽后复用第一个墓碑”。

## 真实编译器证据

把代码10.1 的大小写错误缩成最小片段：

```text
$ printf 'struct Item { int Key; Item(int v):key(v) {} };\n' | clang++ -std=c++17 -x c++ - -c
<stdin>:1:42: error: member initializer 'key' does not name a non-static data member or base class
```

代码10.1 原文的 `vector` 未限定名也会失败：

```text
error: no template named 'vector'; did you mean 'std::vector'?
```

## 验证

```text
$ python3 tools/check_code.py code/ch10/search_hash --allow-degraded
SearchHash: 23 项断言，0 失败
```

关键回归是 1、6、11 共用同一基地址：删掉 1 后，6/11 仍能被查到；插 16 才复用位置 1。
若把墓碑改为空，`算法10.11 probes through tombstone` 会失败。macOS ASan 空探针失败，故本机
只验证 Release；墓碑槽与 vector 的运行期内存检查待 Claude 补跑。
