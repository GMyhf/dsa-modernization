# 协作脚手架 · Claude ⇄ Codex

两个 AI（Claude Code 与 Codex）不能靠"记忆"协作，只能靠**共享事实源**交接。
这个目录就是那层事实源：谁都能读、谁都能写、每一轮都留下书面痕迹。

## 这个项目在做什么

把《数据结构与算法》（张铭、王腾蛟、赵海燕，高等教育出版社 2008）从一份 OCR 底稿
（`dsa_raw.md`）整理成一部**代码能编译、能跑、经得起现代工程规范检验**的教材。

原书成书于 2008 年，C++ 代码是典型的传统高校教材风格：裸 `new[]`、缺析构、
违反三法则、用出参 + `bool` 双通道返回、容器里直接 `cout` 打中文提示、`int` 当下标。
其中不少**根本编译不过**——第一个样板单元就抓到了两处（详见
`code/ch03/array_stack/legacy.md`）。现代化要做的是：保住原书的教学内容与编号，
换掉那套写法，并且让「换对了」这件事可被机器复验。

## 文件职责

| 文件 | 作用 | 谁写 |
| --- | --- | --- |
| `PLAN.md` | 唯一任务清单；决策只留索引 | 人拍板；两个 agent 更新状态 |
| `DECISION_LOG.md` | **已生效决策的权威出处**（含 D-001 代码风格公约） | 人拍板后落笔；推翻要追加不要删除 |
| `HANDOFF.md` | 交接日志：每一次「我做完了，轮到你」都追加一条 | 交接方 |
| `NOTES-claude.md` | Claude 留给 Codex 的话（改了什么、哪里没把握） | 只有 Claude |
| `NOTES-codex.md` | Codex 留给 Claude 的话（审查意见、发现的问题） | 只有 Codex |
| `exclusions.json` | 决定**不做**的清单及其理由、署名、日期 | 谁做的决定谁写 |
| `review-input.md` | 脚本自动生成的 review 包（**不入库**） | `tools/handoff.py` |

> `git` 是最硬的桥梁，**编译器和 sanitizer 是最硬的仲裁**。文档负责「为什么」和
> 「接下来」，代码与测试负责「是什么」。冲突时，能跑通验证的方案胜出。
> `python3 tools/handoff.py --verify` 会依次跑：工具自测 → 语法检查 → 台账一致性 →
> 书稿体检 → 全部代码单元在 `-Werror` + ASan/UBSan 下真编译真运行。

## 仓库结构

```
dsa_raw.md                    OCR 底稿，**只读**（1MB，11978 行，105 条清单）
book/                         现代化后的书稿；插图落在 book/assets/
code/<章>/<单元>/             一个清单单元
  ├── unit.json               认领了原书哪几条清单、C++ 标准、负责人
  ├── legacy.md               原书写法 → 逐条缺陷（附可复现证据）→ 现代写法
  ├── modern.hpp              实现（带 // >>> 锚点，供书稿引用）
  └── test.cpp                自带断言的测试，退出码非 0 即失败
tools/                        闸门与脚手架（纯标准库，无第三方依赖）
tests/                        闸门自身的单元测试
collab/                       本目录
```

## 一轮标准循环

```
1. 人：把目标写进 collab/PLAN.md（Backlog 里加一条任务）
2. 实现方（如 Claude）：
     - 认领任务 → 改 PLAN.md 状态为 In progress，署名
     - 实现 → `python3 tools/handoff.py --verify` → git commit（小步、清晰 message）
     - 写 NOTES-claude.md：做了什么 / 哪里没把握 / 想让对方重点看哪里
     - 追加一条 HANDOFF.md 交接记录
     - 运行 python3 tools/handoff.py --from claude --to codex
3. 人：把生成的 collab/review-input.md 交给 Codex（或让 Codex 直接读仓库）
4. 审查方（Codex）：
     - 读 review-input.md → 审查 / 挑 bug / 写会失败的测试
     - 把意见写进 NOTES-codex.md；能直接修的就修 + commit
     - 追加一条 HANDOFF.md 交接记录，轮回给 Claude
5. 实现方：git pull → 看对方 commit 与 NOTES → 继续迭代
6. 验证全绿 + 双方无异议 → 在 PLAN.md 标 Done，写进 Decision Log（如有决策）
```

## 做一个清单单元的标准动作

```bash
python3 tools/ledger.py --pending                    # 还有哪些清单没人认领
mkdir -p code/ch02/array_list && cd $_               # 建单元目录
# 写 unit.json / legacy.md / modern.hpp / test.cpp
python3 tools/check_code.py code/ch02/array_list     # 真编译真跑
python3 tools/sync_book.py --write                   # 把源码灌进书稿代码块
python3 tools/check_doc.py                           # 书稿体检
python3 tools/handoff.py --verify                    # 完整闸门
```

`legacy.md` 里每条缺陷都要**附可复现的命令与真实输出**（编译器报错、ASan 报告），
不接受「这样写不好」这种没有出处的判断。样板见 `code/ch03/array_stack/legacy.md`。

**动手前先读 `DECISION_LOG.md` 的 D-001**：C++17、STL 的允许与禁用范围、
错误处理与 I/O 规范、命名规则。那是人拍板的公约，不是建议。

## 协作模式（按需选）

- **生成 ↔ 审查**：一方写实现，另一方交叉审查。不同模型盲点不同，能抓到单模型漏掉的问题。
- **规划 ↔ 执行**：一方拆任务写 PLAN，另一方逐条实现，偏差写回 NOTES。
- **红队 / 对抗**：关键判据（闸门本身、扩容的异常安全、边界条件）由另一方专门找茬、
  写会失败的测试。
- **分工并行**：按章切分，各用 git 分支或 `git worktree` 隔离，避免踩同一段代码。

## 硬约束（避免互相覆盖）

- 开工前先在 `PLAN.md` 认领任务并署名；**不要两个 agent 同时改同一文件的同一段**。
- 小步提交、清晰 commit message，审查方才看得懂 diff。
- 交接格式统一走 `HANDOFF.md` 模板，减少人工搬运。
- **交回时必须附一次真正跑完的验证结果**（`--verify` 输出必须包含各步的尾部计数）。
  不接受「我觉得没问题」。
- **交付后回来销账：任务落地时，把它回答掉的「未决 / 待拍板 / TODO」逐条改成带出处的已决记录。**
  保留原问题、注明最终取值与代码出处，不要删除，让来回可查。
  两个 agent 每轮都读这些文档，一份多数已决的待办清单会让人重开已经关掉的方向。

## 本项目红线（审查时必查）

1. **`dsa_raw.md` 只读**。它是 OCR 原始底稿，是「原书到底怎么写的」唯一凭据。
   任何修订落在 `book/`；一旦改了底稿，就再也说不清哪些是原书的错、哪些是我们的错。
   `tests/test_ledger.py` 用「105 条清单 / 70 算法 / 35 代码 / 5 条缺结束标记」做锚，
   底稿被动过会立刻变红。
2. **书上印的代码 = 能跑的代码**。书稿里的 C++ 一律用 ```cpp file=... 引用 `code/` 下
   的真实文件，由 R3 逐字核对。**不许为了排版好看手改块内容**——那正是原书错误
   能印进教材的原因。改完代码跑 `tools/sync_book.py --write`。
3. **现代化不是「换成 STL 调用」**。这是数据结构教材：手写的链表、栈、树本身就是
   教学内容。`std::stack` 的薄封装等于把这一节删了。改的是所有权、异常安全、
   接口设计、可测试性，**不是**把实现换掉。
   这条与「容器内零 I/O」由 `check_code.py` 的 **D-001 静态检查**守着：
   `modern.*` 里出现 `<iostream>` / `std::cout` / STL 容器头文件即红。
   确有必要的豁免写进 `unit.json` 的 `d001_exceptions`，**键是被豁免的写法，值是理由**。
4. **教学内容与编号不许漂**。章节号、算法/代码编号、图号与原书一致；
   正文的「见算法3.3」「如图3.2」必须指得到东西（R5/R6/R7）。
5. **每条缺陷都要有证据**。`legacy.md` 里的每一条都附命令与真实输出。
   「不符合现代规范」不是证据，`error:` 和 `AddressSanitizer:` 才是。
6. **测试要能抓回归**。新写的 `test.cpp` 用例必须满足：**把实现改回原书的写法，
   这里要有一条会红**。提交前做一次变异自检（故意改坏一处，确认闸门变红）。
7. **不许悄悄少做**。清单要么被某个 `code/**/unit.json` 认领，要么进
   `exclusions.json` 并写明理由、署名、日期。`tools/ledger.py --check` 守着
   「已覆盖 + 退场 + 待办 = 105」这条等式。
8. **零第三方依赖**。`tools/` 只用 Python 标准库，`code/` 只用标准 C++。
   引入依赖或构建系统（CMake 等）属于架构决策，先在 PLAN 里由人拍板。

## 生成 review 包

```bash
python3 tools/handoff.py --from claude --to codex          # 默认：未提交改动 or 最近一次提交
python3 tools/handoff.py --from claude --to codex --base main   # main..HEAD 的全部改动
python3 tools/handoff.py --from codex --to claude --range HEAD~3..HEAD --verify
```

生成 `collab/review-input.md`：包含改动摘要、changed files、完整 diff、交接方 NOTES、
PLAN 里的未决项，以及一份针对本项目的 review 检查清单。把这个文件喂给另一方即可。
