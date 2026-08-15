# dsa-modernization

把《数据结构与算法》（张铭、王腾蛟、赵海燕编著，高等教育出版社 2008，
普通高等教育"十一五"国家级规划教材）从一份 OCR 底稿，整理成一部**代码能编译、
能跑、经得起现代工程规范检验**的教材。

原书的 C++ 是典型的 2008 年高校教材风格：裸 `new[]`、缺析构、违反三法则、
用出参 + `bool` 双通道返回、容器里直接 `cout` 打中文提示、`int` 当下标。
其中不少**根本编译不过**。

现代化要保住的是原书的教学内容、编号与讲法；换掉的是那套写法；
而「换对了」这件事**由机器复验，不靠自觉**。

## 现状

```
台账   104 已现代化 / 1 退场 / 0 待办  = 105 条清单
书稿   16 个文件（12 章正文 + 总目录 + 习题 + 勘误 + 插图），8 条规则通过
成品   PDF（book/pdf/）与网页版（book/site/，双击 index.html 即可读）
代码   32 个单元 × 2 种构建（Debug+ASan/UBSan、Release-O2）
自测   115 项（闸门自己的单元测试）
```

`python3 tools/handoff.py --verify` 退出码 0。

**但这行绿色只证明了一半的事——另一半在[下面第「闸门证明不了什么」节](#闸门证明不了什么)。**

## 抓到了什么

全书 105 条清单，二十余处**编译级硬伤**，每一处都附了编译器或 sanitizer 的真实输出
（逐条见各单元的 `legacy.md`）：

| 缺陷 | 出处 |
| --- | --- |
| `delete` 当成员函数名 | 代码2.1 / 代码2.2 / 算法2.5 |
| `int top` 与 `bool top(T&)` 重名，整类编译不过 | 代码3.2、代码3.4 |
| `stack.top` 同时当数据成员和成员函数用——**依赖上一条缺陷才可能存在** | 算法3.12 |
| 模式匹配返回位置**一律差 1**（书中图4.12 自己的例子就错） | 算法4.6、算法4.8 |
| `assert(str != '\0')` 是指针与整数比较，本身编译不过 | 算法4.4 |
| `enum rdType {0,1,2}`（枚举值不能是字面量）、`public class`（**Java 语法**） | 算法3.11 |
| `factorial(21)` 返回负数、`factorial(66)` 返回 0，UBSan 判为未定义行为 | 算法3.6/3.8/3.9 |
| `class List` 无 `public:`，ADT 的每个运算都调不到 | 代码2.1 |

第 4 章 4.3 节开头明写「P和T的第一个字符都从位置0开始」，而两个匹配算法都返回
`j - pLen + 1`——**书里的约定与代码自相矛盾，且从未被暴露**，因为原书只画了匹配过程、
没给返回值。

## 快速开始

```bash
python3 tools/handoff.py --verify     # 完整闸门：工具自测 → 台账 → 书稿 → 真编译真运行
python3 tools/ledger.py               # 105 条清单现在做到哪了
python3 tools/check_code.py           # 只跑 code/：-Werror + ASan/UBSan + O2 双构建
python3 tools/check_doc.py            # 只跑 book/：OCR 残留、编号、插图、代码块一致性
python3 tools/build_site.py           # 把书稿渲染成网页版 book/site/，入口 index.html
```

网页版在线可读：**<https://gmyhf.github.io/dsa-modernization/>**。
本地读就双击 `book/site/index.html`，或 `python3 -m http.server -d book` 后打开
`http://localhost:8000/site/`。它是 `book/*.md` 的产物，闸门里有一条
`build_site.py --check` 盯着两者不许脱节。

### 怎么更新这本在线书

改书稿 → 重建 → 提交推送，三步，剩下的由 CI 做：

```bash
# 1. 改 book/*.md（或改 code/ 后跑 tools/sync_book.py --write 把源码灌回书稿）
# 2. 重建网页版并自检
python3 tools/build_site.py
python3 tools/handoff.py --verify      # 含 build_site.py --check 这一步
# 3. 提交推送
git add -A && git commit -m "..." && git push
```

推送到 `main` 且改动落在 `book/**` 或 `tools/build_site.py` 时，
`.github/workflows/pages.yml` 会**从 Markdown 现场重建**站点并发布到 GitHub Pages
（也可以在 Actions 页面手动 Run workflow）。发布的是现场构建的版本，不是仓库里那份
`book/site/`——所以线上永远是书稿本身说的话；仓库里那份若过期，CI 会打一条 warning，
本地闸门会直接报红。

发布目录与仓库里的目录形状不同（页面摆在站点根上、插图在 `assets/`），
理由与代价见 `collab/DECISION_LOG.md` 的 D-011。

需要 Python 3（仅标准库）与 g++/clang++（支持 C++17 与 sanitizer）。无第三方依赖。

## 仓库结构

| 路径 | 是什么 |
| --- | --- |
| `dsa_raw.md` | OCR 底稿，**只读**。1MB / 11978 行 / 12 章 / 105 条清单 / 292 张外链插图 |
| `book/` | 现代化后的书稿：12 章正文 + [总目录](book/现代C++数据结构教程.md) + [原书勘误](book/勘误.md) + [插图](book/插图.md)。292 张图在 `book/assets/`。发给学生的带书签 PDF：[`book/pdf/现代C++数据结构教程.pdf`](book/pdf/现代C++数据结构教程.pdf)（`python3 tools/build_book_pdf.py` 重编）；浏览器版：[`book/site/index.html`](book/site/index.html)（`python3 tools/build_site.py` 重编） |
| `code/<章>/<单元>/` | 一个清单单元：`unit.json`（认领哪几条清单）、`legacy.md`（原书写法→缺陷证据→现代写法）、`modern.hpp`、`test.cpp` |
| `code/support/` | 各章测试共用的故障注入探针（只放探针，不放任何数据结构实现） |
| `tools/` | 闸门与脚手架，纯标准库 |
| `tests/` | 闸门自身的单元测试，115 项 |
| `collab/` | 协作事实源：PLAN / DECISION_LOG / HANDOFF / 双向 NOTES / 退场记录 |

## 四条闸门

1. **台账**（`ledger.py`）——原书 105 条清单，每条要么被某个 `code/` 单元认领，
   要么在 `collab/exclusions.json` 里带理由退场。**没有第三种状态**，
   「已覆盖 + 退场 + 待办 = 105」这条等式由脚本守着。
2. **书稿体检**（`check_doc.py`）——7 条规则拦 OCR 残留、假语言标签、断掉的交叉引用、
   热链插图。其中最硬的 R3：书稿里的每段 C++ 必须用 ` ```cpp file=... ` 引用 `code/`
   下的真实文件并**逐字一致**。书上印的代码就是跑过的那份代码。
3. **代码**（`check_code.py`）——每个单元在 `-Wall -Wextra -Wpedantic -Werror` 下
   编译两遍：Debug + ASan/UBSan，以及 Release -O2。两遍都要真跑起来、断言全过。
   （实测：同一段越界 UB 在 `-O2` 那档是**静默通过**的——这就是为什么要跑两种构建。）
4. **实质性检查**（`check_code.py` 的 `check_substance`）——每条清单至少 3 项断言，
   `legacy.md` 至少 20 行且含可复现证据。**没有豁免字段**，这是有意的：
   一旦开了逃生口，最先用它的就是最该被拦下的那类提交。

> 这四条不是设计出来的，是被真事逼出来的。第 4 条来自一次**结构合规、内容近乎为空**
> 的提交：542 行覆盖 61 条清单，第 8 章 17 条排序清单只有 11 项断言，
> 而 `quick()` 是 `std::sort`、`heap()` 是 `std::make_heap`。
> 详见 `collab/DECISION_LOG.md` 的 D-007。

---

## 闸门证明不了什么

**这一节是这份 README 里最该读的部分。** 所有的绿——退出码 0、19/19 单元、
几百项断言——都只证明了「被测试走到的那些路径，在这台机器上、这次构建里没出问题」。
完整版（含可复现的测量程序）在 [`collab/UNVERIFIED-RISKS.md`](collab/UNVERIFIED-RISKS.md)，
下面是必须先知道的几条。

### 1. 递归深度：一个量出来的数字，和一件更要紧的事

第 5 章按公约保留了递归实现（递归结构本身是教学内容），代价是**树高受进程调用栈限制**。
实测（Linux，gcc 13.3，`ulimit -s` 8MB，纯左链）：

| 构建档 | 递归析构 | 递归前序周游 |
| --- | --- | --- |
| Release `-O2` | 50 万通过 · **100 万 SIGSEGV** | 50 万通过 · **100 万 SIGSEGV** |
| Debug + ASan/UBSan | 50 万通过 · **100 万 stack-overflow** | **50 万即 stack-overflow** |

**更要紧的是出事时你看到什么**：ASan 档会打印
`AddressSanitizer: stack-overflow` 并给出精确到行号的递归回溯；
**Release 档只有一个段错误，一行解释都没有。**

所以「没跑 ASan」不是「覆盖少一点」——这个风险一旦发作，只跑 Release 的人
拿到的是一个无法解释的崩溃。

另外两条容易被忽略：`destroy` 与 `clone` 是**隐式触发**的递归（一次普通析构、
一次拷贝就会走到），而**它们没有迭代版本**；退化 BST 的有序插入是 O(n²)，
压测时容易被误判成死循环。

第 3 章还量到一条反直觉的：**同一份递归源码，`-O0` 在 50 万层崩、`-O2` 100 万层不崩**——
因为编译器把递归转成了循环（汇编确认零次自调用）。
「这段递归会不会爆栈」不是源码单独决定的，是源码 × 编译器 × 优化档共同决定的。

### 2. 有代码从未被任何测试走到

`HuffmanTree` 建叶子的 `catch (...) { delete leaf; throw; }` 至今没有用例走到——
只有单对象 `new` 分配失败才可达，而现有探针是按 `operator new[]` 设计的。
删掉它闸门照样全绿。

### 3. 有单元在作者机器上从没跑过 sanitizer

Codex 所在的 macOS 环境，ASan 连**空探针程序**都起不来
（`sanitizer_malloc_mac.inc:189`）。`code/ch02/linked_list`、`code/ch05/*`
以及第 1、6–12 章由它编写时**只跑过 Release**，sanitizer 是事后由另一方补跑的。

补跑结果都通过，且各做过泄漏/悬垂专项变异（链表 5 条、树 5 条、队列 4 条、
散列墓碑 2 条，全被抓）。**但这依赖「有人记得补跑」。**
真跑不起来时 `--allow-degraded` 会在输出里大声喊两次，别忽略它。

### 4. 明确没有证明的六件事

1. **没走到的路径**——sanitizer 是运行期工具，只看得见执行过的代码。
2. **栈深度**——没有任何测试会去逼近那个边界（逼近就意味着让闸门崩）。
3. **别的编译器与平台**——全部结论来自 Linux + gcc 13.3。
4. **并发**——所有容器都不是线程安全的，也没有任何测试涉及并发。
5. **性能**——只有两处规模守门（链表 `append` 的 O(1)、KMP 的线性性），
   靠的是"退化实现会撞上 120 秒超时"，不是基准测试。
6. **教学正确性**——闸门管代码，不管讲法。书稿的文字讲没讲清楚，只有人能判断。

### 如果你接手，先做这三件

1. **在你自己的机器上重跑第 1 条的测量**，把表里的数字换成你的——
   数字跟着栈大小、编译器、优化档走，照抄别人的没有意义。
   复现程序在 `collab/UNVERIFIED-RISKS.md`。
2. **决定 `HuffmanTree` 那条未覆盖路径**：补探针覆盖它，或明确记为不覆盖并写明理由。
3. **要引入新的递归结构就先回到第 1 条**，决定完把结论写进 `collab/DECISION_LOG.md`，
   别让它变成又一处口头约定。

---

## 一条方法上的教训

底稿只读（`dsa_raw.md` 全程一字未动）不是洁癖，是为了任何时候都能分清
**「原书的错」与「OCR 的错」**。

这条差点没守住：第 1 章一度被判定为「原书样例自相矛盾」，依据是印出的矩阵里
B1 与 B5 的最大最短路都是 8。但那些 `8` 是 OCR 把 `∞` 认错的产物——
同一张表里第 2、4 行还残留着真正的 `∞`，而原书正文明写那几行是 ∞。
更关键的是，选起点用的是图1.3（Floyd 的**输出**矩阵），它在底稿里是**一张图片**、
内容从未被 OCR 过。按正文还原重算，**原书是对的**。

差一点就把 OCR 的错当成原书的错印进教材。

## 协作方式

两个 AI（Claude Code 与 Codex）交替实现与审查，靠 `collab/` 下的共享事实源交接，
不靠"记忆"。协议、红线与标准循环见 [`collab/README.md`](collab/README.md)；
已生效的决策见 [`collab/DECISION_LOG.md`](collab/DECISION_LOG.md)。

## 版权说明

`dsa_raw.md` 是原书的 OCR 文本，版权属于原作者与高等教育出版社，本仓库仅用于
教学内容的现代化整理与研究。`code/`、`tools/`、`tests/` 下的代码为本项目新写。
