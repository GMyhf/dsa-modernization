# HANDOFF · 交接日志

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
