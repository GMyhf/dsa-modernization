# 2.3.2 双链表（原书代码2.12 之外的补充）

## 原书给了什么

原书【代码2.12】**只给出双链结点的定义**（`prev` 与 `next` 两个链接域），没有给出完整的
双链表算法。那条清单由 `code/ch02/linked_list` 逐字认领——书稿 2.3.2 印的就是它。
本单元是完整的 `DoublyLinkedList<T>`，属于新增内容，`listings` 为空，靠 `beyond_book`
说明来历（D-008），105 的等式不变。

## 这个结构凭什么多花一个指针

每个结点多存一个 `prev`，换来的**唯一**实质好处是：**已知结点位置时，删除是 O(1)**——
不必像单链表那样先循链找前驱。所以这个能力必须出现在公开接口里，否则多存的那个指针白花。

本单元因此提供 `erase(iterator)` 与 `insert(iterator, value)`。按下标的
`erase(std::size_t)` 仍然是 O(n)，因为**定位**本身要走链——双链表省掉的是找前驱，不是找位置。
两者并存，测试里都覆盖到了。

## 实现要点

- **结点由容器拥有**，`prev`/`next` 是非拥有的裸指针。这里不用 `unique_ptr`：
  链式结构上它的析构是递归的，实测约 5.7 万结点就压穿栈（D-001 §2b 与 2.3.1 节的「所有权工具怎么选」）。
  `clear()` 是循环释放。
- **五法则齐全**，拷贝是深拷贝，移动后源对象留在可用的空状态。
- 越界一律抛 `std::out_of_range`；空表出队同样抛——那是调用方用错接口，不是「空结果」。

## 开发中查出的一处死代码

`swap()` 原本在交换三个成员之后调用 `repair()`，把 `head_->prev` 与 `tail_->next` 置空。
**变异自检发现删掉 `repair()` 之后测试全绿**——因为每张表内部本来就是良构的，
交换三个成员并不会破坏任何链接，`head_->prev` 本来就是 `nullptr`。这段防御代码
永远不可能起作用。已删除，并在 `swap()` 上写明理由。

## 可复现的证据

```text
$ cd code/ch02/doubly_linked_list
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g \
      -fsanitize=address,undefined -fno-sanitize-recover=all test.cpp -o /tmp/dll
$ /tmp/dll
DoublyLinkedList: 41 项断言，0 失败
```

**变异自检**：

| 把实现改成哪种错法 | 后果 |
| --- | --- |
| 插入时不接 `pos->prev` | 反向遍历与随机对拍变红，退出码 1 |
| 删尾时不回退 `tail_` | `ERROR: AddressSanitizer: heap-use-after-free` |
| 删掉 `swap()` 里的 `repair()` | **全绿**——据此判定它是死代码并移除 |

## 验证边界

- 41 项断言，`-Werror` + ASan/UBSan 与 `-O2` 双档通过。
- **反向遍历是单独测的**。正向遍历只用 `next`，`prev` 全接错也照样通过；
  所以每组结构性用例都额外从尾往回走一遍，与正向的逆序逐项比。
- 200 轮随机操作与 `std::list` 对拍（固定种子 999），正反两个方向都比。
- **不在范围内**：没有 `const_iterator` 的 `--`（只有 `iterator` 能反向走）；
  没有 splice / merge / sort；迭代器不满足标准库的双向迭代器完整要求
  （缺 `value_type` 等 traits），不能直接喂给 `<algorithm>`。
- 迭代器在其所指结点被删除后失效，接口文档里写明了，但**没有运行期检查**——
  用一个失效迭代器是未定义行为。
