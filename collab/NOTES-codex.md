# NOTES · Codex → Claude

## 2026-09-05 · T-069：courseware 门禁复核与优化

范围：最近提交 `a3bca68` 及其依赖的 `courseware/verify.py`。本轮未做全书答案正确性审阅或视频试听。

- P2，已修：第 5 项只比文字。保持文字不变、将形状右移一英寸，旧门禁仍通过。
  现在比对 PPTX 包内 XML 和媒体字节，忽略 ZIP 时间戳与压缩元数据；颜色变化也会报错。
- P2，已修：第 7 项不检查 `pdftotext` 返回码，也不要求提取页数与课件一致。
  模拟退出码 1、空输出或多一页，旧门禁均误放行。现在检查返回码和页数，
  同时检查 `pdffonts` 返回码。
- P1，待处理：完整仓库门禁发现已发布书稿 PDF 过期：源码摘要 `3b38eea57d39`，
  sidecar `f5976917926e`。需要另行重建 PDF 并核对新增答案后的页数与排版。

验证命令（本机使用已有 `/private/tmp/faq-pptx-venv/bin/python`，含 python-pptx 1.0.2）：

```text
python -m unittest discover -s courseware -p test_verify.py -v
旧实现：Ran 6 tests，FAILED (failures=4)
修复后并补充颜色和 ZIP 时间戳用例：Ran 8 tests，OK
python courseware/verify.py
12 章 / 378 页包内文件一致；2 个 C++ 编译运行通过；28 个代码块核对；退出码 0
python3 tools/handoff.py --verify
工具自测 Ran 420 tests，OK (skipped=4)
书稿、台账、HTML、12 份 book PPTX（410 页）、插图检查通过
PDF 过期；ASan 空探针退出 -6，sanitizer_malloc_mac.inc:189；整体退出码 1
```

真实调用 `check_render(written_chapters())`：LibreOffice 退出码 1，期望 12 份 PDF、
实得 0 份，检查正确报错。渲染分支的回归测试使用模拟命令输出；不能据此声称
完成 378 页视觉验收。原有未跟踪资料未改动；用户随后要求将本轮 6 个文件提交并推送。

> Codex 留给 Claude 的话：审查意见、发现的问题、我直接改掉的地方。
> 只有 Codex 写这个文件；Claude 的回话写在 `NOTES-claude.md`。
> 保持简短，过期内容可清理——真正的历史在 git 和 `HANDOFF.md` 里。

## 2026-08-18 · T-047：双实现共享用例表（D-028）

11 个双实现单元现共读五列 `cases.tsv`；C++ 与 Python 各自执行并报告条数，闸门要求
两侧都等于表长。常量也进表，异常只用语言中立类别。B+ 树已有单元级 `py_skip`，不造假表。

验收不是只跑绿：临时只改 Python 的计数排序阈值，C++ 仍绿，Python 两档都以
`FAIL: T-047 counting-limit` 判红；恢复后通过。工具自测新增缺表、坏列、未知异常、
漏报与错报。当前 macOS 的 sanitizer 空探针仍在 `sanitizer_malloc_mac.inc:189` 失败，
因此本轮本机全量只能诚实标为 `--allow-degraded`，不能沿用上一轮 Linux 的 sanitizer 结论。

## 2026-08-12 · T-015：队列与堆/Huffman 的 D-007 证据收口

两个早于 D-007 的单元现已达标：`Queue: 36 项断言，0 失败`（3 条下限 9）和
`HeapHuffman: 21 项断言，0 失败`（2 条下限 6）。队列新增前后下标各至少绕环九次、满队列
长度不变、排空端点、数组/链式深复制独立变异等断言；legacy 加入原书逐条核对和真实 `g++`
编译 `error:`。

堆/Huffman 的 legacy 同样补了原书 OCR/编译证据。`MinHeap::ensure_capacity` 的 catch 已删除：
类型契约静态要求移动构造和移动赋值不抛，分配失败发生在迁移之前，原 catch 不可达；本批保留
这个受限契约，不改为 D-005 的可抛移动双判据。风险台账同步删除该死代码项。

Huffman 建叶后、堆扩容分配失败时的 `delete leaf` catch 仍未覆盖：现有故障探针只注入
`operator new[]`，不覆盖单对象 `new Node`。本批没有扩大探针，已明确留在 legacy 和
UNVERIFIED-RISKS。ASan 空探针仍在 `sanitizer_malloc_mac.inc:189` / exit -6 失败，故本机仅
Release；请在可用环境补跑 sanitizer 与该分配失败路径。

## 2026-08-12 · T-014 批次 5：第 1、9、12 章返工交复核

本批三个单元分别满足 D-007：`ADT: 7 项断言，0 失败`（2 条下限 6）、`ExternalSort: 12 项
断言，0 失败`（3 条下限 9）、`OptimalBST: 15 项断言，0 失败`（2 条下限 6）。全部已展开为
书稿可读的多行实现，并逐条补全 legacy 的原书落点和编译证据。

第 1 章保留 Floyd 选择传播源。原书正文称 B3 最佳，但其印刷矩阵下 B1 与 B5 并列最优，且原
严格小于的选择规则返回先出现的 B1；测试按可执行矩阵钉住 B1，已记录这处样例矛盾。第 9 章
不再用标准排序替代，手写最小堆和可重赛竞赛树。第 12 章为可利用空间表提供索引句柄池，避免
原书共享全局 `operator new/delete`，并保留三表最优 BST DP。

本机均只完成 `--allow-degraded` Release；ASan 仍在 `sanitizer_malloc_mac.inc:189` / exit -6
退出。请在可用环境重点检查 Floyd 的距离加法、竞赛树 `replace` 路径、池复用后的陈旧句柄和
DP 大权重溢出。

## 2026-08-12 · T-014 批次 4：第 6 章树返工交复核

第 6 章单独满足 D-007：`GeneralTree: 48 项断言，0 失败`（10 条清单下限 30）。实现已从
压缩单行展开为书稿可读形式；三种周游、复制/移动、路径压缩与按秩合并均保留。修正了森林中
删除非首根的真实空 `parent` 解引用风险：现在统一沿父孩子链或根兄弟链脱链，找不到归属结点
会拒绝销毁。

测试覆盖三种周游、孩子/兄弟插入、森林首尾根删除、深复制、自赋值、移动、空指针、清空、
并查集的合并/压缩/越界。legacy 逐项记录 OCR 的 `m_ Value` 编译 `error:` 与递归 Stack
Overflow Risk。

本机只完成 `--allow-degraded` 的 Release 检查；ASan 空探针仍在
`sanitizer_malloc_mac.inc:189` / exit -6 失败。请重点变异三条最脆弱的析构路径：递归
`destroy` 的 child/sibling 次序、`clone` 右分支抛异常后的半树清理、`delete_subtree` 的局部
子树与森林根脱链。

## 2026-08-12 · T-014 批次 3：第 7 章图返工交复核

第 7 章单独满足 D-007：`Graph: 41 项断言，0 失败`（11 条清单下限 33）。原有七个算法实现
均保留；首个返工提交仍遗留超长压缩行，已由后续独立格式提交展开并同步书稿。新测试逐源点将
Dijkstra 与 Floyd 对拍，比较 Prim/Kruskal 的边数与总权，覆盖 BFS/DFS 顺序、拓扑环、非连通
MST、负权和越界顶点。

legacy 现在记录代码7.1 的缺函数体、代码7.2 标识符被空格切断、代码7.3 不能表示零权边、
代码7.4 裸链表所有权不明，以及算法7.6/7.9 缺结束标记，并附可重跑的 `error:` 证据。

DFS 仍是递归，Stack Overflow Risk 已明示。macOS ASan 未启动；请补跑 sanitizer，重点变异 DFS
访问标记、Floyd 的 infinity 加法保护与 Prim/Kruskal 的非连通返回。

## 2026-08-12 · T-014 批次 2：第 10 章检索与散列返工交复核

第 10 章已单独满足 D-007：`SearchHash: 39 项断言，0 失败`（13 条清单的下限为 39）。
原有实现的墓碑算法不是空壳，但测试缺口已补：1、6、11 冲突后删除 1，6/11 仍可检索；插 16
在确认后方没有重复键后复用第一个墓碑；负键、回绕、满表和幂等删除也被覆盖。

legacy 已重写为 20 行以上的逐清单映射与 OCR/编译问题记录，含代码10.1 `Key`/`key` 大小写
错误和未限定 `vector` 的真实编译器输出。二分检索改为半开区间，避免无符号下标减一。

本机仍为 `--allow-degraded`；请在本批推送后变异墓碑为 empty、墓碑立即插入、以及满表循环的
终止条件，补跑 sanitizer。

## 2026-08-12 · T-014 批次 1：第 8 章排序返工交复核

第 8 章已单独完成返工并满足 D-007：`Sorting: 51 项断言，0 失败`（17 条清单的下限为 51）。
`quick_sort`、`heap_sort`、`radix_sort` 与 `radix_sort_linked_style` 现在均为手写实现；实现文件
不再调用 `std::sort`、`std::make_heap` 或 `std::sort_heap`。书稿已同步，legacy 扩展为带原书
切片、OCR 判断、真实编译器输出和验证命令的审计记录。

扩展测试实际抓到两处我的初版问题：双向快排分区在极值输入 exit -11，计数排序不应对全 int
值域分配桶。前者已改为 Lomuto 边界；后者对超过一千万的稀疏值域抛 `invalid_argument`，由
基数排序处理完整 int 范围。索引循环调整也曾方向错误，固定置换用例已守住。

本机仍只能 `--allow-degraded`；请在本批推送后补跑 sanitizer，重点审计快排递归、桶队列与
索引循环搬运。

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
