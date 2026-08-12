# HANDOFF · 交接日志

### 2026-08-12 · Codex → Claude · T-003b 链表（代码2.6–代码2.12）交复核

- **逐条核对**：七条清单原文范围 `dsa_raw.md:1450-1660`。`legacy.md` 逐条区分 OCR
  损伤与可复现缺陷：`delete` 关键字、`const Link*` → `Link*` 的非法赋值，以及
  算法2.9 每次定位的无主 `new Link(head->next)` 泄漏。
- **实现**：新增 `code/ch02/linked_list`。保留带头结点、尾指针、按位置循链 O(n)、
  定位后的指针改链 O(1)；`append` 直接经 tail 接链 O(1)。实现 Rule of Five、
  复制构造的中途失败清理、move-only 元素和 `DoublyLink` 结点类型。代码2.12 原书没有
  完整算法，本轮不虚构完整双链表。
- **变异自检**：删尾不回退 `tail_` → 后续 append 退出码 138；复制构造 catch 不清理 →
  `FAIL: 复制构造失败时已接入结点全部回收` 与 `FAIL: 链表离开作用域后不遗留元素对象`。
- **闸门结果（真实，降级）**：

  ```
  $ python3 -m unittest discover -s tests       Ran 61 tests ... OK (skipped=6)
  $ python3 -m py_compile tools/*.py tests/*.py  ✅ ok
  $ python3 tools/ledger.py --check              ✅ 台账一致：15/105 已现代化，0 退场，90 待办
  $ python3 tools/check_doc.py                   ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py --allow-degraded
    ArrayList: 47 项断言，0 失败
    LinkedList: 30 项断言，0 失败
    ArrayStack: 58 项断言，0 失败
    ✅ 3/3 个单元通过（Release-O2）
  ```

  Debug ASan/UBSan 未运行：空探针稳定报 `sanitizer_malloc_mac.inc:189`，按 D-006 已显式
  使用 `--allow-degraded`，不能把 Release 绿误称为完整内存验证。

### 2026-08-12 · Claude → Codex · T-009 共享探针 + T-003a 第 2 章顺序表

- **做了什么**：先落地 T-009（把四个故障注入探针抽成 `code/support/fault_injection.hpp`，
  多补一个 `Counted`，共 5 个），再做 T-003a 第 2 章顺序表单元
  （代码2.1 / 代码2.2 / 算法2.3 / 算法2.4 / 算法2.5，5 条清单）。

- **又抓到原书三处编译级硬伤**，比第 3 章还多一处：
  - `bool delete(const int p)` —— **`delete` 是 C++ 关键字**，代码2.1/2.2/算法2.5
    三处都用它当函数名，整章的删除操作建立在编译不过的名字上；
  - `class List { void clear(); ... };` **没写 `public:`** —— 默认 private，
    这个 ADT 的每个运算都调不到（同书第 3 章代码3.1 是写了的，体例不一致）；
  - 算法2.3 的 `for (i = 0; i < n; i++)`，**`n` 从未声明**。

  另有一处设计问题：`int position` 游标住在容器里（const 不能遍历、不能嵌套遍历），
  改为 `begin()/end()`；且该成员在书中所有算法里**一次都没被用到**。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 18.768s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：8/105 已现代化，0 退场，97 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 2/2 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayList: 47 项断言 / ArrayStack: 58 项断言，0 失败
  ```

- **变异自检 6/6 全抓**：拷贝构造退回浅拷贝 → ASan double-free；insert 不右移 →
  断言红；remove 不左移 → UBSan；越界不抛 → UBSan；搬迁判据改回 `is_copy_assignable`
  → 断言红（D-005 在 ch02 这边也守住了）；扩容失败漏 `delete[]` → LeakSanitizer。

- **书稿的图**：第 2 章顺序表一节**原书没有插图**，图2.1/2.2/2.3 在 OCR 里都是
  （乱掉的）HTML 表格。没有伪造图片引用，改成规范的 Markdown 表格重排。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码 `file=` 逐字一致 ✅ ｜
  未换 STL（仍是手写顺序表，插入/删除仍 O(n)）✅ ｜ 编号未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立（8+0+97=105）✅ ｜ 零第三方依赖 ✅

- **两处我不确定、想听你意见**（详见 NOTES-claude）：
  ① `remove()` 返回 `T`（越界抛）而第 3 章 `pop()` 返回 `optional`——同项目两种形状，
  我认为对（栈空是常态、表越界是错误），但接口口径这种事该由人拍板；
  ② `insert(pos == size())` 等价 append 的路径没有单独用例。

- **T-003b 链表（7 条清单）留给你认领**——原书那部分是裸指针 + 手工 delete 的重灾区，
  换个模型看更可能挖出我看不见的东西。我可以接第 4 章字符串。

### 2026-08-12 · Claude → Codex · T-002 复核：两处诊断认可，异常安全的修复方式返工一次

- **复核结论**：Codex 红队达标——交出两条会失败的测试，**都是真缺陷**（`cc26132`）。
  D-001 静态检查的绕过（空白变体、空白豁免理由）与 `move_if_noexcept` 用错维度，
  两条都成立。第二条正是我上一轮在 NOTES 里点名最不放心的地方。

- **返工一处**：修复判据由「可不可拷贝」改为「移动赋值抛不抛」。
  原修法让 `std::string`（移动赋值本就 noexcept）每次扩容退化成深拷贝，
  实测 64 次 push 的扩容搬迁 **63 次全是拷贝**；改后 **0 次**。
  Codex 的两条故障注入用例在新判据下照样通过。
  **补上唯一能分辨两种策略的守门用例**并反向验证（改回旧判据立刻变红）——
  两种修法都能通过当时的全部断言，这才是本轮真正的教训。

- **Codex 报告的 ASan 失败：确认为 macOS 环境问题**。同一份代码在本机 Linux 上
  Debug+ASan/UBSan 档 58 项断言全过。已做成工具自检 `sanitizer_preflight()`：
  跑单元前先试空探针，失败以**退出码 2** 与代码问题（1）区分，并给复现命令。
  降级出口 `--allow-degraded` 只在自检失败时生效，且开头与结论各喊一次。

- **改了哪些文件**：`code/ch03/array_stack/`（modern.hpp 判据、test.cpp 新守门用例、
  legacy.md 缺陷 11）、`book/ch03-stack.md`（新增「`move_if_noexcept` 在这里是错的」
  一小节）、`tools/check_code.py`（preflight + 降级）、`tests/test_check_code.py`、
  `collab/DECISION_LOG.md`（D-005 / D-006）、`collab/PLAN.md`。
  **`dsa_raw.md` 仍未动。**

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 12.437s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 58 项断言，0 失败
  ```

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码 `file=` 逐字一致（R3 本轮真的抓到一次
  漂移，改完 modern.hpp 忘了同步）✅ ｜ 未换 STL ✅ ｜ 编号未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立 ✅ ｜ 零第三方依赖 ✅

- **下一轮**：T-002 标 Done；新开 T-009（把四个故障注入探针类型抽成共享头，
  第 2 章链表会用到）。我准备接 T-003（第 2 章线性表 12 条清单），
  Codex 若想先做链表那半，在 PLAN 里认领。

### 2026-08-12 · Codex → Claude · T-002 红队：两处真缺陷已复现并修复

- **静态闸门打穿并加固**：旧 `check_d001()` 会漏 `#  include <vector>` 与
  `std :: cout`，空格理由可绕过豁免；也会将块注释/字符串里的 token 误判。现改为
  注释/字符串剥离 + 空白规范化匹配，豁免理由 `strip()` 后必须非空；9 条 D-001
  自测覆盖这些情况。
- **强异常保证真 bug**：旧版对「移动构造 `noexcept`、移动赋值第 3 次抛」的故障注入
  输出 `FAIL: redteam strong guarantee after throwing move assignment`（52 项断言，1 失败）。
  根因是 `move_if_noexcept` 看移动构造而代码实际做移动赋值。现对可拷贝 T 用复制迁移；
  不可拷贝 T 静态要求移动赋值 `noexcept`。同时补 `new T[next]` 抛 `bad_alloc` 的回归。
- **peek 复核**：不解引用失效指针是正确取舍（否则 UB）；维持 D-001 §3b 的
  `top()` 副本 / `peek()` 零拷贝双接口，不加调试世代计数。
- **闸门结果（真实）**：书稿同步后 `check_doc.py` 通过；Release `check_code.py`
  输出 `ArrayStack: 55 项断言，0 失败`。完整验证因当前 macOS ASan 运行时失败而未全绿：

  ```
  test_clean_unit_passes_both_profiles ... FAIL
  AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189
  "((!asan_init_is_running)) != (0)"
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ❌ Debug ASan 初始化失败；Release 55 项断言，0 失败
  ```

  该 ASan 错误同样发生在 `tests/test_check_code.py` 的空探针单元，不能宣称本轮完整闸门
  已通过。详见 `NOTES-codex.md` 的攻击记录与环境证据。

### 2026-08-12 · Claude → Codex · D-001 §3b `peek()` 落地；正式交红队（T-002）

- **做了什么**：人拍板补充 D-001 §3b，新增 `const T* peek() const noexcept`
  （零拷贝、空栈 `nullptr`、move-only 元素可用，代价是指针在下次 push/pop/clear 后失效）。
  公约、书稿正文、对照表、`legacy.md` 的欠账条目同步更新——欠账不是删掉，
  是改成带出处的已决记录，原文用删除线保留。
- **改了哪些文件**：`code/ch03/array_stack/`（modern.hpp / test.cpp / legacy.md）、
  `book/ch03-stack.md`、`collab/DECISION_LOG.md`（新增 §3b）、`collab/PLAN.md`、
  `collab/REDTEAM-BRIEF-T002.md`（新建，红队任务书）。**`dsa_raw.md` 仍未动。**
- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 54 tests in 6.220s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 50 项断言，0 失败
  ```

- **peek 的两条守门用例经变异验证**：退化成拷贝实现 → move-only 处**编译期**即挂
  （`unique_ptr` 拷贝赋值 deleted）；用 `if constexpr` 隔离后
  `FAIL: peek 一次拷贝都不做` 变红。空栈不返回 `nullptr` → UBSan 报
  `applying non-zero offset to null pointer`。
- **轮到 Codex**：任务书 `collab/REDTEAM-BRIEF-T002.md`，三条主攻方向按人的指示排定。
  成功标准写死为「**至少交出一条会失败的测试**」；攻不动也算结论，但要带攻击记录。
  我自己最不放心的是第二条：`move_if_noexcept` 看的是移动**构造**是否 noexcept，
  而扩容里元素是被**赋值**进去的——这条推理至今无测试守住。

### 2026-08-12 · Claude → Codex · D-001 公约落地：样板单元由 C++20 重做为 C++17

- **做了什么**：人拍板了 T-006 现代化风格公约（全文落在新建的 `collab/DECISION_LOG.md`
  D-001），按它把样板单元整个重做：C++20 concept → C++17 `static_assert` +
  `<type_traits>`；`unique_ptr<T[]>` → 裸 `T* data_` + 显式五法则；
  `top()`/`pop()` 统一返回 `optional`；越界抛 `out_of_range`、容量溢出抛 `overflow_error`。
  书稿 3.1.1/3.1.2/3.1.3 三节论述随之重写，代码块由 `sync_book.py --write` 重新灌入。

- **改了哪些文件**：`collab/DECISION_LOG.md`（新建）、`collab/PLAN.md`（T-006 → Done，
  Decision Log 改为索引指向 DECISION_LOG，避免两份副本各自腐烂）、`collab/README.md`、
  `code/ch03/array_stack/`（modern.hpp / test.cpp / legacy.md / unit.json 全改）、
  `book/ch03-stack.md`、`tools/check_code.py`（新增 D-001 静态检查）、
  `tools/ledger.py`（默认标准 c++17）、`tools/handoff.py`（检查清单加 D-001 一条）、
  `tests/test_check_code.py` `tests/test_handoff.py`、`CLAUDE.md`。
  **`dsa_raw.md` 仍然一字未动。**

- **闸门结果**（`python3 tools/handoff.py --verify`，退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 54 tests in 6.949s  OK
  $ python3 -m py_compile <11 files>           ✅ ok
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 38 项断言，0 失败
  ```

- **变异自检 5/5 全抓**（都是断言或 sanitizer 抓的，不是编译期）：
  拷贝构造改成抄指针 → `AddressSanitizer: double-free`；
  扩容失败漏 `delete[] fresh` → `LeakSanitizer`；
  放弃先建后换 → Debug 档 UBSan 空指针引用、Release 档段错误；
  删掉深拷贝循环 → `FAIL: 拷贝赋值得到独立副本`；
  越界不抛 → `FAIL: 越界读取抛 std::out_of_range`。

- **上一轮挂着的账已销**：`ensure_capacity()` 的强异常保证从「只做了推理」变成
  **故障注入实测**（`Fragile` 第 3 次拷贝赋值必抛）。原话保留在 NOTES 里，标注了销账。

- **公约不只是文字，有机器守着**：`check_code.py` 新增 D-001 静态检查——`modern.*` 里
  出现 `<iostream>` / `std::cout` / STL 容器头文件即红；豁免写进 `unit.json` 的
  `d001_exceptions`，**键是被豁免的写法、值是理由**。6 条新测试守这个检查本身。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码全部 `file=` 引用且逐字一致 ✅ ｜
  仍是手写数组栈 + 翻倍扩容，没换 STL ✅ ｜ 编号与交叉引用未漂 ✅ ｜
  缺陷条条有证据（证据命令已按 C++17 重跑）✅ ｜ 台账等式成立 ✅ ｜ 零第三方依赖 ✅

- **请你重点看**：① T-002 红队，新靶子是 D-001 静态检查（逐行正则，块注释与字符串
  里的关键字我没处理）；② `Fragile` 只覆盖「拷贝赋值抛」，**移动赋值抛与
  `new T[next]` 抛 `bad_alloc` 都没造过**；③ `top()` 返回副本对 move-only 元素不可用，
  要不要另加 `const T* peek()`。

### 2026-08-12 · Claude → Codex · T-000 脚手架 + T-001 顺序栈样板，交首轮复核

- **做了什么**：从零搭起 Claude⇄Codex 协作脚手架（移植自 `cs101.openjudge.cn/collab`，
  闸门按本项目重写），并用第 3.1 节顺序栈做了**一个跑通全流程的样板单元**——
  脚手架如果没在真内容上跑过，它证明不了任何事。

- **改了哪些文件**：
  `collab/`（README/PLAN/HANDOFF/NOTES×2/exclusions.json）、
  `tools/`（`handoff.py` `ledger.py` `check_doc.py` `check_code.py` `sync_book.py`
  `vendor_figures.py` `repo.py`）、`tests/`（4 个测试文件，48 项）、
  `code/ch03/array_stack/`（unit.json / legacy.md / modern.hpp / test.cpp）、
  `book/ch03-stack.md` + `book/assets/`（1 张图已 vendoring）、
  `CLAUDE.md`、`README.md`、`.gitignore`。
  **`dsa_raw.md` 一字未动**（红线 1）。

- **闸门结果**（`python3 tools/handoff.py --verify`，退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 48 tests in  8.655s  OK
  $ python3 -m py_compile <11 files>           ✅ ok
  $ python3 tools/ledger.py --check            ✅ 台账一致：3/105 已现代化，0 退场，102 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：1 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 1/1 个单元通过（debug+asan+ubsan, release-O2）
                                                  ArrayStack: 29 项断言，0 失败
  ```

- **变异自检 5/5 全抓**（把实现改回原书行为，闸门必须变红）：深拷贝不拷元素、
  翻倍改 `+1`、空栈 pop 不返回 nullopt、容器里恢复 `cout`、`clear()` 扔掉容量。
  逐条结论在 `NOTES-claude.md`。

- **本轮最硬的一个发现：原书这三条清单里有两条按印刷原样根本编译不过。**
  - 代码3.2：`int top` 与成员函数 `bool top(T&)` 重名 →
    `error: ‘bool arrStack<T>::top(T&)’ conflicts with a previous declaration`；
  - 算法3.3：扩容循环的 `i` 从未声明 → `error: ‘i’ was not declared in this scope`。

  另外实测到两处未定义行为：无参构造留下未初始化的 `mSize`/`st`
  （`-Wall -Wextra -Wpedantic` **一句警告都不给**），以及违反三法则导致的二次释放
  （ASan 报告已抄进 `legacy.md`）。命令与完整输出都在 `code/ch03/array_stack/legacy.md`，
  可逐条重跑。

- **红线自检**：`dsa_raw.md` 未动 ✅ ｜ 书稿代码全部 `file=` 引用且 R3 逐字一致 ✅ ｜
  没有换成 STL 封装（仍是手写数组栈 + 翻倍扩容）✅ ｜ 编号与交叉引用未漂 ✅ ｜
  缺陷条条有证据 ✅ ｜ 台账等式成立（3 + 0 + 102 = 105）✅ ｜ 零第三方依赖 ✅

- **请你重点看**（详见 `NOTES-claude.md`）：
  1. **T-002 就是给你的**：找能溜过 `--verify` 的坏改动。我自己怀疑的三个入口
     （R2 剥离器的盲区、R3 的 dedent 放宽、台账不校验「真的实现了」）已写在 NOTES。
  2. `ArrayStack::ensure_capacity()` 的强异常保证我**只做了推理，没做故障注入**。
     要写会失败的测试，建议从这里下手。
  3. `std::move_if_noexcept` 在「赋值而非构造」语义下是否还有意义，我不完全确定。

- **仍然挂着**：T-006 现代化风格公约需要人拍板（C++ 标准、命名、异常 vs 断言、
  允许用哪些 STL）。样板单元已经隐含了一套取值，写在 PLAN 的 Decision Log 里，
  **标注的是「Claude 提议，待人确认」，不是既成事实**。在人确认之前，
  第二个单元最好别开工，否则每个单元各行其是。
