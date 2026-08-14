#!/usr/bin/env python3
"""check_doc.py — 现代化后的书稿（book/）体检。

原书是 OCR 出来的，`dsa_raw.md` 里那些坏味道（拆开的 `i + +`、被认成 `1` 的 `}`、
全角分号、乱贴的 ```hcl 标签）会顺着复制粘贴流进新书稿。这个脚本就是拦它们的。

最硬的一条是**代码块引用契约**：书稿里的 C++ 代码块必须写成

    ```cpp file=code/ch03/array_stack/modern.hpp
    ```cpp file=code/ch03/array_stack/modern.hpp#push        （锚点切片）

脚本会把块内容和文件（或 `// >>> push` … `// <<< push` 之间的切片）逐字比对。
比对不上就红。**书上印的代码 = 能编译能跑的那份代码**，不给「文中示意」留后门。

用法:
  python3 tools/check_doc.py            # 检查 book/ 下全部 markdown
  python3 tools/check_doc.py book/ch03-stack.md
  python3 tools/check_doc.py --list-rules
"""
import argparse
import re
import sys
import textwrap
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

BOOK = ROOT / "book"

FENCE_RE = re.compile(r"^(?P<indent>[ \t]*)```(?P<info>[^\n]*)$")
ALLOWED_LANGS = {"cpp", "c", "text", "console", "bash", "diff", "json", "mermaid", "python", ""}
# 原书 OCR 留下的假语言标签，出现即错
BOGUS_LANGS = {"hcl", "csv", "javascript", "typescript", "matlab", "lisp"}

# 代码块内的 OCR 残留。(正则, 说明)
CODE_SMELLS = [
    (re.compile(r"[+]\s+[+]"), "`+ +` —— OCR 把 `++` 拆开了"),
    (re.compile(r"-\s+-(?![-\s])"), "`- -` —— OCR 把 `--` 拆开了"),
    (re.compile(r"=\s+="), "`= =` —— OCR 把 `==` 拆开了"),
    (re.compile(r"<\s+<"), "`< <` —— OCR 把 `<<` 拆开了"),
    (re.compile(r">\s+>"), "`> >` —— OCR 把 `>>` 拆开了"),
    (re.compile(r":\s+:"), "`: :` —— OCR 把 `::` 拆开了"),
    (re.compile(r"#\s*include\s*<\s+"), "`#include < x >` —— 尖括号里多了空格"),
    (re.compile(r"[；，。（）【】“”]"), "代码里出现全角标点"),
    (re.compile(r"[−–—]"), "代码里出现 Unicode 减号/破折号，不是 ASCII `-`"),
    (re.compile(r"^\s*[1一丨]\s*$"), "孤零零的 `1`/`一` —— OCR 把 `}` 认错了"),
]

IMAGE_RE = re.compile(r"!\[(?P<alt>[^\]]*)\]\((?P<src>[^)\s]+)(?:\s+\"[^\"]*\")?\)")
# 只认「独占一行开头」的起始标记——那才是原书的清单体例。
# 正文里「原书【代码3.1】用一个空基类……」是引用，不是开清单，不该要求配对。
LISTING_OPEN_RE = re.compile(r"^\s*【\s*(算法|代码)\s*([0-9]+\.[0-9]+[a-zA-Z]?)\s*】")
LISTING_END_RE = re.compile(r"【\s*(算法|代码)\s*([0-9]+\.[0-9]+[a-zA-Z]?)\s*结束\s*】")
REF_RE = re.compile(r"(算法|代码)\s*([0-9]+\.[0-9]+[a-zA-Z]?)")
CHAPTER_REF_RE = re.compile(r"第\s*([0-9]+)\s*章")

MAX_CHAPTER = 12

RULES = [
    "R1  代码块语言标签只能用白名单里的；原书的 hcl/csv/javascript 等假标签一律红",
    "R2  代码块里不得残留 OCR 坏味道（拆开的运算符、全角标点、被认成 1 的右花括号）",
    "R3  cpp 代码块必须用 file= 引用 code/ 下的真实文件，且逐字一致",
    "R4  图片必须有 alt 文本，且指向仓库里真实存在的本地文件（不许热链上游）",
    "R5  【算法X.Y】必须配对【算法X.Y结束】",
    "R6  正文引用的 算法X.Y/代码X.Y 必须在 dsa_raw.md 的清单目录里存在",
    "R7  正文引用的「第N章」不得超过原书的 12 章",
    "R8  text 块不得逐字复制 code/ 下的源码——本书自己的代码必须走 cpp file= 由 R3 把关",
]


# R8：本书自己的代码只能经 `cpp file=` 进书稿。
#
# 缘由（2026-08-14）：一次重构把两个 `cpp file=…#anchor` 块改成了 ```text，
# 于是 R3 不再看管它们，源码改了、书上那份没跟着改，两边当场漂开。
# ```text 是留给**引用原书**用的——那些清单按印刷进不了编译器，只能原样照抄；
# 本书自己的代码没有这个借口。
#
# 判据要能区分「照抄一整个函数」和「摘一行出来讲」：正文里有大量
# 「`static constexpr int infinity = …`，其中 static 表示……」这样的教学摘录，
# 它们本来就该是片段。所以要求同时满足：规范化后 ≥ 60 字符、且含成对花括号
# （也就是至少是一个带函数体的定义）。当前书稿在这个判据下 0 误报。
MIN_COPIED_CHARS = 60


def _normalize_code(text):
    """按行去掉首尾空白、丢掉空行——缩进无关，和 R3 的 dedent 是同一个思路。"""
    return "\n".join(line.strip() for line in text.splitlines() if line.strip())


def source_texts(code_root=None):
    """code/ 下全部实现源码的规范化文本，供 R8 比对。"""
    root = code_root or (ROOT / "code")
    out = {}
    if not root.is_dir():
        return out
    for path in sorted(list(root.rglob("*.hpp")) + list(root.rglob("*.cpp"))):
        out[rel_label(path)] = _normalize_code(path.read_text(encoding="utf-8", errors="replace"))
    return out


def copied_from_source(body, sources):
    """这段 text 块是不是逐字抄自 code/ 下某个源文件？是则返回那个文件名。"""
    normalized = _normalize_code(body)
    if len(normalized) < MIN_COPIED_CHARS:
        return None
    if "{" not in normalized or "}" not in normalized:
        return None
    for name, content in sources.items():
        if normalized in content:
            return name
    return None


def iter_blocks(text):
    """切出围栏代码块。产出 (info, start_line, end_line, body_lines)。"""
    lines = text.splitlines()
    i, blocks = 0, []
    while i < len(lines):
        m = FENCE_RE.match(lines[i])
        if not m:
            i += 1
            continue
        info, indent, start = m.group("info").strip(), m.group("indent"), i + 1
        body, i = [], i + 1
        while i < len(lines) and lines[i].strip() != "```":
            body.append(lines[i])
            i += 1
        blocks.append({"info": info, "indent": indent, "start": start, "end": i + 1, "body": body})
        i += 1
    return blocks


def parse_info(info):
    """```cpp file=path#anchor  →  ('cpp', 'path', 'anchor')"""
    parts = info.split()
    lang = parts[0] if parts and not parts[0].startswith("file=") else ""
    path = anchor = None
    for part in parts:
        if part.startswith("file="):
            ref = part[len("file=") :]
            path, _, anchor = ref.partition("#")
    return lang, path, anchor or None


def read_anchor(path: Path, anchor):
    """取 `// >>> anchor` 与 `// <<< anchor` 之间的内容（不含标记行本身）。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    open_pat = re.compile(rf"//\s*>>>\s*{re.escape(anchor)}\s*$")
    close_pat = re.compile(rf"//\s*<<<\s*{re.escape(anchor)}\s*$")
    start = end = None
    for idx, line in enumerate(lines):
        if start is None and open_pat.search(line):
            start = idx + 1
        elif start is not None and close_pat.search(line):
            end = idx
            break
    if start is None:
        return None, f"{path} 里没有锚点 `// >>> {anchor}`"
    if end is None:
        return None, f"{path} 的锚点 `{anchor}` 只有开头没有 `// <<< {anchor}`"
    return "\n".join(lines[start:end]), None


def strip_comments_and_strings(line, in_block_comment=False):
    """把注释与字符串字面量抹掉再查坏味道，返回 (剩下的代码, 是否仍在块注释中)。

    现代化后的代码里中文注释是常态（`// 自赋值：先拷贝再交换`），
    中文提示串也可能合法出现。不抹掉它们，R2 的「全角标点」会把正常代码判红。
    """
    out = []
    i = 0
    while i < len(line):
        if in_block_comment:
            end = line.find("*/", i)
            if end == -1:
                return "".join(out), True
            i, in_block_comment = end + 2, False
            continue
        two = line[i : i + 2]
        if two == "//":
            break
        if two == "/*":
            in_block_comment, i = True, i + 2
            continue
        if line[i] in "\"'":
            quote, i = line[i], i + 1
            while i < len(line):
                if line[i] == "\\":
                    i += 2
                    continue
                if line[i] == quote:
                    i += 1
                    break
                i += 1
            out.append('""')
            continue
        out.append(line[i])
        i += 1
    return "".join(out), in_block_comment


def normalize(text):
    """去掉公共缩进和首尾空行，再比。允许书稿里把类内成员顶格排版。"""
    return textwrap.dedent("\n".join(text.split("\n"))).strip("\n").rstrip()


def known_listings():
    sys.path.insert(0, str(ROOT / "tools"))
    from ledger import parse_inventory  # noqa: E402  同目录工具，延迟导入避免循环

    return {item["id"] for item in parse_inventory()}


def check_file(path: Path, listings, sources=None):
    """返回 problems 列表，每条形如 'book/x.md:12  说明'。"""
    problems = []
    if sources is None:
        sources = source_texts()
    rel = rel_label(path)
    text = path.read_text(encoding="utf-8")

    def add(line, msg):
        problems.append(f"{rel}:{line}  {msg}")

    blocks = iter_blocks(text)
    code_spans = set()
    for block in blocks:
        code_spans.update(range(block["start"], block["end"] + 1))
        lang, ref, anchor = parse_info(block["info"])
        body = "\n".join(block["body"])

        # R1 语言标签
        if lang in BOGUS_LANGS:
            add(block["start"], f"R1 语言标签 `{lang}` 是 OCR 误判的产物，C++ 清单请标 cpp")
        elif lang not in ALLOWED_LANGS:
            add(block["start"], f"R1 未知语言标签 `{lang}`，白名单：{sorted(ALLOWED_LANGS - {''})}")

        # R8 本书自己的代码不许以 text 块手抄进来
        if lang == "text":
            origin = copied_from_source(body, sources)
            if origin is not None:
                add(
                    block["start"],
                    f"R8 这段 text 块逐字抄自 {origin}；本书自己的代码要写成 "
                    "```cpp file=<路径>#<锚点>，交给 R3 逐字核对。"
                    "text 块是留给引用原书用的。",
                )

        # R2 OCR 坏味道
        if lang in ("cpp", "c"):
            in_block_comment = False
            for offset, line in enumerate(block["body"]):
                code, in_block_comment = strip_comments_and_strings(line, in_block_comment)
                if not code.strip():
                    continue
                for pattern, desc in CODE_SMELLS:
                    if pattern.search(code):
                        add(block["start"] + 1 + offset, f"R2 {desc}: `{line.strip()[:60]}`")
                        break

        # R3 引用契约
        if lang == "cpp":
            if not ref:
                add(
                    block["start"],
                    "R3 cpp 代码块没有 file= 引用。书稿里的 C++ 必须来自 code/ 下能编译的文件，"
                    "示意性片段请标 ```text",
                )
                continue
            target = ROOT / ref
            if not target.is_file():
                add(block["start"], f"R3 file={ref} 不存在")
                continue
            if anchor:
                expected, err = read_anchor(target, anchor)
                if err:
                    add(block["start"], f"R3 {err}")
                    continue
            else:
                expected = target.read_text(encoding="utf-8")
            if normalize(expected) != normalize(body):
                add(
                    block["start"],
                    f"R3 代码块与 {ref}{'#' + anchor if anchor else ''} 不一致 —— "
                    "书稿抄漏了，或者代码改了没同步",
                )

    # R4 图片
    for m in IMAGE_RE.finditer(text):
        line = text[: m.start()].count("\n") + 1
        alt, src = m.group("alt").strip(), m.group("src")
        if not alt:
            add(line, "R4 图片缺 alt 文本")
        elif re.search(r"TODO|待补|占位|placeholder", alt, re.I):
            # vendor_figures.py 只搬字节，图注得有人真看图去写。占位符不算写过。
            add(line, f"R4 alt 还是占位文本 `{alt[:30]}`")
        if src.startswith(("http://", "https://")):
            add(line, f"R4 图片还热链在上游 `{src[:60]}…`，请用 tools/vendor_figures.py 落到本地")
        elif not (path.parent / src).is_file() and not (ROOT / src).is_file():
            add(line, f"R4 图片文件不存在: {src}")

    # R5 清单标记配对
    prose_lines = [
        (idx, line)
        for idx, line in enumerate(text.splitlines(), 1)
        if idx not in code_spans
    ]
    opened = {f"{k}{n}": idx for idx, line in prose_lines for k, n in LISTING_OPEN_RE.findall(line)}
    closed = {f"{k}{n}" for _, line in prose_lines for k, n in LISTING_END_RE.findall(line)}
    for listing, idx in opened.items():
        if listing not in closed:
            add(idx, f"R5 【{listing}】没有配对的【{listing}结束】")

    # R6 / R7 交叉引用
    for idx, line in prose_lines:
        for kind, number in REF_RE.findall(line):
            if f"{kind}{number}" not in listings:
                add(idx, f"R6 引用了原书没有的 {kind}{number}")
        for number in CHAPTER_REF_RE.findall(line):
            if not 1 <= int(number) <= MAX_CHAPTER:
                add(idx, f"R7 引用了第{number}章，原书只有 {MAX_CHAPTER} 章")

    return problems


def main():
    parser = argparse.ArgumentParser(description="书稿体检")
    parser.add_argument("paths", nargs="*", help="默认 book/ 下全部 .md")
    parser.add_argument("--list-rules", action="store_true")
    opts = parser.parse_args()

    if opts.list_rules:
        print("\n".join(RULES))
        return

    if opts.paths:
        targets = [Path(p) if Path(p).is_absolute() else ROOT / p for p in opts.paths]
    else:
        targets = (
            sorted(p for p in BOOK.rglob("*.md") if "pdf" not in p.relative_to(BOOK).parts)
            if BOOK.is_dir()
            else []
        )

    if not targets:
        print("⚠️  book/ 下还没有书稿，跳过（脚手架已就位，等第一章现代化）")
        return

    listings = known_listings()
    problems = []
    for target in targets:
        problems += check_file(target, listings)

    if problems:
        print("\n".join(f"❌ {p}" for p in problems))
        print(f"\n{len(problems)} 个问题。规则说明见 `python3 tools/check_doc.py --list-rules`")
        sys.exit(1)
    print(f"✅ 书稿体检通过：{len(targets)} 个文件，{len(RULES)} 条规则")


if __name__ == "__main__":
    main()
