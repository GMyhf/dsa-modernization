# PLAN · 唯一任务清单与决策记录

> 这是 Claude 与 Codex 共享的**唯一任务事实源**。人拍板任务与优先级；
> 两个 agent 认领任务、更新状态、署名。状态流转：`Backlog → In progress → Review → Done`。
> 每条任务用一个 `T-<编号>` 标识，交接与提交信息里引用它。
>
> **格式硬约束**：每行恰好 5 列；追加复核结论写进备注列、用 ` — ` 分隔，
> **不要**用 `| ` 另起一格——超出表头的单元格在 GitHub 渲染时被直接丢弃，
> 人看到的会是另一份 PLAN。描述里含 `|` 的代码片段要转义（`\|`）。

## 状态看板

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
| T-005 | 全书 292 张插图 vendoring + 逐张写图注 | Backlog | — | `tools/vendor_figures.py` 只搬字节；alt 文本必须有人看图去写，R4 会一直红着 |
| T-006 | 现代化风格公约（C++ 标准、命名、异常 vs 断言、允许用哪些 STL） | Done | 人 | **2026-08-12 人已拍板**，全文见 `collab/DECISION_LOG.md` 的 D-001。四条红线：C++17；STL 只做基础设施不做替身；容器内零 I/O、空状态用 `optional`、真错误抛标准异常；命名消除成员变量与成员函数重名。样板单元已按此重做并全绿 |
| T-007 | 原书勘误表：105 条清单里逐条标出「印刷即错」的部分 | Backlog | — | 已知 3 条：代码3.2（`int top` 与 `top()` 重名）、算法3.3（`i` 未声明）、代码3.2 无参构造未初始化成员。这份表本身对读原书的人有独立价值 |
| T-010 | **待人拍板**：把「`remove()` 返回 T 并抛越界 vs `pop()` 返回 optional」的口径写进 D-001 | Backlog | 人 | 三个容器已按「栈的空是常态、表的越界是错误」实现，两个 agent 独立选到同一处，但公约里没写。第 5 章树开工前定下来 |
| T-009 | 把 `Fragile` / `ThrowingMoveAssignment` / `AllocationFailure` / `CheapMove` 抽成共享的故障注入工具头 | Done | Claude | `code/support/fault_injection.hpp`（含 `Counted`，共 5 个探针，静态成员用 C++17 inline 变量，各带 `reset()`）。`check_code.py` 加了 `-I code/`。ch03 改用共享探针后断言数不变（58），ch02 直接复用 |
| T-008 | 5 条被 OCR 吃掉「结束」标记的清单需人工定边界 | Backlog | — | 算法2.11、代码3.1、代码5.8、算法7.6、算法7.9。`tools/ledger.py` 每次报告都会列出来 |

## Decision Log

> **决策全文在 `collab/DECISION_LOG.md`**，这里只留索引。
> 两份可编辑的决策副本一定会各自腐烂，所以这里不复制正文。

| 编号 | 日期 | 决策 | 谁拍的 |
| --- | --- | --- | --- |
| D-001 | 2026-08-12 | DSA 教材 C++ 现代化风格公约（T-006）：C++17、STL 边界、错误处理与 I/O、命名 | **人** |
| D-001 §3b | 2026-08-12 | 补充条款：新增 `const T* peek() const noexcept` 作为 `optional<T> top()` 的零拷贝补充 | **人** |
| D-005 | 2026-08-12 | 扩容搬迁判据落在「移动赋值是否 noexcept」；由此产生的元素类型约束写成 static_assert | Codex 发现 / Claude 定案 |
| D-006 | 2026-08-12 | 闸门先自检 sanitizer 环境，环境问题用退出码 2 与代码问题区分；降级出口必须吵 | Claude 记录 |
| 退场 #1 | 2026-08-12 | 代码4.2 退场：它是标准库 `basic_string` 的空壳声明，无可现代化内容（顺带记：往 `namespace std` 加 typedef 是 UB）| Claude，见 `exclusions.json` |
| D-002 | 2026-08-12 | `dsa_raw.md` 永久只读 | Claude 记录 |
| D-003 | 2026-08-12 | 书稿代码块与源码一致性靠机器保证（R3 + sync_book） | Claude 记录 |
| D-004 | 2026-08-12 | 闸门跑 Debug+ASan/UBSan 与 Release-O2 两种构建 | Claude 记录 |

**已被取代**：2026-08-12 早些时候 Claude 提议的「C++20 + concept」被 D-001 取代为
C++17 + `static_assert`。原提议与取代理由都保留在 D-001 里，没有删除。
