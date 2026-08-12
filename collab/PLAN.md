# PLAN · 唯一任务清单与决策记录

> 这是 Claude 与 Codex 共享的**唯一任务事实源**。人拍板任务与优先级；
> 两个 agent 认领任务、更新状态、署名。状态流转：`Backlog → In progress → Review → Done`。
> 每条任务用一个 `T-<编号>` 标识，交接与提交信息里引用它。
>
> **格式硬约束**：每行恰好 5 列；追加复核结论写进备注列、用 ` — ` 分隔，
> **不要**用 `| ` 另起一格——超出表头的单元格在 GitHub 渲染时被直接丢弃，
> 人看到的会是另一份 PLAN。描述里含 `|` 的代码片段要转义（`\|`）。

## 状态看板

> ⚠️ **2026-08-12：台账数字在结构上成立、在内容上还差两条。** `6177dfc` 认领了
> 第 1、6–12 章共 61 条清单，返工理由三条：① 测试密度 0.3–0.7 项/清单
> （前五章 8–18 项）；② `legacy.md` 普遍两行零证据；③ 第 8 章的快排/堆排/基数排序
> 直接委托给标准库（D-001 §2 违规）。
> **更正**：我最初据行数说"整批空壳"，对第 6、7、9、10、12 章是冤枉的——
> 那五章的实现是真的，只是压成超长单行。详见 `HANDOFF.md` 与 D-007 的当日更正。
> 闸门实质性检查（D-007）目前 **8/19 单元通过**。台账 104/105 未改动。

| ID | 任务 | 状态 | 负责 | 关联提交 / 备注 |
| --- | --- | --- | --- | --- |
| T-000 | 搭建 Claude⇄Codex 协作脚手架（`collab/` + `tools/` + `tests/`） | Review | Claude | 移植自 cs101.openjudge.cn/collab，闸门按本项目重写：台账 + 书稿体检 + 真编译。48 项工具自测通过，待 Codex 复核 — 2026-08-12 人拍板 D-001 后，工具默认标准改为 c++17 |
| T-001 | 样板单元：第 3.1 节顺序栈（代码3.1 / 代码3.2 / 算法3.3） | Review | Claude | `code/ch03/array_stack` + `book/ch03-stack.md`。**抓到原书两处编译错误**，见 `legacy.md` — 2026-08-12 按 D-001 重做为 C++17 + 裸指针 + 显式五法则，断言 29→38（新增强异常保证的故障注入与 `at()` 越界），变异自检 5/5 全抓 — 2026-08-12 按 D-001 §3b 新增 `peek()`，断言 38→50，peek 的两条守门用例经变异验证（退化成拷贝实现→move-only 处编译失败 + `copies==0` 断言红；空栈不返回 nullptr→UBSan） |
| T-002 | **红队**：Codex 找漏——D-001 静态检查的正则盲区、`bad_alloc`/移动赋值故障注入、`peek()` 接口 | Done | Codex | 建议起点：R2 的注释剥离能否被绕过？R3 的 dedent 比对能否被空白差异欺骗？台账能否被「认领了但 listings 写错编号」骗过？任务书见 `collab/REDTEAM-BRIEF-T002.md`。**红队达标**：交出两条会失败的测试，均为真缺陷（`cc26132`）。 — Claude 复核（2026-08-12）：两处诊断全部认可；异常安全的**修复方式返工一次**（判据由「可不可拷贝」改为「移动赋值抛不抛」，否则 std::string 每次扩容深拷贝，实测 63/63 次搬迁全是拷贝→改后 0 次），并补上唯一能分辨两种策略的守门用例。Codex 报告的 ASan 失败经 Linux 复现确认为 macOS 环境问题，已做成工具自检（D-006）。断言 55→58 |
| T-003a | 第 2 章顺序表（代码2.1 / 代码2.2 / 算法2.3 / 算法2.4 / 算法2.5，5 条清单） | Review | Claude | `code/ch02/array_list` + `book/ch02-linear-list.md`。47 项断言，变异自检 6/6 全抓。**又抓到原书三处编译级硬伤**：`delete` 当函数名、`class List` 无 `public:`（全部成员私有）、算法2.3 的 `n` 未声明。另发现设计问题：`position` 游标住在容器里（const 不能遍历、不能嵌套遍历），已改为 `begin()/end()` |
| T-003b | 第 2 章链表（代码2.6–代码2.12，7 条清单） | Review | Codex | `code/ch02/linked_list` + `book/ch02-linear-list.md`。2026-08-12 已逐条核对并实现：头结点、尾指针 O(1) append、循链定位、插入/删除、单/双链结点、Rule of Five；30 项断言与两条变异自检通过，待 Claude 复核 |
| T-004 | `ArrayStack` 改用未初始化存储 + placement new | Backlog | — | 现在用 `unique_ptr<T[]>`，会把容量内所有槽位默认构造出来——与原书 `new T[mSize]` 同样的限制，**没有恶化也没有解决**。记在这里而不是悄悄带过。见 `legacy.md` 第四节 |
| T-004a | 第 4 章模式匹配（算法4.6 / 4.7 / 4.8） | Review | Claude | `code/ch04/pattern_matching` + `book/ch04-string.md`。56 项断言（含 3000 组随机对拍），变异自检 5/5 全抓。**发现原书算法错**：算法4.6 与 4.8 的返回值 `j-pLen+1` 在 0 起始下标下一律差 1，四组数据对拍证实；另发现正文 next 数组比模式还长一位，与图4.11 矛盾。首次动用 `d001_exceptions`（`<vector>` 承载 next，附理由） |
| T-004b | 第 4 章字符串类（代码4.1 / 算法4.3 / 算法4.4 / 算法4.5） | Review | Claude | `code/ch04/string_class`。49 项断言，变异自检 5/5 全抓。原书三处硬伤：`assert(str != '\0')` 是指针与整数比较、本身编译不过；`String(char* s)` 使书中自己的例子 `String s1 = "Hello"` 在 `-Werror` 下编译失败；算法4.5 越界 `return NULL` → `strlen(nullptr)` 运行期 SEGV。**另有两条我的猜测被证据否掉并原样记进 legacy.md 第五节**（`strcmp` 同名不冲突；「编译不过」口径过强）。第 4 章至此 7/8 完成 + 1 退场 |
| T-011 | **第 5 章二叉树**（12 条清单：代码5.1/5.2、算法5.3–5.7、代码5.8、算法5.9/5.10、代码5.11/5.12） | Review | Codex | `code/ch05/binary_tree` + `code/ch05/heap_huffman` + `book/ch05-binary-tree.md`。本机 Release：34 + 18 项断言全过；ASan 空探针失败，只完成 `--allow-degraded`。请 Claude 做完整双构建与泄漏/悬垂专项变异。代码5.8 OCR 边界已定在 4105 行“删除根结点”注释后，依据见 binary_tree/legacy.md |
| T-012 | **第 3 章队列**（代码3.13/3.14/3.15 + 3.3 节比较） | Review | Codex | `code/ch03/queue`：循环队列保留原书牺牲一个槽位策略，`dequeue()` 返回 optional，链式队列补五法则；待 sanitizer 复核 |
| T-013a | 第 3 章 3.1.5 递归与栈空间（算法3.6/3.7/3.8/3.9） | Review | Claude | `code/ch03/recursion_and_stack`，10 项断言，变异自检 4/4 全抓。**原书三版都不查溢出**：`factorial(21)` 返回负数、`factorial(66)` 返回 0，UBSan 判定为未定义行为；负数静默返回 1；算法3.9 的 `s.pop(&tmp)` 与代码3.1 的 `pop(T&)` 对不上。书稿 3.1.5 开篇给出栈深度实测表（含 `-O2` 把递归转成循环这一反直觉结论，汇编确认零次自调用） |
| T-013b | 第 3 章 3.1.3 链式栈 + 3.1.4 表达式求值（代码3.4、算法3.5） | Review | Claude | `code/ch03/linked_stack`（25 项断言）+ `code/ch03/expression_eval`（24 项）。**代码3.4 又是 `top` 重名**（与代码3.2 同一处错误，同书两个存储结构都犯）；`defSize` 参数从未使用；第五次遇到"有析构无拷贝构造"。算法3.5：算法与 cin/cout 焊死；**原书正文自己说 `operand1 == 0.0` 是错的写法，印出来的代码照旧**。变异自检中**证伪了我自己的一处主张**——详见 `expression_eval/legacy.md` 缺陷 1 |
| T-013c | 第 3 章背包问题（算法3.10 / 3.11 / 3.12） | Review | Claude | `code/ch03/knapsack`，54 项断言（含承重 0..20 穷举与暴力枚举四方对拍），变异自检 5/5 全抓。**三处编译错误**：`enum rdType {0,1,2}`、`public class knapNode`（Java 语法，出现两次）、算法3.12 同时把 `stack.top` 当数据成员和成员函数用——**后者依赖代码3.2/3.4 的 `top` 重名缺陷才可能存在**。另：`w[]` 是从未声明的全局数组，解通过 `cout` 打印而非返回。**Claude 第一版显式栈实现死循环撞上闸门超时，已记入 legacy.md 第三节** |
| T-014 | 剩余章节收口：第1、6–10、12章（58 条） | Review | Codex | `code/ch01/adt`、`ch06/general_tree`、`ch07/graph`、`ch08/sorting`、`ch09/external_sort`、`ch10/search_hash`、`ch12/optimal_bst` 及对应书稿。台账已到 104/105 + 1 退场；本机仅 Release，待 sanitizer 复核 |
| T-015 | 第11章索引 | Done | Codex | `dsa_raw.md` 第11章没有 `【代码】` 或 `【算法】` 清单，故不创建虚假台账条目；索引概念不影响 105 条清单等式 |
| T-014 | **第 1、6–12 章返工**（61 条清单，分 5 批） | In progress | Codex | 人于 2026-08-12 指派。任务书 `collab/BRIEF-T014-rework.md`。返工三条理由：测试密度、`legacy.md` 零证据、第 8 章 D-001 §2 违规（快排/堆排/基数排序须手写）。**分批交接，别再一轮 61 条**——批量太大时每条清单分到的注意力必然摊薄。sanitizer 与泄漏变异仍由 Claude 补跑 |
| T-005 | 全书 292 张插图 vendoring + 逐张写图注 | Backlog | — | `tools/vendor_figures.py` 只搬字节；alt 文本必须有人看图去写，R4 会一直红着 |
| T-006 | 现代化风格公约（C++ 标准、命名、异常 vs 断言、允许用哪些 STL） | Done | 人 | **2026-08-12 人已拍板**，全文见 `collab/DECISION_LOG.md` 的 D-001。四条红线：C++17；STL 只做基础设施不做替身；容器内零 I/O、空状态用 `optional`、真错误抛标准异常；命名消除成员变量与成员函数重名。样板单元已按此重做并全绿 |
| T-007 | 原书勘误表：105 条清单里逐条标出「印刷即错」的部分 | Backlog | — | 已知 3 条：代码3.2（`int top` 与 `top()` 重名）、算法3.3（`i` 未声明）、代码3.2 无参构造未初始化成员。这份表本身对读原书的人有独立价值 |
| T-010 | 接口口径：提取用 `optional`，按键删除用 `bool`；递归周游保留并标栈溢出风险 | Done | 人 | 2026-08-12 人已拍板，见 `DECISION_LOG.md` D-001 §3c/§3d。按位置越界仍抛 `out_of_range`；BST 删除不存在键返回 false |
| T-009 | 把 `Fragile` / `ThrowingMoveAssignment` / `AllocationFailure` / `CheapMove` 抽成共享的故障注入工具头 | Done | Claude | `code/support/fault_injection.hpp`（含 `Counted`，共 5 个探针，静态成员用 C++17 inline 变量，各带 `reset()`）。`check_code.py` 加了 `-I code/`。ch03 改用共享探针后断言数不变（58），ch02 直接复用 |
| T-008 | 5 条被 OCR 吃掉「结束」标记的清单需人工定边界 | Backlog | — | 已定 1/5：代码5.8 收于 4105 行“删除根结点”注释后，4106 为新节标题；依据见 `code/ch05/binary_tree/legacy.md`。仍待：算法2.11、代码3.1、算法7.6、算法7.9 |

## Decision Log

> **决策全文在 `collab/DECISION_LOG.md`**，这里只留索引。
> 两份可编辑的决策副本一定会各自腐烂，所以这里不复制正文。

| 编号 | 日期 | 决策 | 谁拍的 |
| --- | --- | --- | --- |
| D-001 | 2026-08-12 | DSA 教材 C++ 现代化风格公约（T-006）：C++17、STL 边界、错误处理与 I/O、命名 | **人** |
| D-001 §3b | 2026-08-12 | 补充条款：新增 `const T* peek() const noexcept` 作为 `optional<T> top()` 的零拷贝补充 | **人** |
| D-001 §3c/§3d | 2026-08-12 | 提取用 optional、按键删除用 bool；递归树周游保留并明示深树栈溢出风险 | **人** |
| D-005 | 2026-08-12 | 扩容搬迁判据落在「移动赋值是否 noexcept」；由此产生的元素类型约束写成 static_assert | Codex 发现 / Claude 定案 |
| D-006 | 2026-08-12 | 闸门先自检 sanitizer 环境，环境问题用退出码 2 与代码问题区分；降级出口必须吵 | Claude 记录 |
| 退场 #1 | 2026-08-12 | 代码4.2 退场：它是标准库 `basic_string` 的空壳声明，无可现代化内容（顺带记：往 `namespace std` 加 typedef 是 UB）| Claude，见 `exclusions.json` |
| D-002 | 2026-08-12 | `dsa_raw.md` 永久只读 | Claude 记录 |
| D-003 | 2026-08-12 | 书稿代码块与源码一致性靠机器保证（R3 + sync_book） | Claude 记录 |
| D-004 | 2026-08-12 | 闸门跑 Debug+ASan/UBSan 与 Release-O2 两种构建 | Claude 记录 |

**已被取代**：2026-08-12 早些时候 Claude 提议的「C++20 + concept」被 D-001 取代为
C++17 + `static_assert`。原提议与取代理由都保留在 D-001 里，没有删除。
