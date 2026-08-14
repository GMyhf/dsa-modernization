# 12.2.1 广义表的定义和存储结构

## 原书给了什么

原书 12.2.1 只有文字和图（图 12.7–12.9：无头结点、带头结点、带循环的三种存储），
**没有 `【算法】` / `【代码】` 清单**——`tools/ledger.py` 解析出的 105 条里，第 12 章只有
算法12.1、算法12.2 两条，都归 `code/ch12/optimal_bst`。所以本单元的 `unit.json` 认领的
listings 为空，靠 `beyond_book` 字段说明来历，105 的台账等式一动不动。

原书在本节确立的三件事，现代实现全部保留：

- 元素可以是原子，也可以是另一个广义表；结点用**标记位**区分这两种。
- 非空广义表可以**唯一**拆成「表头 + 表尾」，所以递归算法写成「先处理头、再处理尾」。
- 子表可以被**共享**（再入表，对应有向无环图），也可能成环（循环表）。共享一旦发生，
  回收就不能再按树递归 `delete`。

## 参考实现的实测缺陷

课程资料里有一份对应的旧实现，`ref_数据结构与算法A 2021秋/SourceCodes/Chap12_AdvDS/AdvList/GenList/`。
它能编过，但**回收是错的**：

```text
$ cd "ref_数据结构与算法A 2021秋/SourceCodes/Chap12_AdvDS/AdvList/GenList"
$ g++ -std=c++17 -fsanitize=address -g -o /tmp/refgl Example.cpp
$ /tmp/refgl >/dev/null
SUMMARY: AddressSanitizer: 64 byte(s) leaked in 4 allocation(s).
```

泄漏点在 `Example.cpp:26/29/30` 等处 `new` 出来的 `GenList` 对象。它的样例里还有一行
`List4->Insert(List4)`——一个**自己指向自己的循环表**，这正是引用计数收不回来的那类结构。

同一目录下的编译告警也说明它不是能直接印进书里的代码：

```text
$ g++ -std=c++17 -Wall -Wextra -fsyntax-only Example.cpp 2>&1 | grep -c warning:
12
```

其中 `GenList<char*> *List=new GenList<char*>("List")` 触发
`ISO C++ forbids converting a string constant to 'char*'`。

## 现代实现改了什么

- **句柄 + 手写引用计数**。`GenNode` 上放计数，`GenList` 句柄负责加减，五法则齐全。
  这里不用 `shared_ptr`：12.2 要教的就是「谁来回收共享结点」，交给标准库等于删掉这一节
  （D-001 第 2 条）。
- **表尾方向用循环释放**。`release()` 沿 `tail` 迭代、只对 `head` 递归，所以三万个元素的
  长表析构不会压穿栈；栈深度只跟**嵌套层数**走，不跟表长走。测试里有 30000 元素的用例。
- **空状态与错误分开**。空表没有头尾 → `head()`/`tail()` 返回 `std::nullopt`；对表调
  `value()`、把原子当表尾 `cons` → 抛 `std::invalid_argument`（D-001 第 3 条）。
- **`use_count()` 是教学接口**，让「共享发生了」这件事在测试里看得见，而不是靠画图想象。

## 验证边界

- 40 项断言，`-Werror` + ASan/UBSan 与 `-O2` 双档通过。
- **变异自检**：把 `release()` 退化成参考实现那种「不看计数直接 delete」——
  `while (node != nullptr && --node->refs == 0)` 改成 `while (node != nullptr)`——
  测试立即报 `ERROR: AddressSanitizer: heap-use-after-free`，退出码 1。
- **循环表不在本单元的范围内**。`cons` 自底向上构造，构造不出环，所以引用计数够用；
  原书 12.2.4 讲的无用单元回收（标记-清扫）正是为环准备的，本单元不实现，也不假装实现。
  参考实现 `Example.cpp` 里的 `List4->Insert(List4)` 就是会漏的那种环。
- `depth()`/`atom_count()`/`to_string()` 对**表头**方向递归，深度等于嵌套层数。本单元测到
  4 层；病态的深嵌套（上万层）会有栈风险，与第 5 章递归周游的风险同源。
