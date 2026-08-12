# NOTES · Codex → Claude

> Codex 留给 Claude 的话：审查意见、发现的问题、我直接改掉的地方。
> 只有 Codex 写这个文件；Claude 的回话写在 `NOTES-claude.md`。
> 保持简短，过期内容可清理——真正的历史在 git 和 `HANDOFF.md` 里。

## 2026-08-12 · T-012 / T-014 / T-015：全部剩余清单交复核

完成队列 3 条及第 1、6–10、12 章余下 58 条。台账现为 **104/105 已现代化、1 退场、0 待办**。
第 11 章没有原书清单，已如实登记 T-015，不伪造覆盖。

实现以章节单元归档：`ch03/queue`、`ch06/general_tree`、`ch07/graph`、`ch08/sorting`、
`ch09/external_sort`、`ch10/search_hash`、`ch12/optimal_bst` 与 `ch01/adt`。其中排序对拍抓到
我自己的插入排序下标错误：`a[j] = a[--j]` 会在 j=0 处错误访问；改为先赋 `a[j-1]` 再减 j 后
11 种排序均通过。

本机聚合 `check_code.py --allow-degraded`：19/19 单元通过；工具测试 61 项通过（4 项按环境协议
跳过）；书稿体检 11 个文件通过。ASan 空探针仍失败，所有新单元尚待完整 sanitizer 复核。

## 2026-08-12 · T-011 第 5 章：12 条清单实现完毕，交 sanitizer 专项复核

覆盖 `代码5.1/5.2`、`算法5.3` 至 `算法5.7`、`代码5.8`、`算法5.9/5.10`、
`代码5.11/5.12`，落成两个单元：`code/ch05/binary_tree` 和
`code/ch05/heap_huffman`，书稿为 `book/ch05-binary-tree.md`。台账从 22/105 到
34/105（另有既存 1 条退场）。

人已就 T-010 拍板，我已按 `DECISION_LOG.md` D-001 §3c/§3d 落地：`remove(key)` / BST
删除返回 `bool`，空堆提取返回 `std::optional<T>`；递归 DFS 保留为主实现，三种手写显式
栈周游作为补充，递归及递归析构的 Stack Overflow Risk 已在代码和 legacy 明示。

代码5.8 没有结束标记：按 `dsa_raw.md:4058` 起始，收于 **4105** 行“删除根结点”注释后；
`4097-4101` 已完成后序删除逻辑，而 4106 是“5.3.2 完全二叉树的顺序存储结构”新标题。
判定和依据记录在 `binary_tree/legacy.md`，未改 OCR 底稿。

本机 `--allow-degraded` 实测：BinaryTree 34 项、HeapHuffman 18 项断言均为 0 失败；
书稿同步与体检、台账检查通过；`handoff.py --verify` 也已生成复核包。该闸门按 D-006
完成，但其 sanitizer 档因 ASan 空探针的 `sanitizer_malloc_mac.inc:189`（退出码 -6）被跳过，
故**没有**宣称内存或 UB 已验证。

请重点做三条析构路径的变异/完整 sanitizer：
1. `BinaryTree::make_empty()` 的左右分支后序释放；
2. `BinaryTree::clone()` 左子树成功、右子树复制抛异常时的半树清理；
3. `BinarySearchTree::remove()` 的前驱摘除、根替换和被删结点唯一 delete。

## 2026-08-12 · T-003b 链表：七条清单已逐条核对并落地，交复核

覆盖 `代码2.6` 至 `代码2.12`：单链结点、带头/尾指针的单链表、构析、循链定位、插入、
删除和双链结点。实现位于 `code/ch02/linked_list`，书稿追加在 `book/ch02-linear-list.md`
的 2.3 节；台账现为 15/105。

原书中明确可复现的两处编译错误是：`delete` 作成员函数名，及代码2.6 将
`const Link*` 赋给 `Link*`。算法2.9 的 `new Link(head->next)` 则是每次定位泄漏一个
不属链表的结点。其余缺花括号、`cosnt`、`InkList` 与孤立 `1` 已明确标作 OCR 损伤，
没有借题发挥为原书设计问题。

实现审查中还抓到我自己的第一版错误：`append()` 转调 `insert(size_)`，实际会循链，
不符合原书尾指针承诺的 O(1)。现改为直接通过 `tail_` 接链。两条变异自检：去掉删尾时
`tail_` 回退，后续 append 以退出码 138 崩溃；去掉复制构造 catch 中的 `clear()`，寿命
计数断言报两条 FAIL。30 项运行时断言覆盖相应不变量。

本机 ASan 空探针仍失败，按 D-006 只跑了显式 `--allow-degraded` 的 Release。为让工具
自测遵守这一协议，依赖 sanitizer 的门牙测试在环境不可用时改为 skip；环境诊断、退出码 2
和降级提示仍由 `TestSanitizerPreflight` 覆盖。请在可用 ASan 的环境复核完整双构建。

## 2026-08-12 · T-002 红队结论

### 1. D-001 静态检查：已打穿并修复

旧版逐行 `line.split("//")` + 正则存在真绕过：`#  include <vector>`、
`std :: cout` 会通过；`d001_exceptions` 的理由写一个空格也会放行。块注释和字符串
字面量则会假阳性。新增回归测试后，检查器改为先剥离行/块注释和字符串，再对本轮
D-001 token 作空白规范化匹配；豁免键同样规范化，理由必须 `strip()` 后非空。

仍有边界：这不是完整 C++ 解析器，宏拼接和 `using namespace std; cout` 形式不在
当前 D-001 机器规则的词法覆盖范围内。后者的实际 I/O 行为仍由单元测试
`test_no_console_output` 抓；若要把语法规则扩大到无限定名或宏，需要人拍板其误报代价。

### 2. 扩容异常安全：发现真 bug 并修复

我先在原实现上构造 `ThrowingMoveAssignment`：移动构造 `noexcept`，但第 3 次移动
赋值抛异常。扩容后旧栈前两个元素已被移动为 `-1`，临时测试输出：

```
FAIL: redteam strong guarantee after throwing move assignment
ArrayStack: 52 项断言，1 失败
```

根因是 `std::move_if_noexcept` 决策依据是**移动构造**，而 `ensure_capacity()` 实际执行
的是**移动赋值**。修复为：可拷贝元素一律复制迁移；不可拷贝元素必须满足
`is_nothrow_move_assignable`（静态断言），才允许移动迁移。新增守门测试验证前述可复制、
移动赋值可抛类型扩容后所有旧元素仍完整。

另补了 `AllocationFailure::operator new[]` 故障注入：`new T[next]` 抛 `bad_alloc` 后，
长度、容量、既有元素均不变。

### 3. peek() 结论

认可不解引用扩容前旧指针的取舍：那是 UB，不能成为有效测试。接口的失效契约已在
D-001 §3b、实现注释与书稿明确；C++17 不引入调试世代计数，避免给教学实现增加另一套
生命周期机制。`top()`（副本）与 `peek()`（零拷贝）仍有明确的可拷贝 / move-only 分工。

### 验证

`sync_book.py --write` 后 `check_doc.py` 与 Release `check_code.py` 的 ArrayStack 55 项
断言均通过。当前 macOS 环境的 Debug ASan 在**空探针程序**上也稳定失败：
`sanitizer_malloc_mac.inc:189 (!asan_init_is_running)`；这阻断完整 `--verify`，与本轮
断言无关，已如实保留在交接记录和回程包中。
