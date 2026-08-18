#!/usr/bin/env python3
"""把 book/ 的 16 份 Markdown 渲染成可以直接用浏览器打开的静态站点 book/site/。

    python3 tools/build_site.py            # 构建
    python3 tools/build_site.py --check    # 只校验：站点与书稿是否已经脱节

**Markdown 是唯一事实源**，HTML 是产物 —— 不要手改 book/site/*.html，下次构建会覆盖。
入口是 `book/site/index.html`，双击即可用浏览器打开；也可以
`python3 -m http.server -d book` 后访问 `/site/`。

零第三方依赖（与 tools/ 下其余脚本同一条规矩），因此这里只实现书稿**实际用到**的
Markdown 子集：

    #/##/###/#### 标题、段落、`- ` 与 `1. ` 列表（含缩进续行）、竖线表格、
    ``` 代码块（含 `file=` 标注）、> 引用、--- 分隔线、`行内代码`、**粗体**、
    *斜体*、[链接](url)、![图](assets/x.jpg)、$行内公式$ 与 $$行间公式$$

子集之外的写法不会被静默吃掉：未知的 LaTeX 命令原样印出并加虚线下划线，构建结束时
统一列在 stderr 上。

三件值得说明的事：

**站内链接会被校验。** 书稿里 `[原书勘误](勘误.md)` 这类链接会改写成 `.html`，
`../code/...`、`../collab/...` 这类仓库内链接改写成 GitHub 地址。改写完之后逐条检查
目标页面和锚点真的存在，对不上就非零退出 —— 死链是这种生成式站点最容易长出来的东西。

**公式不引 KaTeX。** 全书 LaTeX 只用到 40 来个命令（`\\log`、`\\Theta`、`\\lceil`、
`\\frac` …），手写一个转换表比为它拉一个 300KB 的库划算，也不必联网。

**C++ 代码块在构建期高亮。** 浏览器端不跑任何高亮库；高亮只加标签不改字节，
`tests/test_build_site.py` 里有一条断言盯着这一点 —— 书稿代码块与 `code/` 下源码
逐字一致是 R3 的契约，渲染环节不能把字符改了。
"""
from __future__ import annotations

import argparse
import html
import json
import re
import sys
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parent.parent
BOOK = ROOT / "book"
SITE = BOOK / "site"
REPO_BLOB = "https://github.com/GMyhf/dsa-modernization/blob/main/"

# 插图在页面里的前缀。默认 `../assets/`：站点住在 book/site/，图片住在 book/assets/，
# 不复制第二份。发布到 GitHub Pages 时页面被摆到发布目录的根上，改用 `assets/`
# （见 .github/workflows/pages.yml 与 D-011）。
ASSETS_HREF = "../assets/"

# 学生用 PDF：首页给一个下载卡片。页数来自 tools/build_book_pdf.py 落下的 sidecar
# （PDF 自身的页树是压缩的，挖不出来），体积直接量文件。PDF 不在时卡片自动消失，
# 构建照常通过——网页版不该因为没排版 PDF 就构建不出来。
PDF_NAME = "数据结构与算法.pdf"
PDF_FILE = BOOK / "pdf" / PDF_NAME
PDF_INFO = BOOK / "pdf" / "build-info.json"
PDF_HREF = "../pdf/" + PDF_NAME

BOOK_TITLE = "数据结构与算法：Python 讲算法，C++ 讲实现"
DESCRIPTION = ("《数据结构与算法》（张铭、王腾蛟、赵海燕，高等教育出版社 2008）的现代化重编："
               "保留原书章节脉络与算法思想，示例统一为可编译、可测试的 C++17；"
               "讲算法的章节另给一份同样跑过测试的 Python 实现。")

# (Markdown 文件, 输出文件名, 侧栏分组)。侧栏标题取自各文件的 H1，不在这里重复维护。
PAGES = [
    ("数据结构与算法.md", "index.html", "front"),
    ("ch01-adt.md", "ch01-adt.html", "body"),
    ("ch02-linear-list.md", "ch02-linear-list.html", "body"),
    ("ch03-stack.md", "ch03-stack.html", "body"),
    ("ch04-string.md", "ch04-string.html", "body"),
    ("ch05-binary-tree.md", "ch05-binary-tree.html", "body"),
    ("ch06-tree.md", "ch06-tree.html", "body"),
    ("ch07-graph.md", "ch07-graph.html", "body"),
    ("ch08-sorting.md", "ch08-sorting.html", "body"),
    ("ch09-external-sort.md", "ch09-external-sort.html", "body"),
    ("ch10-search.md", "ch10-search.html", "body"),
    ("ch11-index.md", "ch11-index.html", "body"),
    ("ch12-advanced.md", "ch12-advanced.html", "body"),
    ("习题与参考答案.md", "exercises.html", "back"),
    ("插图.md", "figures.html", "back"),
    ("勘误.md", "errata.html", "back"),
]
MD_TO_HTML = {md: out for md, out, _ in PAGES}
GROUP_LABEL = {"front": "导读", "body": "正文", "back": "附录"}
# 首页 H1 就是书名，侧栏里叫「封面与导读」更像一本书的目录
SIDEBAR_OVERRIDE = {"index.html": "封面与导读"}


# --------------------------------------------------------------------------- 锚点

def slugify(text):
    """GitHub 风格锚点。书稿正文里的目录链接用的就是这套规则。"""
    text = text.replace("`", "")
    text = re.sub(r"[^\w一-鿿\s-]", "", text).strip().lower()
    return re.sub(r"\s+", "-", text)


class Anchors:
    """同一页内重名标题按 GitHub 的办法加 -1、-2 后缀。"""

    def __init__(self):
        self.seen = {}

    def take(self, text):
        base = slugify(text)
        count = self.seen.get(base, 0)
        self.seen[base] = count + 1
        return base if count == 0 else f"{base}-{count}"


# --------------------------------------------------------------------------- 公式

MATH_SYMBOLS = {
    "times": "×", "cdot": "⋅", "cdots": "⋯", "ldots": "…", "dots": "…",
    "le": "≤", "leq": "≤", "ge": "≥", "geq": "≥", "ne": "≠", "neq": "≠",
    "approx": "≈", "sim": "∼", "equiv": "≡", "pm": "±", "mp": "∓",
    "in": "∈", "notin": "∉", "subset": "⊂", "subseteq": "⊆", "cup": "∪",
    "cap": "∩", "emptyset": "∅", "infty": "∞", "to": "→", "rightarrow": "→",
    "ll": "≪", "gg": "≫", "propto": "∝", "setminus": "∖", "supseteq": "⊇",
    "leftarrow": "←", "Rightarrow": "⇒", "leftrightarrow": "↔", "mapsto": "↦",
    "sum": "∑", "prod": "∏", "int": "∫", "star": "⋆", "circ": "∘",
    "lceil": "⌈", "rceil": "⌉", "lfloor": "⌊", "rfloor": "⌋",
    "langle": "⟨", "rangle": "⟩", "vert": "|", "|": "‖",
    "alpha": "α", "beta": "β", "gamma": "γ", "delta": "δ", "epsilon": "ε",
    "lambda": "λ", "mu": "μ", "pi": "π", "sigma": "σ", "tau": "τ",
    "phi": "φ", "omega": "ω", "theta": "θ", "imath": "ı", "jmath": "ȷ",
    "Delta": "Δ", "Gamma": "Γ", "Theta": "Θ", "Lambda": "Λ", "Sigma": "Σ",
    "Omega": "Ω", "Phi": "Φ", "Pi": "Π",
    "{": "{", "}": "}", "%": "%", "&": "&", "#": "#", "_": "_",
    # 定界符尺寸命令：只影响括号大小，本渲染器不分尺寸，直接丢掉，括号本身照常输出。
    "left": "", "right": "", "big": "", "bigl": "", "bigr": "",
    "Big": "", "Bigl": "", "Bigr": "", "bigg": "", "biggl": "", "biggr": "",
    "Bigg": "", "Biggl": "", "Biggr": "",
    "quad": " ", "qquad": "  ", ",": " ", ";": " ",
    ":": " ", "!": "", " ": " ", "\\": "<br>",
}
# 直立排版的算子名：变量是斜体，log/max 这类不是
MATH_OPERATORS = {"log", "ln", "lg", "exp", "max", "min", "deg", "bmod", "mod",
                  "gcd", "lim", "sup", "inf", "det", "dim", "sin", "cos", "tan"}
# 只影响字体的包装命令，内容照常渲染
MATH_WRAPPERS = {"mathrm": "upr", "mathbf": "bold", "mathsf": "upr", "mathfrak": "frak",
                 "mathcal": "cal", "texttt": "mono", "text": "upr", "pmb": "bold",
                 "textstyle": "", "displaystyle": "", "mathit": ""}


MATH_RELATIONS = set("=<>≤≥≠≈∼≡∈∉⊂⊆→←⇒↔↦")
MATH_BINARIES = set("+−×⋅±∓∪∩")


def spaced(symbol):
    """关系符两侧留宽一点、二元运算符窄一点 —— TeX 的间距分级，只保留最粗的两档。"""
    if symbol in MATH_RELATIONS:
        return f'<span class="rel">{symbol}</span>'
    if symbol in MATH_BINARIES:
        return f'<span class="bin">{symbol}</span>'
    return symbol


def _take_group(tex, index):
    """读 tex[index:] 开头的一个 {...} 或单个字符，返回 (内容, 新下标)。

    OCR 出来的公式里 `a _ { 1 6 }` 这种带空格的写法很常见，所以先跳空白 ——
    不跳的话下标会退化成一个空格，`a_{16}` 就印成「a 1 6」。
    """
    while index < len(tex) and tex[index] == " ":
        index += 1
    if index >= len(tex):
        return "", index
    if tex[index] != "{":
        if tex[index] == "\\":
            match = re.match(r"\\([A-Za-z]+|.)", tex[index:])
            return match.group(0), index + len(match.group(0))
        return tex[index], index + 1
    depth, start = 0, index
    while index < len(tex):
        if tex[index] == "{":
            depth += 1
        elif tex[index] == "}":
            depth -= 1
            if depth == 0:
                return tex[start + 1:index], index + 1
        index += 1
    return tex[start + 1:], index  # 括号没闭合，按到末尾算


def render_math(tex, unknown=None):
    """把一小段 LaTeX 转成 HTML。不认识的命令原样留下并记账，不静默丢弃。

    间距按 TeX 的规矩来：源码里的空格不算数（`a _ { 1 6 }` 与 `a_{16}` 同义），
    真正的间距来自关系符与二元运算符两侧的留白。
    """
    out = []
    i = 0
    while i < len(tex):
        char = tex[i]
        if char == " ":
            i += 1                      # 数学模式下源码空格无意义
            continue
        if char == "\\":
            match = re.match(r"\\([A-Za-z]+|.)", tex[i:])
            name = match.group(1)
            i += len(match.group(0))
            if name == "frac":
                num, i = _take_group(tex, i)
                den, i = _take_group(tex, i)
                out.append(f'<span class="frac"><span class="num">{render_math(num, unknown)}</span>'
                           f'<span class="den">{render_math(den, unknown)}</span></span>')
            elif name == "sqrt":
                inner, i = _take_group(tex, i)
                out.append(f'<span class="sqrt">{render_math(inner, unknown)}</span>')
            elif name in MATH_WRAPPERS:
                inner, i = _take_group(tex, i)
                css = MATH_WRAPPERS[name]
                body = render_math(inner, unknown)
                out.append(f'<span class="{css}">{body}</span>' if css else body)
            elif name in MATH_OPERATORS:
                # log/max 这类算子名直立排版，两侧各补一个细空格（紧跟上下标时右侧不补）
                follows = tex[i:].lstrip(" ")[:1]
                after = "" if follows in ("_", "^") else "&thinsp;"
                before = "" if not out or out[-1].endswith(("(", "⌈", "⌊", "⟨")) else "&thinsp;"
                out.append(f'{before}<span class="upr">{name}</span>{after}')
            elif name in MATH_SYMBOLS:
                out.append(spaced(MATH_SYMBOLS[name]))
            else:
                if unknown is not None:
                    unknown.add("\\" + name)
                out.append(f'<span class="tex-raw">{html.escape(chr(92) + name)}</span>')
        elif char in "^_":
            inner, i = _take_group(tex, i + 1)
            tag = "sup" if char == "^" else "sub"
            out.append(f"<{tag}>{render_math(inner, unknown)}</{tag}>")
        elif char.isdigit():
            match = re.match(r"[0-9.]+", tex[i:])
            out.append(f'<span class="upr">{match.group(0)}</span>')
            i += len(match.group(0))
        elif char in "{}":
            i += 1  # 分组括号本身不出现在输出里
        else:
            out.append(spaced(html.escape(char) if char != "-" else "−"))
            i += 1
    return "".join(out)


# --------------------------------------------------------------------------- C++ 高亮

CPP_KEYWORDS = {
    "alignas", "alignof", "auto", "break", "case", "catch", "class", "const",
    "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
    "for", "friend", "goto", "if", "inline", "mutable", "namespace", "new",
    "noexcept", "nullptr", "operator", "private", "protected", "public",
    "register", "reinterpret_cast", "return", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this", "throw", "true", "try",
    "typedef", "typeid", "typename", "union", "using", "virtual", "volatile", "while",
}
CPP_TYPES = {
    "bool", "char", "double", "float", "int", "long", "short", "signed",
    "unsigned", "void", "size_t", "ptrdiff_t", "wchar_t", "char16_t", "char32_t",
}
CPP_TOKEN = re.compile(r"""
    (?P<pre>^[ \t]*\#[^\n]*)
  | (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')
  | (?P<number>\b(?:0[xX][0-9a-fA-F']+|[0-9][0-9']*(?:\.[0-9']*)?(?:[eE][+-]?[0-9]+)?)[uUlLfF]*\b)
  | (?P<word>[A-Za-z_][A-Za-z0-9_]*)
""", re.X | re.S | re.M)


def highlight_cpp(code):
    """构建期高亮：只加标签，一个字符都不改（tests 里有断言盯着）。"""
    out, last = [], 0
    for match in CPP_TOKEN.finditer(code):
        out.append(html.escape(code[last:match.start()]))
        kind = match.lastgroup
        text = html.escape(match.group(0))
        if kind == "word":
            word = match.group(0)
            if word in CPP_KEYWORDS:
                out.append(f'<span class="k">{text}</span>')
            elif word in CPP_TYPES:
                out.append(f'<span class="t">{text}</span>')
            else:
                out.append(text)
        else:
            out.append(f'<span class="{kind[0]}">{text}</span>')
        last = match.end()
    out.append(html.escape(code[last:]))
    return "".join(out)


# --------------------------------------------------------------------------- Python 高亮

# D-025 起书里有两种语言。Python 块若只做 html.escape，读者会看到一段没有层次的
# 灰字——那等于在版面上把它降格成附录。分词规则与 C++ 那份共用同一批 span 类名，
# 于是两种语言的关键字、字符串、注释在页面上是同一种颜色语义。
PY_KEYWORDS = {
    "and", "as", "assert", "async", "await", "break", "class", "continue", "def",
    "del", "elif", "else", "except", "finally", "for", "from", "global", "if",
    "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
    "return", "try", "while", "with", "yield", "None", "True", "False",
}
PY_BUILTINS = {
    "abs", "all", "any", "bool", "bytes", "dict", "enumerate", "float", "int",
    "isinstance", "len", "list", "max", "min", "object", "print", "range",
    "reversed", "set", "str", "sum", "tuple", "type", "zip",
}
# 三引号串必须排在单引号串前面，否则 `"""` 会被当成「空串 + 一个引号」拆开。
PY_TOKEN = re.compile(r"""
    (?P<comment>\#[^\n]*)
  | (?P<string>[rbfuRBFU]{0,2}(?:\"\"\".*?\"\"\"|'''.*?'''|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*'))
  | (?P<number>\b(?:0[xXbBoO][0-9a-fA-F_]+|[0-9][0-9_]*(?:\.[0-9_]*)?(?:[eE][+-]?[0-9]+)?)\b)
  | (?P<word>[A-Za-z_][A-Za-z0-9_]*)
""", re.X | re.S | re.M)


def highlight_python(code):
    """构建期高亮：只加标签，一个字符都不改（tests 里有断言盯着）。"""
    out, last = [], 0
    for match in PY_TOKEN.finditer(code):
        out.append(html.escape(code[last:match.start()]))
        kind = match.lastgroup
        text = html.escape(match.group(0))
        if kind == "word":
            word = match.group(0)
            if word in PY_KEYWORDS:
                out.append(f'<span class="k">{text}</span>')
            elif word in PY_BUILTINS:
                out.append(f'<span class="t">{text}</span>')
            else:
                out.append(text)
        else:
            out.append(f'<span class="{kind[0]}">{text}</span>')
        last = match.end()
    out.append(html.escape(code[last:]))
    return "".join(out)


# --------------------------------------------------------------------------- 链接改写

class Context:
    """一次构建共享的状态：链接账本、公式告警、图片检查。"""

    def __init__(self):
        self.links = []       # (来源页, 原始 href, 改写后 href)
        self.unknown_tex = set()
        self.missing_assets = []
        self.anchors_by_page = {}
        self.page = ""


def rewrite_href(href, ctx):
    """书稿里的相对链接 → 站点内的 .html / 仓库里的 GitHub 地址。"""
    if re.match(r"^(https?:|mailto:|#)", href):
        return href
    target, _, fragment = href.partition("#")
    fragment = f"#{fragment}" if fragment else ""
    if target in MD_TO_HTML:                       # 同目录的另一章
        return MD_TO_HTML[target] + fragment
    if target.startswith("../"):                   # code/、collab/、仓库根
        return REPO_BLOB + target[3:] + fragment
    if target.startswith("assets/"):               # 图片：默认站点在 book/assets/ 的隔壁
        return ASSETS_HREF + target[len("assets/"):] + fragment
    if target.endswith(".md"):                     # book/ 下没进站点的 Markdown
        return REPO_BLOB + "book/" + target + fragment
    return href


def image_tag(alt, src, ctx):
    if src.startswith("assets/") and not (BOOK / src).is_file():
        ctx.missing_assets.append((ctx.page, src))
    return (f'<img loading="lazy" src="{html.escape(rewrite_href(src, ctx), quote=True)}" '
            f'alt="{html.escape(alt, quote=True)}">')


# --------------------------------------------------------------------------- 行内

def decorate(text, stash):
    """转义 → 粗体/斜体 → 把占位符换回真正的标签。"""
    text = html.escape(text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*\s][^*]*)\*(?!\*)", r"<em>\1</em>", text)
    for index, markup in enumerate(stash):
        text = text.replace(f"\x00{index}\x00", markup)
    return text


def render_inline(text, ctx):
    stash = []

    def keep(markup):
        stash.append(markup)
        return f"\x00{len(stash) - 1}\x00"

    # 行内代码要在转义之前摘出来，否则代码里的 < > 会被处理两次
    text = re.sub(r"`([^`]+)`", lambda m: keep(f"<code>{html.escape(m.group(1))}</code>"), text)
    text = re.sub(r"\$([^$\n]+)\$",
                  lambda m: keep(f'<span class="math">{render_math(m.group(1), ctx.unknown_tex)}</span>'),
                  text)
    text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)",
                  lambda m: keep(image_tag(m.group(1), m.group(2), ctx)), text)

    def take_link(match):
        label, href = match.group(1), match.group(2)
        new = rewrite_href(href, ctx)
        ctx.links.append((ctx.page, href, new))
        external = ' class="ext"' if new.startswith("http") else ""
        return keep(f'<a href="{html.escape(new, quote=True)}"{external}>'
                    f'{decorate(label, [])}</a>')

    text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", take_link, text)
    return decorate(text, stash)


# --------------------------------------------------------------------------- 块

FENCE = re.compile(r"^```\s*([A-Za-z]*)\s*(.*)$")
IMAGE_ONLY = re.compile(r"^!\[([^\]]*)\]\(([^)\s]+)\)$")
BLOCK_START = re.compile(r"^(#{1,6}\s|```|>|\s*[-*]\s|\s*\d+\.\s|\||\s*---+\s*$|\$\$)")


def render_blocks(lines, ctx, anchors):
    """返回 (正文 HTML, [(层级, 标题文本, 锚点)])。"""
    out, headings = [], []
    index, count = 0, len(lines)

    while index < count:
        line = lines[index]

        # 代码块
        fence = FENCE.match(line) if line.startswith("```") else None
        if fence:
            language, info = fence.group(1), fence.group(2).strip()
            body = []
            index += 1
            while index < count and not lines[index].startswith("```"):
                body.append(lines[index])
                index += 1
            index += 1  # 吃掉收尾围栏
            code = "\n".join(body)
            if language == "cpp":
                rendered = highlight_cpp(code)
            elif language == "python":
                rendered = highlight_python(code)
            else:
                rendered = html.escape(code)
            source = ""
            path = re.match(r"file=(\S+)", info)
            if path:
                target, _, anchor = path.group(1).partition("#")
                label = path.group(1)
                source = (f'<div class="srcbar"><a href="{REPO_BLOB}{html.escape(target, quote=True)}">'
                          f'{html.escape(label)}</a></div>')
            out.append(f'<div class="codeblock" data-lang="{html.escape(language or "text")}">'
                       f'{source}<pre><code>{rendered}</code></pre></div>')
            continue

        # 标题
        heading = re.match(r"^(#{1,6})\s+(.*)$", line)
        if heading:
            level, text = len(heading.group(1)), heading.group(2).strip()
            anchor = anchors.take(text)
            headings.append((level, text, anchor))
            out.append(f'<h{level} id="{anchor}"><a class="anchor" href="#{anchor}">#</a>'
                       f'{render_inline(text, ctx)}</h{level}>')
            index += 1
            continue

        # 行间公式
        if line.strip().startswith("$$") and line.strip().endswith("$$") and len(line.strip()) > 4:
            body = line.strip()[2:-2]
            out.append(f'<div class="math math-display">{render_math(body, ctx.unknown_tex)}</div>')
            index += 1
            continue

        if re.match(r"^\s*---+\s*$", line):
            out.append("<hr>")
            index += 1
            continue

        # 表格：一行竖线 + 一行分隔
        if line.strip().startswith("|") and index + 1 < count \
                and re.match(r"^\s*\|[\s:|-]+\|\s*$", lines[index + 1]):
            def cells(row):
                return [c.strip() for c in row.strip().strip("|").split("|")]

            aligns = []
            for spec in cells(lines[index + 1]):
                left, right = spec.startswith(":"), spec.endswith(":")
                aligns.append("center" if left and right else "right" if right else "left")

            def align_of(position):
                return aligns[position] if position < len(aligns) else "left"

            out.append('<div class="table-wrap"><table><thead><tr>')
            out += [f'<th style="text-align:{align_of(n)}">{render_inline(c, ctx)}</th>'
                    for n, c in enumerate(cells(line))]
            out.append("</tr></thead><tbody>")
            index += 2
            while index < count and lines[index].strip().startswith("|"):
                out.append("<tr>")
                out += [f'<td style="text-align:{align_of(n)}">{render_inline(c, ctx)}</td>'
                        for n, c in enumerate(cells(lines[index]))]
                out.append("</tr>")
                index += 1
            out.append("</tbody></table></div>")
            continue

        # 引用：空的 > 行分段
        if line.startswith(">"):
            chunks, current = [], []
            while index < count and lines[index].startswith(">"):
                stripped = lines[index].lstrip(">").strip()
                if stripped:
                    current.append(stripped)
                elif current:
                    chunks.append(current)
                    current = []
                index += 1
            if current:
                chunks.append(current)
            body = "".join(f"<p>{render_inline(' '.join(c), ctx)}</p>" for c in chunks)
            out.append(f"<blockquote>{body}</blockquote>")
            continue

        # 列表（书稿里没有嵌套列表，但有缩进续行）
        item = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if item:
            ordered = not item.group(2) in ("-", "*")
            tag = "ol" if ordered else "ul"
            out.append(f"<{tag}>")
            while index < count:
                item = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[index])
                if not item or (not item.group(2) in ("-", "*")) != ordered:
                    break
                parts = [item.group(3)]
                index += 1
                while index < count and lines[index].strip() and not BLOCK_START.match(lines[index]):
                    parts.append(lines[index].strip())
                    index += 1
                out.append(f"<li>{render_inline(' '.join(parts), ctx)}</li>")
                if index < count and not lines[index].strip():
                    index += 1  # 列表项之间的空行不打断列表
                    if index < count and not re.match(r"^(\s*)([-*]|\d+\.)\s+", lines[index]):
                        break
            out.append(f"</{tag}>")
            continue

        if not line.strip():
            index += 1
            continue

        # 独立成段的图片 → figure；紧随其后、与 alt 相同的那行是题注（插图册的体例）
        picture = IMAGE_ONLY.match(line.strip())
        if picture:
            alt, src = picture.group(1), picture.group(2)
            index += 1
            caption = alt
            probe = index
            while probe < count and not lines[probe].strip():
                probe += 1
            if probe < count and lines[probe].strip() == alt.strip():
                index = probe + 1
            out.append(f'<figure>{image_tag(alt, src, ctx)}'
                       f'<figcaption>{render_inline(caption, ctx)}</figcaption></figure>')
            continue

        # 段落
        paragraph = [line]
        index += 1
        while index < count and lines[index].strip() and not BLOCK_START.match(lines[index]):
            paragraph.append(lines[index])
            index += 1
        out.append("<p>" + render_inline(" ".join(p.strip() for p in paragraph), ctx) + "</p>")

    return "\n".join(out), headings


# --------------------------------------------------------------------------- 页面

STYLE = """
:root{color-scheme:light;
  --ink:#1a2230;--muted:#68758a;--faint:#93a0b3;--line:#dde3ec;--bg:#f3f5f9;--panel:#fff;
  --soft:#eef1f7;--accent:#2f5fd0;--accent-soft:#e5ecfb;--warn:#b06a1c;--danger:#c0392b;
  --kw:#8b2fa0;--ty:#0f6f8c;--st:#177245;--cm:#7d8899;--pp:#a3572a}
@media (prefers-color-scheme:dark){:root{color-scheme:dark;
  --ink:#e3e8f0;--muted:#98a3b5;--faint:#6f7c90;--line:#2b3342;--bg:#12151b;--panel:#171b22;
  --soft:#1e232c;--accent:#84a9f5;--accent-soft:#1c2740;--warn:#dcae72;--danger:#e08a80;
  --kw:#d79ae8;--ty:#7cc7dd;--st:#8fd0a4;--cm:#7b8598;--pp:#e0a97b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);
  font:16px/1.8 system-ui,-apple-system,"Segoe UI","Noto Sans CJK SC","PingFang SC",sans-serif}
a{color:var(--accent)}
.masthead{background:var(--panel);border-bottom:1px solid var(--line);padding:30px 26px 24px}
.masthead .inner{max-width:1440px;margin:0 auto}
.masthead .eyebrow{font-size:13px;color:var(--muted);letter-spacing:.06em;margin:0 0 6px}
.masthead .eyebrow a{text-decoration:none}
.masthead h1{margin:0;font-size:clamp(23px,3.4vw,34px);line-height:1.25;letter-spacing:-.01em}
.masthead p.sub{margin:10px 0 0;color:var(--muted);font-size:15px;max-width:62ch}
.masthead .meta{margin:12px 0 0;font-size:13.5px;color:var(--muted)}
.layout{max-width:1440px;margin:0 auto;padding:0 26px 80px;display:grid;
  grid-template-columns:228px minmax(0,1fr) 208px;gap:40px;align-items:start}
nav.book,nav.page{position:sticky;top:16px;max-height:calc(100vh - 32px);overflow:auto;
  padding:22px 0;font-size:14px;line-height:1.55}
nav b{display:block;color:var(--faint);font-size:11.5px;letter-spacing:.1em;
  text-transform:uppercase;margin:16px 0 8px}
nav b:first-child{margin-top:0}
nav a{display:block;padding:5px 10px;border-left:2px solid transparent;
  color:var(--ink);text-decoration:none;border-radius:0 5px 5px 0}
nav a:hover{background:var(--soft);color:var(--accent)}
nav.book a.here{background:var(--accent-soft);border-left-color:var(--accent);
  color:var(--accent);font-weight:600}
nav.page a{color:var(--muted);font-size:13.5px;padding:4px 10px}
nav.page a.sub{padding-left:22px;font-size:13px;color:var(--faint)}
nav.page a.active{border-left-color:var(--accent);color:var(--accent);background:var(--accent-soft)}
main{min-width:0;padding-top:24px}
h1,h2,h3,h4{scroll-margin-top:16px}
h2{font-size:24px;margin:48px 0 14px;padding-bottom:8px;border-bottom:2px solid var(--line)}
h3{font-size:18.5px;margin:30px 0 10px}
h4{font-size:16px;margin:22px 0 8px;color:var(--muted)}
.anchor{float:left;margin-left:-1.05em;padding-right:.3em;color:transparent;
  text-decoration:none;font-weight:400}
h2:hover .anchor,h3:hover .anchor,h4:hover .anchor{color:var(--faint)}
p{margin:0 0 15px}
code{font:13.5px/1.5 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  background:var(--soft);padding:2px 6px;border-radius:4px;word-break:break-word}
.codeblock{position:relative;background:var(--panel);border:1px solid var(--line);
  border-radius:9px;margin:0 0 18px;overflow:hidden}
.codeblock .srcbar{border-bottom:1px solid var(--line);background:var(--soft);
  padding:6px 12px;font:12.5px ui-monospace,SFMono-Regular,Menlo,monospace}
.codeblock .srcbar a{color:var(--muted);text-decoration:none}
.codeblock .srcbar a:hover{color:var(--accent);text-decoration:underline}
.codeblock pre{margin:0;padding:14px 16px;overflow:auto}
.codeblock code{background:none;padding:0;font-size:13px;line-height:1.65;white-space:pre}
.codeblock[data-lang=console] pre,.codeblock[data-lang=bash] pre{background:var(--soft)}
.codeblock[data-lang=text]::after{content:"原书印刷";position:absolute;top:8px;right:10px;
  font-size:11px;color:var(--faint);letter-spacing:.05em}
.copy{position:absolute;top:6px;right:8px;border:1px solid var(--line);background:var(--panel);
  color:var(--muted);border-radius:6px;font-size:12px;padding:2px 8px;cursor:pointer;opacity:0}
.codeblock:hover .copy{opacity:1}
.copy:hover{color:var(--accent);border-color:var(--accent)}
.codeblock .srcbar~.copy{top:34px}
.k{color:var(--kw)}.t{color:var(--ty)}.s{color:var(--st)}.c{color:var(--cm);font-style:italic}
.n{color:var(--warn)}.p{color:var(--pp)}
blockquote{margin:0 0 18px;padding:12px 18px;border-left:3px solid var(--accent);
  background:var(--soft);border-radius:0 7px 7px 0}
blockquote p:last-child{margin-bottom:0}
.table-wrap{overflow-x:auto;margin:0 0 18px}
table{border-collapse:collapse;width:100%;font-size:14.5px;background:var(--panel)}
th,td{border:1px solid var(--line);padding:8px 11px;vertical-align:top}
th{background:var(--soft);font-weight:650}
ul,ol{margin:0 0 15px;padding-left:26px}
li{margin:6px 0}
hr{border:0;border-top:1px solid var(--line);margin:34px 0}
figure{margin:0 0 22px;padding:12px;background:var(--panel);border:1px solid var(--line);
  border-radius:9px}
figure img{max-width:100%;display:block;margin:0 auto;border-radius:4px;background:#fff}
figcaption{margin-top:9px;color:var(--muted);font-size:13.5px;text-align:center}
.gallery-page{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));
  gap:18px;align-items:start}
.gallery-page>figure{margin:0}
.gallery-page>h2,.gallery-page>h3,.gallery-page>p,.gallery-page>blockquote,
.gallery-page>hr,.gallery-page>.pagenav{grid-column:1/-1}
.math{font-family:"Latin Modern Math","Cambria Math",Georgia,serif;font-style:italic}
.math .upr,.math .mono,.math .bold{font-style:normal}
.math .mono{font-family:ui-monospace,Menlo,monospace}
.math .bold{font-weight:700}
.math sup,.math sub{font-size:.72em}
.math .rel{margin:0 .28em;font-style:normal}
.math .bin{margin:0 .16em;font-style:normal}
.math-display{display:block;text-align:center;margin:18px 0;font-size:17px}
.frac{display:inline-flex;flex-direction:column;vertical-align:middle;text-align:center;
  font-size:.85em;line-height:1.15}
.frac .num{border-bottom:1px solid currentColor;padding:0 .25em}
.frac .den{padding:0 .25em}
.sqrt{border-top:1px solid currentColor}
.sqrt::before{content:"√";border-top:0}
.tex-raw{border-bottom:1px dotted var(--danger);font-style:normal}
.download{display:flex;align-items:center;gap:14px;margin:4px 0 26px;padding:14px 16px;
  background:var(--panel);border:1px solid var(--line);border-left:3px solid var(--accent);
  border-radius:9px;text-decoration:none;color:var(--ink)}
.download:hover{background:var(--accent-soft);border-color:var(--accent)}
.dl-badge{flex:none;background:var(--accent-soft);color:var(--accent);border-radius:6px;
  padding:6px 9px;font:700 12.5px/1 ui-monospace,SFMono-Regular,Menlo,monospace;
  letter-spacing:.06em}
.dl-body{display:block;min-width:0}
.dl-body b{display:block;font-size:15.5px}
.dl-meta{display:block;color:var(--muted);font-size:13px;line-height:1.5;margin-top:2px}
.dl-arrow{margin-left:auto;color:var(--accent);font-size:19px}
.pagenav{display:flex;justify-content:space-between;gap:16px;margin:50px 0 0;
  border-top:1px solid var(--line);padding-top:18px;font-size:14.5px}
.pagenav a{text-decoration:none;max-width:46%}
.pagenav .dir{display:block;color:var(--faint);font-size:12px}
footer{max-width:1440px;margin:0 auto;padding:22px 26px 60px;color:var(--muted);
  font-size:13.5px;border-top:1px solid var(--line)}
@media(max-width:1180px){
  .layout{grid-template-columns:212px minmax(0,1fr);gap:32px}
  nav.page{display:none}
}
@media(max-width:820px){
  .layout{grid-template-columns:1fr;gap:0;padding:0 18px 60px}
  nav.book{position:static;max-height:none;border-bottom:1px solid var(--line);
    margin-bottom:14px;padding:14px 0}
  .masthead{padding:22px 18px 18px}
}
@media print{
  nav,.pagenav,.copy{display:none}
  .layout{display:block;max-width:none}
  body{background:#fff}
  a{color:inherit;text-decoration:none}
  .codeblock,figure,table{break-inside:avoid}
}
"""

SCRIPT = """
(function(){
  var links=[].slice.call(document.querySelectorAll('nav.page a'));
  if(links.length){
    var map={};links.forEach(function(a){map[a.getAttribute('href').slice(1)]=a});
    var seen=[];
    var io=new IntersectionObserver(function(entries){
      entries.forEach(function(e){
        var id=e.target.id;
        if(e.isIntersecting){if(seen.indexOf(id)<0)seen.push(id)}
        else{var i=seen.indexOf(id);if(i>=0)seen.splice(i,1)}
      });
      links.forEach(function(a){a.classList.remove('active')});
      if(seen.length&&map[seen[0]])map[seen[0]].classList.add('active');
    },{rootMargin:'0px 0px -75% 0px'});
    [].slice.call(document.querySelectorAll('h2[id],h3[id]')).forEach(function(h){io.observe(h)});
  }
  [].slice.call(document.querySelectorAll('.codeblock')).forEach(function(block){
    var button=document.createElement('button');
    button.className='copy';button.type='button';button.textContent='复制';
    button.addEventListener('click',function(){
      var text=block.querySelector('code').textContent;
      var done=function(){button.textContent='已复制';
        setTimeout(function(){button.textContent='复制'},1200)};
      if(navigator.clipboard&&navigator.clipboard.writeText){
        navigator.clipboard.writeText(text).then(done,function(){button.textContent='复制失败'});
      }else{
        var area=document.createElement('textarea');area.value=text;document.body.appendChild(area);
        area.select();try{document.execCommand('copy');done()}catch(e){button.textContent='复制失败'}
        document.body.removeChild(area);
      }
    });
    block.appendChild(button);
  });
})();
"""


def download_card():
    """首页的 PDF 下载卡片。PDF 不存在就返回空串——不印一个点了 404 的链接。"""
    if not PDF_FILE.is_file():
        return ""
    megabytes = PDF_FILE.stat().st_size / 1024 / 1024
    facts = [f"{megabytes:.1f} MB"]
    main_chapters = 12
    if PDF_INFO.is_file():
        try:
            info = json.loads(PDF_INFO.read_text(encoding="utf-8"))
            if info.get("pages"):
                facts.insert(0, f"{info['pages']} 页")
            if info.get("figures"):
                facts.append(f"{info['figures']} 张插图")
            if info.get("main_chapters"):
                main_chapters = info["main_chapters"]
        except (ValueError, OSError):
            pass                                  # sidecar 坏了就少显示两个数字，不挡构建
    href = quote(PDF_HREF, safe="/:.-_~")
    return (f'<a class="download" href="{html.escape(href, quote=True)}" download>'
            f'<span class="dl-badge">PDF</span>'
            f'<span class="dl-body"><b>下载完整教程</b>'
            f'<span class="dl-meta">B5 开本，带书签目录 · {" · ".join(facts)} · '
            f'含 {main_chapters} 章正文、习题与参考答案、原书插图与勘误</span></span>'
            f'<span class="dl-arrow">↓</span></a>')


def sidebar(pages, current):
    """全站共享的左侧目录：分组 + 当前页高亮。"""
    out, group = [], None
    for page in pages:
        if page["group"] != group:
            group = page["group"]
            out.append(f'<b>{GROUP_LABEL[group]}</b>')
        css = ' class="here"' if page["out"] == current["out"] else ""
        out.append(f'<a href="{page["out"]}"{css}>{html.escape(page["label"])}</a>')
    return "".join(out)


def page_toc(headings):
    """本页小节目录：只收 h2/h3。"""
    out = []
    for level, text, anchor in headings:
        if level not in (2, 3):
            continue
        css = ' class="sub"' if level == 3 else ""
        out.append(f'<a href="#{anchor}"{css}>{html.escape(text.replace("`", ""))}</a>')
    return "".join(out)


def page_nav(pages, position):
    parts = []
    previous = pages[position - 1] if position > 0 else None
    following = pages[position + 1] if position + 1 < len(pages) else None
    parts.append(f'<a href="{previous["out"]}"><span class="dir">← 上一篇</span>'
                 f'{html.escape(previous["label"])}</a>' if previous else "<span></span>")
    parts.append(f'<a href="{following["out"]}" style="text-align:right">'
                 f'<span class="dir">下一篇 →</span>{html.escape(following["label"])}</a>'
                 if following else "<span></span>")
    return f'<div class="pagenav">{"".join(parts)}</div>'


def build_page(page, pages, position, body, headings, subtitle):
    title = page["label"] if page["out"] != "index.html" else BOOK_TITLE
    eyebrow = ("" if page["out"] == "index.html" else
               f'<p class="eyebrow"><a href="index.html">{html.escape(BOOK_TITLE)}</a></p>')
    subtitle_html = f'<p class="sub">{html.escape(subtitle)}</p>' if subtitle else ""
    head_title = html.escape(title if page["out"] == "index.html"
                             else f"{title} · {BOOK_TITLE}")
    toc = page_toc(headings)
    toc_html = f'<nav class="page"><b>本页</b>{toc}</nav>' if toc else "<div></div>"
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{head_title}</title>
<meta name="description" content="{html.escape(DESCRIPTION)}">
<meta property="og:title" content="{html.escape(title)}">
<meta property="og:description" content="{html.escape(DESCRIPTION)}">
<meta property="og:type" content="article">
<style>{STYLE}</style>
</head>
<body>
<header class="masthead"><div class="inner">
{eyebrow}  <h1>{html.escape(title)}</h1>
{subtitle_html}
  <p class="meta"><a href="{REPO_BLOB}book/{page["md"]}">Markdown 源文件</a>
    · <a href="https://github.com/GMyhf/dsa-modernization">GitHub 仓库</a>
    · <a href="errata.html">原书勘误</a></p>
</div></header>
<div class="layout">
<nav class="book">{sidebar(pages, page)}</nav>
<main{' class="gallery-page"' if page["out"] == "figures.html" else ""}>
{download_card() if page["out"] == "index.html" else ""}
{body}
{page_nav(pages, position)}
</main>
{toc_html}
</div>
<footer>
  本页由 <code>tools/build_site.py</code> 从 <code>book/{html.escape(page["md"])}</code> 生成
  —— Markdown 是唯一事实源，请勿手改此文件。正文中的 C++ 代码块与 <code>code/</code> 下
  通过测试的源码逐字一致（由 <code>tools/check_doc.py</code> 的 R3 保证）。
</footer>
<script>{SCRIPT}</script>
</body>
</html>
"""


# --------------------------------------------------------------------------- 构建

def first_heading(text):
    match = re.search(r"^#\s+(.*)$", text, re.M)
    return match.group(1).strip() if match else "(无标题)"


def split_front(text):
    """首页：H1 之后的第一段引用当副标题，其余进正文。"""
    lines = text.split("\n")
    subtitle = ""
    start = 0
    for number, line in enumerate(lines):
        if line.startswith("# "):
            start = number + 1
            continue
        if start and line.startswith(">"):
            if not subtitle:
                subtitle = line.lstrip(">").strip().rstrip("。")
            start = number + 1
            continue
        if start and not line.strip():
            start = number + 1
            continue
        if start:
            break
    return subtitle, "\n".join(lines[start:])


def render_site():
    """把 book/*.md 全部渲染一遍，返回 ({页面名: HTML}, ctx)。不碰磁盘上的站点。"""
    ctx = Context()
    pages = []
    for md, out, group in PAGES:
        source = BOOK / md
        if not source.is_file():
            raise FileNotFoundError(source)
        text = source.read_text(encoding="utf-8")
        label = SIDEBAR_OVERRIDE.get(out, first_heading(text))
        pages.append({"md": md, "out": out, "group": group, "label": label, "text": text})

    rendered = {}
    for position, page in enumerate(pages):
        ctx.page = page["out"]
        anchors = Anchors()
        if page["out"] == "index.html":
            subtitle, body_md = split_front(page["text"])
        else:
            subtitle = ""
            body_md = re.sub(r"^#\s+.*$", "", page["text"], count=1, flags=re.M)
        body, headings = render_blocks(body_md.split("\n"), ctx, anchors)
        ctx.anchors_by_page[page["out"]] = {anchor for _, _, anchor in headings}
        rendered[page["out"]] = build_page(page, pages, position, body, headings, subtitle)
    return rendered, ctx


def find_problems(ctx):
    """站内链接与图片的死链检查。死链是生成式站点最容易长出来的东西。"""
    problems = []
    for source_page, original, target in ctx.links:
        if target.startswith("http") or target.startswith("../"):
            continue
        file_part, _, fragment = target.partition("#")
        file_part = file_part or source_page
        if file_part not in ctx.anchors_by_page:
            problems.append(f"{source_page}: 链接 {original} 指向站点里没有的页面 {file_part}")
        elif fragment and fragment not in ctx.anchors_by_page[file_part]:
            problems.append(f"{source_page}: 链接 {original} 的锚点 #{fragment} 不存在")
    for source_page, asset in ctx.missing_assets:
        problems.append(f"{source_page}: 图片 book/{asset} 不存在")
    return problems


def build(check_only=False, out_dir=None):
    out_dir = out_dir or SITE
    try:
        rendered, ctx = render_site()
    except FileNotFoundError as missing:
        print(f"找不到 {missing}", file=sys.stderr)
        return 1

    problems = find_problems(ctx)
    if problems:
        print("站点自检未通过：", file=sys.stderr)
        for problem in problems:
            print(f"  ✗ {problem}", file=sys.stderr)
        return 1

    if check_only:
        stale = [name for name, page in rendered.items()
                 if not (SITE / name).is_file()
                 or (SITE / name).read_text(encoding="utf-8") != page]
        if stale:
            print(f"book/site/ 与书稿已脱节，需要重新构建：{', '.join(sorted(stale))}", file=sys.stderr)
            print("  修法：python3 tools/build_site.py", file=sys.stderr)
            return 1
        print(f"✅ book/site/ 与 book/*.md 一致（{len(rendered)} 个页面）")
        return 0

    out_dir.mkdir(parents=True, exist_ok=True)
    total = 0
    for name, page in rendered.items():
        (out_dir / name).write_text(page, encoding="utf-8")
        total += len(page.encode())
    known = set(rendered)
    for orphan in sorted(out_dir.glob("*.html")):
        if orphan.name not in known:
            orphan.unlink()
            print(f"删除了不再对应任何 Markdown 的旧页面 {orphan.name}")

    if ctx.unknown_tex:
        print(f"⚠️  {len(ctx.unknown_tex)} 个 LaTeX 命令没有转换规则，已原样印出并加虚线："
              f"{' '.join(sorted(ctx.unknown_tex))}", file=sys.stderr)
    where = out_dir.relative_to(ROOT) if out_dir.is_relative_to(ROOT) else out_dir
    print(f"✅ {where}/  {len(rendered)} 个页面  {total:,} 字节  入口 {where}/index.html")
    return 0


def main():
    global ASSETS_HREF, PDF_HREF
    parser = argparse.ArgumentParser(description="把 book/*.md 渲染成静态站点 book/site/")
    parser.add_argument("--check", action="store_true",
                        help="只校验 book/site/ 是否与书稿一致，不写文件")
    parser.add_argument("--out", metavar="目录", default=None,
                        help="改写到别处（默认 book/site/）。GitHub Pages 的发布目录用它")
    parser.add_argument("--assets-href", metavar="前缀", default=ASSETS_HREF,
                        help=f"插图在页面里的前缀，默认 {ASSETS_HREF}；"
                             "页面被摆到发布目录根上时用 assets/")
    parser.add_argument("--pdf-href", metavar="路径", default=PDF_HREF,
                        help=f"首页下载卡片指向的 PDF，默认 {PDF_HREF}；"
                             "发布时 PDF 摆在站点根上，用它的文件名即可")
    args = parser.parse_args()
    ASSETS_HREF = args.assets_href
    PDF_HREF = args.pdf_href
    return build(check_only=args.check, out_dir=Path(args.out).resolve() if args.out else SITE)


if __name__ == "__main__":
    sys.exit(main())
