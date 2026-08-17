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
import json
import collections
import re
import sys
import textwrap
import unicodedata
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ledger  # noqa: E402  底稿的 parser of record
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

# R9：公式得真能渲染出来。
#
# 缘由（2026-08-17 Codex 复查）：`book/ch12-advanced.md` 里那个多维数组偏移公式
# 从写下来那天起就是**坏的**——它跨了三行，而 `build_site.py` 的行间公式只认
# 「同一行 $$ 开头、同一行 $$ 结尾」，于是整段以原始 LaTeX 文本印在页面上，
# 一直没人发现。这正是「公式错了没有任何东西会红」的那一类问题。
#
# 两条判据都只覆盖**能机器判定**的部分：公式是否渲染得出来、命令是否认识。
# **数学本身对不对，机器判不了**，仍然要人复核（见 UNVERIFIED-RISKS）。
DISPLAY_MATH_RE = re.compile(r"^\s*\$\$")

RULES = [
    "R1  代码块语言标签只能用白名单里的；原书的 hcl/csv/javascript 等假标签一律红",
    "R2  代码块里不得残留 OCR 坏味道（拆开的运算符、全角标点、被认成 1 的右花括号）",
    "R3  cpp 代码块必须用 file= 引用 code/ 下的真实文件，且逐字一致",
    "R4  图片必须有 alt 文本，且指向仓库里真实存在的本地文件（不许热链上游）",
    "R5  【算法X.Y】必须配对【算法X.Y结束】",
    "R6  正文引用的 算法X.Y/代码X.Y 必须在 dsa_raw.md 的清单目录里存在",
    "R7  正文引用的「第N章」不得超过原书的 12 章",
    "R8  text 块不得逐字复制 code/ 下的源码——本书自己的代码必须走 cpp file= 由 R3 把关",
    "R9  行间公式 $$…$$ 必须写在一行内，且不得出现渲染器不认识的 LaTeX 命令",
    "R10 原书有的节，新书要么有同号的节，要么在 collab/section_gaps.json 里登记（并入/不写/待补）",
    "R11 沿用原书编号的节必须沿用原书题名；现代教学步骤不得占用旧编号",
    "R12 正文里「第 X.Y 节」这类引用必须指向真实存在的节；带「原书」标记的按底稿解析",
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


def unknown_math_commands(text):
    """这份书稿里用到、而渲染器不认识的 LaTeX 命令。

    直接调 `build_site.render_math`——判据就是渲染器自己，不另立一套会漂的清单。
    渲染器不可用时（例如只想跑 check_doc）静默跳过，不把体检卡住。
    """
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import build_site
    except Exception:
        return []
    unknown = set()
    body = re.sub(r"^```.*?^```", "", text, flags=re.S | re.M)   # 代码块里的 $ 不是公式
    for m in re.finditer(r"\$\$(.+?)\$\$|\$([^$\n]+)\$", body):
        try:
            build_site.render_math(m.group(1) or m.group(2), unknown)
        except Exception:
            continue
    return sorted(unknown)


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


# `#fn:名字` —— 按**函数名**取切片，源码里不需要留任何标记。
#
# 为什么要有它：`// >>> anchor` 得写进源码。书稿印切片时无所谓（读者看不到整个文件），
# 但 D-012 之后 `teaching.hpp` 是**整块印出来**的——往里塞十几对锚点注释，
# 等于把 D-012 刚清掉的噪声又请回来。课件（`book/slides/`）每页只放一个函数，
# 需求量更大。于是改成按名字找：源码干干净净，引用方写 `#fn:push` 即可。
#
# 代价写明：函数改名 → 引用失效并报错（这是好事，比悄悄印错强）；
# 同名重载全部一并取出（讲课时本来就该一起看），中间空一行隔开。
FUNCTION_REF = re.compile(r"^fn:(.+)$")


# 定义与调用长得很像。判据落在**参数表右括号后面是什么**上：
#   `void push(const T& v) {`      → `)` 之后是 `{`      定义
#   `ArrayStack(size_type n = 8)`  → 下一行是 `: data_(` 定义（构造函数初始化列表）
#   `bool full() const { ... }`    → 跳过 const 之后是 `{` 定义
#   `void clear();`                → `)` 之后是 `;`      声明，不是定义
#   `if (full()) {`                → `)` 之后是 `)`      调用，不是定义
# 最后一条是真踩过的：找 `full` 时把 `enqueue` 里那句 `if (full())` 连同它的
# if 块一起当成了函数体切出来。
QUALIFIERS = {"const", "noexcept", "override", "final", "mutable", "constexpr"}


def _match_paren(stripped, line_index, column):
    """从 `(` 开始找配对的 `)`，返回它的 (行, 列)；找不到返回 None。"""
    depth = 0
    for row in range(line_index, len(stripped)):
        text = stripped[row]
        begin = column if row == line_index else 0
        for col in range(begin, len(text)):
            if text[col] == "(":
                depth += 1
            elif text[col] == ")":
                depth -= 1
                if depth == 0:
                    return row, col
    return None


def _is_definition(stripped, row, col):
    """参数表收尾之后第一个有意义的字符是 `{` 或 `:` 才算定义。"""
    word = ""
    for scan in range(row, min(row + 4, len(stripped))):
        text = stripped[scan]
        begin = col + 1 if scan == row else 0
        for ch in text[begin:]:
            if ch.isspace():
                if word and word not in QUALIFIERS:
                    return False
                word = ""
                continue
            if ch.isalnum() or ch == "_":
                word += ch
                continue
            if word and word not in QUALIFIERS:
                return False
            word = ""
            return ch in "{:"
        if word and word not in QUALIFIERS:
            return False
        word = ""
    return False


def read_function(path: Path, name):
    """取出名为 name 的函数定义（含紧邻其上的注释）。多个重载依次取出。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    stripped, in_block = [], False
    for line in lines:
        code, in_block = strip_comments_and_strings(line, in_block)
        stripped.append(code)

    # `\b` 在这里不够用：析构函数 `~ArrayStack(` 的 `~` 前面是空格，两个都是
    # 非单词字符，`\b` 不成立。而且若只排除单词字符，找 `ArrayStack` 会把
    # `~ArrayStack` 一起命中。所以前瞻要按名字分两种。
    boundary = "(?<![A-Za-z0-9_])" if name.startswith("~") else "(?<![A-Za-z0-9_~])"
    pattern = re.compile(boundary + re.escape(name) + r"\s*\(")

    chunks, index = [], 0
    while index < len(stripped):
        found = pattern.search(stripped[index])
        if not found:
            index += 1
            continue
        opening = stripped[index].find("(", found.start())
        closed = _match_paren(stripped, index, opening)
        if not closed or not _is_definition(stripped, closed[0], closed[1]):
            index += 1
            continue

        # 从函数体的 `{` 数到配对的 `}`
        depth, seen, row = 0, False, closed[0]
        while row < len(stripped):
            for ch in stripped[row]:
                if ch == "{":
                    depth += 1
                    seen = True
                elif ch == "}":
                    depth -= 1
            if seen and depth == 0:
                break
            row += 1
        if not seen or row >= len(stripped):
            index += 1
            continue

        # 把紧贴在上面的注释一起带上——教学代码的注释就是内容的一半
        top = index
        while top > 0 and lines[top - 1].strip().startswith("//"):
            top -= 1
        chunks.append("\n".join(lines[top:row + 1]))
        index = row + 1
    if not chunks:
        return None, f"{path} 里没有名为 `{name}` 的函数定义"
    return "\n\n".join(chunks), None


def read_slice(path: Path, anchor):
    """按引用取切片：`#fn:名字` 走函数名，其余走 `// >>> 锚点`。"""
    match = FUNCTION_REF.match(anchor)
    if match:
        return read_function(path, match.group(1).strip())
    return read_anchor(path, anchor)


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


# R10：原书分了哪些节，是 dsa_raw.md 说了算。
#
# 缘由（2026-08-17）：第 8 章的 8.3.1 直接选择排序、8.4.1 冒泡排序、8.6.1 桶式排序、
# 8.6.3 索引排序**整节没写**——而 `code/ch08/sorting` 里 `selection_sort`、
# `bubble_sort`、`counting_sort` 都实现了、有测试、还认领着算法8.3/8.5/8.10。
# 台账说「已覆盖」，书上却没讲；R5–R7 只管交叉引用能不能解析，管不到「这一节在不在」。
#
# R10 问「同号的节在不在」；T-028 新增的 R11 再问「同号是否同题」。
# R11 只做规范化后的题名相等，不做模糊语义匹配；内容对不对仍是人工复核项。
# 合并进父节、有意不写、欠着没写，都合法，但都得在 section_gaps.json 里
# 带理由、责任人、日期登记，理由要具体到「并进哪一节」。
CHAPTER_FILE_RE = re.compile(r"^ch(\d+)-")
GAP_KINDS = ("merged", "declined", "pending")


def load_section_gaps(path=None):
    """读 collab/section_gaps.json。返回 (dict[节号] -> entry, problems)。"""
    path = path or (ROOT / "collab" / "section_gaps.json")
    entries, problems = {}, []
    if not path.is_file():
        return entries, problems
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return entries, [f"collab/section_gaps.json 不是合法 JSON: {exc}"]
    for entry in raw.get("gaps", []):
        number = entry.get("section")
        if not number:
            problems.append("section_gaps.json: 有一条记录没写 section")
            continue
        if entry.get("kind") not in GAP_KINDS:
            problems.append(
                f"section_gaps.json[{number}]: kind 必须是 {GAP_KINDS} 之一，"
                f"现在是 {entry.get('kind')!r}"
            )
        for field in ("reason", "by", "date"):
            if not entry.get(field):
                problems.append(f"section_gaps.json[{number}]: 缺少 {field}——登记必须留下出处")
        if entry.get("kind") == "merged" and not entry.get("into"):
            problems.append(f"section_gaps.json[{number}]: merged 必须写 into，说明并进了哪一节")
        if number in entries:
            problems.append(f"section_gaps.json[{number}]: 重复记录")
        entries[number] = entry
    return entries, problems


def check_sections(path: Path, gaps, original=None):
    """R10/R11：原书的节号必须存在或登记；存在时题名不得漂移。"""
    # 只管书稿正文。课件（book/slides/）按讲课节奏组织，一页一个话题，
    # 本来就不该逐节对应原书目录——拿 R10 去要求它，只会逼人往课件里塞凑数的标题。
    found = CHAPTER_FILE_RE.match(path.name)
    if not found or path.parent.resolve() != BOOK.resolve():
        return []
    chapter = int(found.group(1))
    original = ledger.parse_sections() if original is None else original
    wanted = {num: meta for num, meta in original.items() if meta["chapter"] == chapter}
    if not wanted:
        return []
    have = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        hit = ledger.SECTION_RE.match(line)
        if hit:
            have[hit.group(1)] = hit.group(2).strip()
    problems = []
    for number in sorted(wanted, key=lambda n: [int(x) for x in n.rstrip("abcdefghij").split(".")]):
        if number in have:
            original_title = normalize_section_title(wanted[number]["title"])
            current_title = normalize_section_title(have[number])
            if current_title != original_title:
                problems.append(
                    f"{rel_label(path)}  R11 {number} 同号不同题：原书是"
                    f"「{wanted[number]['title']}」（底稿第 {wanted[number]['line']} 行），"
                    f"新书是「{have[number]}」"
                )
            continue
        if number in gaps:
            continue
        problems.append(
            f"{rel_label(path)}  R10 原书有 {number} {wanted[number]['title']}"
            f"（底稿第 {wanted[number]['line']} 行），新书没有这一节，"
            f"也没在 collab/section_gaps.json 里登记"
        )
    return problems


def normalize_section_title(title: str) -> str:
    """只忽略排版差异，不猜两个不同词组是否语义相近。"""
    title = unicodedata.normalize("NFKC", title)
    title = title.translate(str.maketrans({"“": '"', "”": '"', "「": '"', "」": '"'}))
    return re.sub(r"\s+", "", title)


# R12：写着「见第 X.Y 节」，那一节就得真的存在。
#
# 缘由（2026-08-17）：T-028 把被占用的编号还给原书之后，正文里 9 处
# 「后面 2.2.1–2.2.4 各节」「判据见第 2.3.2a 节」「见 4.2.5」当场悬空——
# 它们指向的小节要么改成了不带编号的 `####`，要么换了号。
# **改编号是对的，漏改引用是自动的**：没有任何一条规则在看这些引用。
# R6 管的是【算法X.Y】清单号，R7 管的是「第 N 章」，中间这一层一直空着。
#
# 判据分两个池子：默认按**新书**的节号解析；引用前面十几个字里出现
# 「原书 / 底稿 / 课程」时按**底稿**解析（正文里大量「原书 4.2.2 节自己写下」这种句子）。
# `勘误.md` 整篇都在说原书，按底稿解析。
SECTION_NUMBER = r"\d+\.\d+(?:\.\d+)?[a-z]?"
SECTION_REF_RES = [
    re.compile(r"第\s*(" + SECTION_NUMBER + r")\s*节"),
    re.compile(r"(?:见|参见|详见|同)\s*(" + SECTION_NUMBER + r")(?:\s*节)?"),
    re.compile(r"(" + SECTION_NUMBER + r")\s*节"),
    re.compile(r"(" + SECTION_NUMBER + r")[–—-](" + SECTION_NUMBER + r")\s*各?节"),
]
ORIGINAL_MARKERS = ("原书", "底稿", "课程")
ORIGINAL_SCOPE_FILES = ("勘误.md",)


def book_section_numbers(book_root=None):
    """新书（正文 + 课件）里实际存在的节号。"""
    root = book_root or BOOK
    numbers = set()
    if not root.is_dir():
        return numbers
    for path in sorted(root.rglob("*.md")):
        if "pdf" in path.relative_to(root).parts:
            continue
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            hit = ledger.SECTION_RE.match(line)
            if hit:
                numbers.add(hit.group(1))
    return numbers


def check_section_refs(path: Path, book_numbers, original_numbers):
    """R12：正文里的节引用必须解析得到。"""
    problems = []
    original_scope = path.name in ORIGINAL_SCOPE_FILES
    for lineno, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if line.startswith("#"):
            continue
        seen = set()
        for pattern in SECTION_REF_RES:
            for found in pattern.finditer(line):
                context = line[max(0, found.start() - 16):found.start()]
                to_original = original_scope or any(w in context for w in ORIGINAL_MARKERS)
                pool = original_numbers if to_original else book_numbers
                where = "底稿" if to_original else "新书"
                for number in found.groups():
                    if not number or number in seen:
                        continue
                    seen.add(number)
                    if number in pool:
                        continue
                    problems.append(
                        f"{rel_label(path)}:{lineno}  R12 引用了 {number} 节，"
                        f"但{where}里没有这一节"
                    )
    return problems


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
                expected, err = read_slice(target, anchor)
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

    # R9 公式：跨行的 $$ 渲染不出来；不认识的命令会以原始 LaTeX 印在页面上
    in_fence = False
    for idx, line in enumerate(text.splitlines(), 1):
        if line.strip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        stripped = line.strip()
        if DISPLAY_MATH_RE.match(line) and not (stripped.endswith("$$") and len(stripped) > 4):
            add(idx, "R9 行间公式跨行了——渲染器只认「同一行 $$ 开头、同一行 $$ 结尾」，"
                     "跨行会把原始 LaTeX 印在页面上")
    for name in unknown_math_commands(text):
        add(1, f"R9 渲染器不认识的 LaTeX 命令 `{name}`——它会以原始文本印在页面上；"
               f"请在 tools/build_site.py 的 MATH_SYMBOLS 里补上，或换一种写法")

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
    gaps, problems = load_section_gaps()
    original = ledger.parse_sections()
    book_numbers = book_section_numbers()
    original_numbers = set(original)
    for target in targets:
        problems += check_file(target, listings)
        problems += check_sections(target, gaps, original)
        problems += check_section_refs(target, book_numbers, original_numbers)

    if problems:
        print("\n".join(f"❌ {p}" for p in problems))
        print(f"\n{len(problems)} 个问题。规则说明见 `python3 tools/check_doc.py --list-rules`")
        sys.exit(1)
    kinds = collections.Counter(entry.get("kind") for entry in gaps.values())
    print(f"✅ 书稿体检通过：{len(targets)} 个文件，{len(RULES)} 条规则")
    if gaps:
        print(f"   节覆盖：原书 {len(original)} 节，已登记 {len(gaps)} 节"
              f"（并入父节 {kinds['merged']}，有意不写 {kinds['declined']}，待补 {kinds['pending']}）")


if __name__ == "__main__":
    main()
