# dsa-modernization

把《数据结构与算法》（张铭、王腾蛟、赵海燕编著，高等教育出版社 2008，
普通高等教育"十一五"国家级规划教材）从一份 OCR 底稿，整理成一部**代码能编译、
能跑、经得起现代工程规范检验**的教材。

原书的 C++ 是典型的 2008 年高校教材风格：裸 `new[]`、缺析构、违反三法则、
用出参 + `bool` 双通道返回、容器里直接 `cout` 打中文提示、`int` 当下标。
其中不少**根本编译不过**——第一个样板单元就抓到两处，
详见 [`code/ch03/array_stack/legacy.md`](code/ch03/array_stack/legacy.md)。

现代化要保住的是原书的教学内容、编号与讲法；换掉的是那套写法；
而「换对了」这件事**由机器复验，不靠自觉**。

## 快速开始

```bash
python3 tools/handoff.py --verify     # 完整闸门：工具自测 → 台账 → 书稿 → 真编译真运行
python3 tools/ledger.py               # 105 条清单现在做到哪了
python3 tools/ledger.py --pending     # 还有哪些没人认领
python3 tools/check_code.py           # 只跑 code/：-Werror + ASan/UBSan + O2 双构建
python3 tools/check_doc.py            # 只跑 book/：OCR 残留、编号、插图、代码块一致性
```

需要 Python 3（仅标准库）与 g++/clang++（支持 C++17 与 sanitizer）。无第三方依赖。

## 仓库结构

| 路径 | 是什么 |
| --- | --- |
| `dsa_raw.md` | OCR 底稿，**只读**。1MB / 11978 行 / 12 章 / 105 条清单 / 292 张外链插图 |
| `book/` | 现代化后的书稿。插图落在 `book/assets/` |
| `code/<章>/<单元>/` | 一个清单单元：`unit.json`（认领哪几条清单）、`legacy.md`（原书写法→缺陷证据→现代写法）、`modern.hpp`、`test.cpp` |
| `tools/` | 闸门与脚手架 |
| `tests/` | 闸门自身的单元测试（48 项） |
| `collab/` | Claude ⇄ Codex 协作事实源：PLAN / HANDOFF / NOTES / 台账退场记录 |

## 三条闸门

1. **台账**（`ledger.py`）——原书 105 条清单，每条要么被某个 `code/` 单元认领，
   要么在 `collab/exclusions.json` 里带理由退场。**没有第三种状态**，
   「已覆盖 + 退场 + 待办 = 105」这条等式由脚本守着。
2. **书稿体检**（`check_doc.py`）——7 条规则拦 OCR 残留、假语言标签、断掉的交叉引用、
   热链插图。其中最硬的 R3：书稿里的每段 C++ 必须用 ` ```cpp file=... ` 引用 `code/`
   下的真实文件并**逐字一致**。书上印的代码就是跑过的那份代码。
3. **代码**（`check_code.py`）——每个单元在 `-Wall -Wextra -Wpedantic -Werror` 下
   编译两遍：Debug + ASan/UBSan，以及 Release -O2。两遍都要真跑起来、断言全过。
   （实测：同一段越界 UB 在 `-O2` 那档是静默通过的——这就是为什么要跑两种构建。）

## 协作方式

两个 AI（Claude Code 与 Codex）交替实现与审查，靠 `collab/` 下的共享事实源交接，
不靠"记忆"。协议、红线与标准循环见 [`collab/README.md`](collab/README.md)。

## 版权说明

`dsa_raw.md` 是原书的 OCR 文本，版权属于原作者与高等教育出版社，本仓库仅用于
教学内容的现代化整理与研究。`code/`、`tools/`、`tests/` 下的代码为本项目新写。
