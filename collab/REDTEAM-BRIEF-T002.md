# 红队任务书 · T-002 · 交给 Codex

> **你的角色**：红队。不是来点头的，是来找漏的。
> 本轮的成功标准不是「审完说没问题」，而是**至少交出一条会失败的测试**。
> 如果三条主攻方向都攻不动，请明确写出你试过哪些攻法、为什么没打穿——
> 「攻不动」也是结论，但必须带着攻击记录，不能只有一句「看起来没问题」。

## 先读什么

| 顺序 | 文件 | 为什么 |
| --- | --- | --- |
| 1 | `collab/DECISION_LOG.md` | D-001 是人拍板的风格公约，判「对不对」的依据在这里，不在你的习惯里 |
| 2 | `collab/README.md` | 协议、八条红线、交接格式 |
| 3 | `collab/review-input.md` | 本轮完整 diff + 闸门输出（脚本生成，不入库） |
| 4 | `code/ch03/array_stack/legacy.md` | 原书缺陷与实测证据，可逐条重跑 |

分支 `feat/ch03-stack-modernization`。闸门：`python3 tools/handoff.py --verify`（当前退出码 0）。

## 主攻方向一：D-001 静态检查的正则漏洞（`tools/check_code.py`）

`check_d001()` 逐行扫 `modern.*`，禁 `<iostream>` / `std::cout` / STL 容器头文件。
它是**逐行正则 + 一句 `line.split("//")[0]` 去注释**，我知道它薄，但没有加固——
留给你打。已知的可疑面：

- 块注释 `/* ... */` 跨行，里面的关键字会不会被误判？（假阳性）
- 反过来：**真正的违规能不能藏进它的盲区**？例如
  `/* 注释 */ std::cout << x;`、字符串字面量里的 `#include <vector>`、
  行末续行 `\` 把 `std::cout` 拆到两行、`std ::cout`（空格）、
  `using namespace std; cout << x;`（没有 `std::` 前缀）、
  宏拼接、`#include <vector>` 写成 `#  include   <vector>`。
- `d001_exceptions` 的键是「被豁免的写法」，值是理由。**值只要非空就放行**——
  写一个空格算不算理由？这是不是等于没有闸门？

**交付**：`tests/test_check_code.py` 里新增会失败的用例，然后（可选）加固实现。
判据放宽或收紧都可以，但**任何放宽都要在 NOTES 里写清代价**。

## 主攻方向二：`bad_alloc` 与移动赋值的故障注入（`code/ch03/array_stack/`）

`ensure_capacity()` 声称强异常保证，我用 `Fragile`（第 N 次**拷贝赋值**必抛）做了
故障注入实测。**没覆盖的两条，请你造**：

1. **`new T[next]` 本身抛 `std::bad_alloc`**。分配失败时 `fresh` 未赋值，
   `try` 块还没进——我认为原栈完好，但没验过。
   建议手法：给 `T` 加一个会抛的 `operator new[]`，或用一个构造函数会抛的元素类型
   （注意 `new T[n]` 会逐个默认构造，中途抛出时已构造的元素由运行时负责析构，
   这条本身也值得验）。
2. **移动赋值抛异常**。`std::move_if_noexcept` 的选择依据是**移动构造**是否 noexcept，
   而这里元素是被**赋值**进新缓冲区的。我在 NOTES 里推理过「原栈仍然完好」，
   但**这条推理没有被任何测试守住**。请构造：移动构造 noexcept（于是 move_if_noexcept
   选择移动）、但移动赋值会抛——然后看强异常保证还成不成立。

**这两条我判断是本轮最可能真出 bug 的地方。**

## 主攻方向三：新增的 `peek()` 接口（人于 2026-08-12 拍板，D-001 §3b）

```cpp
[[nodiscard]] const T* peek() const noexcept {
    return empty() ? nullptr : &data_[top_index_ - 1];
}
```

契约：零拷贝；空栈返回 `nullptr`；**返回的指针在下一次 push/pop/clear 后失效**。

请重点看：

- 失效契约只写在注释和书稿里，**没有任何机制阻止误用**。这是可接受的（C++ 常态），
  还是应该做点什么（例如提供一个只在 `-D_GLIBCXX_DEBUG` 下生效的世代计数）？
- `noexcept` 标得对吗？`empty()` 与取址都不抛，我认为对。
- `test_peek_after_growth_is_refetched` 里我**刻意不去解引用扩容前的旧指针**
  （那是 UB，测了也不算数）。这个取舍你认同吗？还是有办法把「失效」这件事
  变成可测的？
- 与 `top()` 的分工是否真的必要，还是我把接口做大了？——**这条欢迎你推翻**，
  它是人拍板的，但如果你有技术上的反对理由，写进 NOTES，由人复裁。

## 顺带：三条我自己知道、但没动的

1. `check_doc.py` 的 R3 比对前两边都过 `textwrap.dedent`，书稿整体多缩进两格可能仍判等——有意放宽，你可能觉得放宽过头。
2. 台账只校验「认领的编号在原书里存在」，**不校验「这个单元真的实现了那条清单」**。一个 `unit.json` 可以认领算法7.6 却实现一个空类。我没想到不靠人读代码的补法。
3. `check_doc.py` R2 的注释/字符串剥离器是手写小状态机，**原始字符串 `R"(...)"`、行末续行 `\`、`'\''` 都没覆盖**。

## 交回格式

1. 意见写进 `collab/NOTES-codex.md`；能直接修的就修 + 小步 commit。
2. `collab/HANDOFF.md` 追加一条交接记录，**附 `python3 tools/handoff.py --verify` 的真实尾部计数**。
3. 生成回程包：`python3 tools/handoff.py --from codex --to claude --base main --verify`
4. 若你放宽了任何判据，或决定某条「不修」，按红线第 7 条：**把结论记下来**
   （NOTES + 必要时 `collab/exclusions.json`），不要让它悄悄消失。
