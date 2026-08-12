# HANDOFF · 交接日志

### 2026-08-12 · Claude → Codex · T-004b String 类；第 4 章收完（7/8 + 1 退场）

- **原书三处硬伤，都有编译器/sanitizer 输出**：
  ① `assert(str != '\0')` **本身编译不过**（`'\0'` 是 char，指针与整数比较是 ill-formed），
  而且改成 `!= nullptr` 也是无效断言——`new` 失败抛 `bad_alloc`，从不返回空指针；
  ② `String(char* s)` 让书中自己写的 `String s1 = "Hello";` 在 `-Werror` 下编译失败
  （字面量是 `const char[6]`，C++11 起不能转 `char*`）；
  ③ 算法4.5 越界 `return NULL`——返回类型是 `String`，NULL 走 `String(char*)`
  于是 `strlen(nullptr)`，**能编译**，运行期 UBSan + ASan 当场 SEGV。

- **我这轮错了两次，都原样记进 `legacy.md` 第五节而不是删掉**：
  ① 猜「算法4.3 的 `strcmp` 与标准库同名会冲突」——实测能编译能链接，**不成立，
  没写进缺陷清单**；② 说「`String s1 = "Hello"` 编译不过」——口径过强，
  GCC 默认只警告，只有 `-Werror` 下才是错误。
  另有一处我自己的过度断言已改：代码4.1 只有声明没有函数体，
  我无从证明原书 `append` 「会把结果丢掉」，只能断言签名含混。

- **变异自检 5/5，但前三次撞上「编译期假象」**（被 `-Wunused-variable`、
  `-Wtype-limits` 挡下，不是被断言抓的）。重做干净版本后确认：
  拷贝构造抄指针 → ASan heap-use-after-free；append 漏 `delete[]` → LeakSanitizer；
  append 改成"返回副本、本串不变" → 断言链抛 out_of_range；
  substr 越界返回空串 → `FAIL: pos 越界抛 out_of_range`；
  substr 不截断 → ASan heap-buffer-overflow。
  **教训（第二次记）**：`-Werror` 越严，变异越容易被无关编译错误挡下，伪造出"有牙"的假象。

- **补了上一轮那条差一错误的第三重佐证**：原书 4.3 节开头的约定
  「P和T的第一个字符都从位置0开始」（`dsa_raw.md:3246`）——白纸黑字，
  与 `return (j - pLen + 1)` 直接冲突。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 6.690s  OK
  $ python3 tools/ledger.py --check            ✅ 22 已现代化，1 退场，82 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：3 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 5/5 个单元通过（双构建）
     ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56 / String 49
  ```

- **一处设计取舍想让你看一眼**：移动声明 `noexcept` 就不能在里面分配，
  所以被移动方 `data_` 置空、读取路径统一走私有 `raw()`（空时返回静态 `""`）。
  代价是所有读 `data_` 的地方都必须走 `raw()`——我漏过一次（拷贝构造从
  可能为空的 `other.data_` memcpy，即使长度 0 也是 UB），已修。请复核这个取舍。

- **第 4 章至此 7/8 + 1 退场。** 下一步建议第 5 章二叉树（12 条清单）——
  那是全书最大的一章之一，且树的删除/析构是裸指针的另一个重灾区。你想接就认领。

### 2026-08-12 · Claude → Codex · T-004a 第 4 章模式匹配：发现原书**算法结果错**

- **本轮最重的一条不是写法问题**：原书【算法4.6】朴素匹配与【算法4.8】KMP，
  匹配成功时都写 `return (j - pLen + 1);`，而 0 起始下标下正确的是 `j - pLen`。
  拿标准库 `find` 对拍，四组数据**每组恰好多 1**：

  ```
  T=abc                              P=abc          原书=  1  正确=  0
  T=xabc                             P=abc          原书=  2  正确=  1
  T=aaab                             P=ab           原书=  3  正确=  2
  T=abcddabcababcdaabcababcdaabcabaa P=abcdaabcab   原书= 11  正确= 10
  ```

  最后一组正是书中图4.12 自己演示 KMP 用的那对串——原书逐趟画了过程却没给返回值，
  错误因此在书里没有暴露。不是 OCR：两处独立印出、写法一致，而同段的
  `j = j - i + 1` 恰恰证明作者用的就是 0 起始下标。

- **这条改变了测试写法**：所有匹配用例都拿 `std::string_view::find` 逐个对拍，
  外加 3000 组随机对拍。只断言「找到了」的测试在原书那份实现下同样全绿——等于没测。

- **第二处：正文的 next 数组比模式还长一位。** 正文写 11 个值，模式只有 10 个字符，
  图4.11 给的是 10 个。算法实算站在图这一边。**是印刷错误还是 OCR 多插，分辨不出**，
  legacy.md 如实写明，只断言三者不能同时成立。

- **两个机制首次投入使用**：
  ① `d001_exceptions`（`<vector>` 承载 next 以修掉原书 `new int[]` 的泄漏，附理由）
  ——你红队时加的「理由 `strip()` 非空」检查正在守它；
  ② `exclusions.json`（代码4.2 退场：它是标准库 `basic_string` 的空壳声明，
  无可现代化内容；顺带记了往 `namespace std` 加 typedef 是 UB）。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 9.646s  OK
  $ python3 tools/ledger.py --check            ✅ 18 已现代化，1 退场，86 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：3 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 4/4 个单元通过（双构建）
                          ArrayList 47 / ArrayStack 58 / LinkedList 33 / PatternMatching 56
  ```

- **变异自检 5/5，其中一条返工过**：「去掉 next 优化」第一次是被**未使用变量的编译错误**
  挡下的，不是被断言抓的——重做了干净的变异才确认 `test_next_matches_the_book_figure`
  真的会红。变异自检本身也会做假，得看清是被什么抓的。

- **T-004b 留给你认领**（代码4.1 / 算法4.3 / 4.4 / 4.5，String 类）。证据已采好：
  `String::String(char* s)` 让书中自己的例子 `String s1 = "Hello";` 在 C++11 起编译不过；
  算法4.5 从返回 `String` 的函数里 `return NULL`；算法4.3 的 `strcmp` 与标准库同名同签名。

### 2026-08-12 · Claude → Codex · T-003b 复核：ASan 双构建通过；补 O(1) 守门 + 返工 skip 粒度

- **你交给我的那件事已完成**：三单元双构建全绿，LinkedList 在 Debug+ASan/UBSan 档
  33 项断言通过。你的 macOS 环境诊断成立，实现本身没有内存问题。

- **链表专属的变异自检 5/5 全抓**（这组只有 sanitizer 能证，是本轮我能做而你做不了的）：
  析构不清链 / `remove` 不 delete / `clear` 只断链 / 拷贝构造失败不回收半截链
  → 四条全部 **LeakSanitizer**；`swap` 不回填哨兵尾指针 → **UBSan 空指针成员访问**。
  `fix_sentinel_tail` 与 `take_from` 的求值顺序我逐例推演 + 变异验证，都对。

- **补一处：`append` 的 O(1) 没有守门用例。** 你自己抓到并修了「`append` 转调
  `insert(size_)` 会循链」，但现有 31 条断言在 O(1) 与 O(n) 两种实现下表现完全相同——
  差别只在复杂度。补 `test_append_does_not_walk_the_chain`（30 万次 append，
  O(1) 档 0.208 秒）。反向验证：改回循链版本，闸门报 **`❌ 超过 120s 未结束`**。
  用例注释里写明了局限：它证明「不随表长线性变慢」，不证明严格 O(1)。

- **返工一处：闸门自测的 skip 粒度过宽。** `@unittest.skipUnless(SANITIZER_AVAILABLE)`
  加在整个 `TestGateHasTeeth` 类上，而其中两条（断言失败要被报出、`-Werror` 要生效）
  与 sanitizer 无关——整类 skip 使你在 macOS 上对「闸门有没有牙」变成**零覆盖**。
  改为 skip 下沉到依赖 sanitizer 的三条，`run_gate()` 环境不可用时自动带
  `--allow-degraded`。打桩模拟验证：3 skip、2 通过。

- **闸门结果**（退出码 0）：

  ```
  $ python3 -m unittest discover -s tests      Ran 61 tests in 19.524s  OK
  $ python3 tools/ledger.py --check            ✅ 台账一致：15/105 已现代化，0 退场，90 待办
  $ python3 tools/check_doc.py                 ✅ 书稿体检通过：2 个文件，7 条规则
  $ python3 tools/check_code.py                ✅ 3/3 个单元通过（debug+asan+ubsan, release-O2）
                                     ArrayList 47 / ArrayStack 58 / LinkedList 33 项断言，0 失败
  ```

- **需要人拍板的一条**：`remove()` 返回 `T`（越界抛）与 `pop()` 返回 `optional`
  是两种形状，三个容器都按「栈的空是常态、表的越界是错误」这条隐含口径实现，
  两个 agent 各自独立选到了同一处——但 D-001 里没写。建议补进公约，
  免得第 5 章树一开工又各选各的。

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
