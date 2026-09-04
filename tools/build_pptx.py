#!/usr/bin/env python3
"""build_pptx.py — 把 `book/slides/*.md` 排成可直接放映的 .pptx。

**Markdown 仍是唯一事实源**（见 `book/slides/README.md`）：.pptx 和
`book/slides/site/`（网页课件）、`book/pdf/`（学生 PDF）一样，是**产物**不是稿子。
之所以还要一份 .pptx——教室里的机器常常只有 PowerPoint / WPS，没有浏览器投屏权限，
而拿网页版当场改一个字很麻烦。产物多一份，事实源仍然只有一份。

**幻灯片上的代码由闸门逐字核对**：源文件在 `book/` 下，`check_doc.py` 的 R3 照样管着
```cpp file=... 块——所以 .pptx 里印出来的 C++ 与 `code/` 下真编译真跑过的文件是同一份字节。

排版思路（PowerPoint 没有「自动排版」，一切都是绝对坐标）：

    块 → 估高 → 竖直流式堆叠 → 超出正文区就整体缩一档字号，最多缩到 0.62

估高靠字符宽度模型（汉字 1 em、西文约 0.52 em），不是真正的字体度量——**它只需要够准到
不让内容溢出屏幕**。缩不下去的页会在构建时报出来，由人回去拆页，工具不偷偷截断。

用法:
  python3 tools/build_pptx.py            # 生成 book/slides/pptx/*.pptx
  python3 tools/build_pptx.py --check    # 只校验产物是否最新（闸门用）
  python3 tools/build_pptx.py --only ch03   # 只做一章，改排版时用
"""
import argparse
import hashlib
import html
import json
import math
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_site  # noqa: E402
import build_slides  # noqa: E402
import pptx_writer as W  # noqa: E402
from repo import ROOT, rel_label  # noqa: E402

SLIDES = ROOT / "book" / "slides"
OUT_DIR = SLIDES / "pptx"
INFO = OUT_DIR / "build-info.json"

PT = W.EMU_PER_POINT
MARGIN_X = int(0.62 * W.EMU_PER_INCH)
BODY_W = W.SLIDE_W - 2 * MARGIN_X
TITLE_Y = int(0.42 * W.EMU_PER_INCH)
BODY_TOP = int(1.42 * W.EMU_PER_INCH)
BODY_BOTTOM = W.SLIDE_H - int(0.62 * W.EMU_PER_INCH)
BODY_H = BODY_BOTTOM - BODY_TOP

# 字号（百分之一磅）。这一组是「一页放得下 8~10 行要点」的基准，放不下时整体按 SCALES 缩。
SZ_TITLE = 3200
SZ_H2 = 2300
SZ_H3 = 1950
SZ_BODY = 1800
SZ_TABLE = 1350
SZ_CODE = 1200
SZ_CAPTION = 1250
SZ_FOOT = 1000
# 一页放不下就往下缩，放得下还有富余就往上放大——投影仪后排看得清才是标准，
# 而课件里长短页差得很远：一页只有三条要点时按 18pt 排，屏幕上是一小撮字。
SCALES = [1.30, 1.22, 1.14, 1.07, 1.0, 0.94, 0.88, 0.82, 0.76, 0.70, 0.66, 0.62]
NEUTRAL = SCALES.index(1.0)

BULLETS = ["●", "○", "–"]


# --------------------------------------------------------------- 行内：TeX → 文字

SUP = {"0": "⁰", "1": "¹", "2": "²", "3": "³", "4": "⁴",
       "5": "⁵", "6": "⁶", "7": "⁷", "8": "⁸", "9": "⁹",
       "+": "⁺", "-": "⁻", "(": "⁽", ")": "⁾",
       "a": "ᵃ", "b": "ᵇ", "c": "ᶜ", "d": "ᵈ", "e": "ᵉ", "f": "ᶠ",
       "g": "ᵍ", "h": "ʰ", "i": "ⁱ", "j": "ʲ", "k": "ᵏ", "l": "ˡ",
       "m": "ᵐ", "n": "ⁿ", "o": "ᵒ", "p": "ᵖ", "r": "ʳ", "s": "ˢ",
       "t": "ᵗ", "u": "ᵘ", "v": "ᵛ", "w": "ʷ", "x": "ˣ", "y": "ʸ", "z": "ᶻ"}
SUB = {"0": "₀", "1": "₁", "2": "₂", "3": "₃", "4": "₄",
       "5": "₅", "6": "₆", "7": "₇", "8": "₈", "9": "₉",
       "+": "₊", "-": "₋", "(": "₍", ")": "₎",
       "a": "ₐ", "e": "ₑ", "i": "ᵢ", "j": "ⱼ", "k": "ₖ",
       "m": "ₘ", "n": "ₙ", "o": "ₒ", "p": "ₚ", "r": "ᵣ",
       "s": "ₛ", "t": "ₜ", "u": "ᵤ", "v": "ᵥ", "x": "ₓ"}

MACROS = {
    "infty": "∞", "times": "×", "cdot": "·", "cdots": "⋯",
    "ldots": "…", "dots": "…", "le": "≤", "leq": "≤",
    "ge": "≥", "geq": "≥", "ne": "≠", "neq": "≠",
    "approx": "≈", "equiv": "≡", "pm": "±", "to": "→",
    "rightarrow": "→", "leftarrow": "←", "Rightarrow": "⇒",
    "in": "∈", "notin": "∉", "subseteq": "⊆", "subset": "⊂",
    "cup": "∪", "cap": "∩", "emptyset": "∅", "forall": "∀",
    "exists": "∃", "lfloor": "⌊", "rfloor": "⌋",
    "lceil": "⌈", "rceil": "⌉", "langle": "⟨", "rangle": "⟩",
    "alpha": "α", "beta": "β", "gamma": "γ", "delta": "δ",
    "epsilon": "ε", "theta": "θ", "lambda": "λ", "mu": "μ",
    "pi": "π", "rho": "ρ", "sigma": "σ", "tau": "τ",
    "phi": "φ", "omega": "ω", "Theta": "Θ", "Omega": "Ω",
    "Sigma": "Σ", "Delta": "Δ", "Phi": "Φ", "sum": "∑",
    "prod": "∏", "sqrt": "√", "log": "log", "ln": "ln", "max": "max",
    "min": "min", "bmod": " mod ", "mod": " mod ", "deg": "deg", "gcd": "gcd",
    "quad": "  ", "qquad": "    ", ",": " ", ";": " ", "!": "", " ": " ",
    "left": "", "right": "", "cong": "≅", "sim": "∼", "propto": "∝",
    "land": "∧", "lor": "∨", "lnot": "¬", "oplus": "⊕",
    "leftrightarrow": "↔", "mapsto": "↦", "ast": "*", "star": "⋆",
    # 转义出来的普通字符：`\{` 在 TeX 里就是一个花括号。
    "{": "{", "}": "}", "%": "%", "&": "&", "#": "#", "$": "$", "_": "_",
}
# 函数名：两侧要有空隙（`n\log n`）。符号类宏不适用——`\Omega(f)` 中间不留空。
FUNCTIONS = frozenset({"log", "ln", "max", "min", "deg", "gcd", "bmod", "mod"})
# 只改字形、不改内容的包装宏：取出花括号里的东西原样用。
UNWRAP = ("mathrm", "mathsf", "mathbf", "mathit", "text", "textbf", "mathcal",
          "operatorname", "mbox")


def _group(tex, index):
    """读 `{...}` 或单个字符，返回 (内容, 新下标)。"""
    if index >= len(tex):
        return "", index
    if tex[index] == "{":
        depth, start = 1, index + 1
        index += 1
        while index < len(tex) and depth:
            if tex[index] == "{":
                depth += 1
            elif tex[index] == "}":
                depth -= 1
            index += 1
        return tex[start:index - 1], index
    if tex[index] == "\\":
        match = re.match(r"\\([A-Za-z]+)", tex[index:])
        if match:
            return tex[index:index + len(match.group(0))], index + len(match.group(0))
    return tex[index], index + 1


# 关系与二元运算符两侧该有空隙，这是 TeX 的排版规则；直接拼字符会挤成「a≤b」。
RELATIONS = "＝=≤≥≠≈≡∈∉⊆⊂∪∩→←⇒↔↦∼∝≅⊕±×·∧∨"
_SPACED = re.compile(f"\\s*([{RELATIONS}])\\s*")


def _wrap(text):
    """分数/根号里只有一个字符就不加括号：`√n` 比 `√(n)` 好看，也不会有歧义。"""
    return text if len(text) <= 1 else f"({text})"


def tex_to_text(tex, unknown=None):
    """把行内 LaTeX 压成一行 Unicode 文本。

    幻灯片上的公式都很短（`$O(n\\log n)$`、`$\\lceil N/B\\rceil$`），
    上标下标能映成 Unicode 就映，映不了退回 `^(...)`——**宁可难看，不可丢内容**。
    """
    out, index = [], 0
    while index < len(tex):
        char = tex[index]
        if char == "\\":
            match = re.match(r"\\([A-Za-z]+|.)", tex[index:])
            if not match:
                index += 1
                continue
            name = match.group(1)
            index += len(match.group(0))
            if name in UNWRAP:
                body, index = _group(tex, index)
                out.append(tex_to_text(body, unknown))
            elif name == "frac" or name == "dfrac" or name == "tfrac":
                num, index = _group(tex, index)
                den, index = _group(tex, index)
                out.append(f"{_wrap(tex_to_text(num, unknown))}/"
                           f"{_wrap(tex_to_text(den, unknown))}")
            elif name == "sqrt":
                body, index = _group(tex, index)
                out.append(f"√{_wrap(tex_to_text(body, unknown))}")
            elif name in MACROS:
                word = MACROS[name]
                # TeX 会吃掉控制词后面的空格：`\lceil N` 排出来是「⌈N」而不是「⌈ N」。
                if name.isalpha():
                    while index < len(tex) and tex[index] == " ":
                        index += 1
                # 而函数名两侧本来就有空隙：`n\log n` 是「n log n」，不是「nlogn」。
                # 只有函数名如此——`\Omega(f)` 是「Ω(f)」，中间不该塞空格。
                tail = out[-1][-1:] if out and out[-1] else ""
                if name in FUNCTIONS and tail.isalnum():
                    out.append(" ")
                out.append(word)
                if name in FUNCTIONS and index < len(tex) and \
                        (tex[index].isalnum() or tex[index] == "("):
                    out.append(" ")
            else:
                if unknown is not None:
                    unknown.add("\\" + name)
                out.append(name)
            continue
        if char in "^_":
            body, index = _group(tex, index + 1)
            body = tex_to_text(body, unknown)
            table = SUP if char == "^" else SUB
            if body and all(c in table for c in body):
                out.append("".join(table[c] for c in body))
            else:
                out.append(f"{char}({body})" if len(body) > 1 else f"{char}{body}")
            continue
        if char in "{}":
            index += 1
            continue
        if char == "~":
            out.append(" ")
            index += 1
            continue
        out.append(char)
        index += 1
    text = re.sub(r"  +", " ", _SPACED.sub(r" \1 ", "".join(out)))
    # `\lfloor (i-1)/2 \rfloor` 里那个空格是源文件的排版习惯，不是内容。
    return re.sub(r"\s+([⌋⌉⟩)\]])", r"\1", re.sub(r"([⌊⌈⟨(\[])\s+", r"\1", text)).strip()


# --------------------------------------------------------------- 行内：分段成 Run

INLINE = re.compile(
    r"(?P<code>`[^`]+`)"
    r"|(?P<bold>\*\*.+?\*\*)"
    r"|(?P<math>\$[^$]+\$)"
    r"|(?P<link>\[[^\]]+\]\([^)]+\))"
    r"|(?P<em>(?<![\*\w])\*[^*\s][^*]*\*)", re.S)


def inline_runs(text, ctx=None, base_bold=False):
    """一行 Markdown → [Run]。粗体、行内代码、`$…$`、链接（只留文字）。"""
    runs, pos = [], 0
    unknown = ctx.unknown_tex if ctx is not None else None
    for match in INLINE.finditer(text):
        if match.start() > pos:
            runs.append(W.Run(_plain(text[pos:match.start()]), bold=base_bold))
        kind = match.lastgroup
        body = match.group(kind)
        if kind == "code":
            runs.append(W.Run(body[1:-1], mono=True, color=W.COLORS["ty"], bold=base_bold))
        elif kind == "bold":
            # 粗体里常常还套着 `$…$` 或行内代码（「**下界是 $\\Omega(n\\log n)$**」），
            # 所以要递归再解一层，否则公式会原样把美元符印在幻灯片上。
            for inner in inline_runs(body[2:-2], ctx, base_bold=True):
                if inner.color is None:
                    inner.color = W.COLORS["accent"]
                inner.bold = True
                runs.append(inner)
        elif kind == "math":
            runs.append(W.Run(tex_to_text(body[1:-1], unknown), italic=True, bold=base_bold))
        elif kind == "link":
            label = re.match(r"\[([^\]]+)\]", body).group(1)
            runs.append(W.Run(_plain(label), bold=base_bold, color=W.COLORS["accent"]))
        else:
            runs.append(W.Run(_plain(body[1:-1]), italic=True, bold=base_bold))
        pos = match.end()
    if pos < len(text):
        runs.append(W.Run(_plain(text[pos:]), bold=base_bold))
    return [r for r in runs if r.text]


def _strip_marks(text):
    return text.replace("**", "")


def _plain(text):
    return html.unescape(text).replace(" ", " ")


def notes_text(notes, ctx):
    """讲稿转纯文本：备注窗格不认 Markdown，`**重点**` 原样印出来只是噪音。

    顺带按换行拆段——`<a:t>` 里的换行在 PowerPoint 里不换行，得拆成多个段落。
    """
    out = []
    for note in notes:
        for line in note.split("\n"):
            line = line.strip()
            if line:
                out.append("".join(run.text for run in inline_runs(line, ctx)))
    return out



# --------------------------------------------------------------- 宽度与高度估算

def em_width(text):
    """一段文字有多少个「汉字宽」。

    这是排版的全部度量基础：汉字与全角标点算 1，西文字母数字约 0.52，
    空格 0.28。**它只需要够准到不让内容溢出屏幕**，不是字体度量。
    """
    total = 0.0
    for char in text:
        code = ord(char)
        if code < 0x2E80:
            total += 0.28 if char == " " else 0.52
        elif 0x2000 <= code < 0x2100:
            total += 0.55
        else:
            total += 1.0
    return total


def wrapped_lines(runs, size, width_emu):
    """按可用宽度估算这些 Run 会折成几行。"""
    columns = width_emu / (size / 100 * PT)
    if columns <= 0:
        return 1
    total = sum(em_width(r.text) for r in runs)
    hard = 1 + sum(r.text.count("\n") for r in runs)
    return max(hard, int(math.ceil(total / columns)) or 1)


def text_height(runs, size, width_emu, spacing=1.34):
    return int(wrapped_lines(runs, size, width_emu) * size / 100 * PT * spacing)


# --------------------------------------------------------------- 图片尺寸

def image_size(blob):
    """PNG / JPEG 的像素尺寸。只读文件头，不解码。"""
    if blob[:8] == b"\x89PNG\r\n\x1a\n":
        width, height = struct.unpack(">II", blob[16:24])
        return width, height
    if blob[:2] == b"\xff\xd8":
        index = 2
        while index < len(blob) - 9:
            if blob[index] != 0xFF:
                index += 1
                continue
            marker = blob[index + 1]
            if marker in (0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
                          0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF):
                height, width = struct.unpack(">HH", blob[index + 5:index + 9])
                return width, height
            length = struct.unpack(">H", blob[index + 2:index + 4])[0]
            index += 2 + length
    raise ValueError("认不出的图片格式")


# --------------------------------------------------------------- Markdown → 块

FENCE = re.compile(r"^```([A-Za-z0-9_+-]*)\s*(.*)$")
HEADING = re.compile(r"^(#{1,6})\s+(.*)$")
ITEM = re.compile(r"^(\s*)([-*]|\d+[.)])\s+(.*)$")
IMAGE = re.compile(r"^!\[([^\]]*)\]\(([^)\s]+)[^)]*\)\s*$")
TABLE_SEP = re.compile(r"^\s*\|[\s:|-]+\|\s*$")


def parse_blocks(lines):
    """课件一页的行 → 结构化的块列表。

    刻意只认课件里真正在用的那几种语法（`build_slides.py` 渲染的也是这些），
    多认一种就多一条没人走过的代码路径。
    """
    blocks, index, count = [], 0, len(lines)
    while index < count:
        line = lines[index]

        fence = FENCE.match(line) if line.startswith("```") else None
        if fence:
            language, info = fence.group(1), fence.group(2).strip()
            body, index = [], index + 1
            while index < count and not lines[index].startswith("```"):
                body.append(lines[index])
                index += 1
            index += 1
            label = ""
            path = re.match(r"file=(\S+)", info)
            if path:
                label = path.group(1)
            blocks.append(("code", language, label, "\n".join(body)))
            continue

        heading = HEADING.match(line)
        if heading:
            blocks.append(("heading", len(heading.group(1)), heading.group(2).strip()))
            index += 1
            continue

        stripped = line.strip()
        if stripped.startswith("$$") and stripped.endswith("$$") and len(stripped) > 4:
            blocks.append(("math", stripped[2:-2]))
            index += 1
            continue

        if re.match(r"^\s*---+\s*$", line):
            blocks.append(("rule",))
            index += 1
            continue

        if stripped.startswith("|") and index + 1 < count and TABLE_SEP.match(lines[index + 1]):
            def cells(row):
                return [c.strip() for c in row.strip().strip("|").split("|")]

            aligns = []
            for spec in cells(lines[index + 1]):
                left, right = spec.startswith(":"), spec.endswith(":")
                aligns.append("ctr" if left and right else "r" if right else "l")
            rows = [cells(line)]
            index += 2
            while index < count and lines[index].strip().startswith("|"):
                rows.append(cells(lines[index]))
                index += 1
            blocks.append(("table", rows, aligns))
            continue

        if stripped.startswith(">"):
            chunk = []
            while index < count and lines[index].strip().startswith(">"):
                chunk.append(lines[index].strip().lstrip(">").strip())
                index += 1
            blocks.append(("quote", " ".join(c for c in chunk if c)))
            continue

        picture = IMAGE.match(stripped)
        if picture:
            alt, src = picture.group(1), picture.group(2)
            index += 1
            probe = index
            while probe < count and not lines[probe].strip():
                probe += 1
            if probe < count and lines[probe].strip() == alt.strip():
                index = probe + 1
            blocks.append(("image", alt, src))
            continue

        item = ITEM.match(line)
        if item:
            items = []
            while index < count:
                item = ITEM.match(lines[index])
                if not item:
                    break
                depth = len(item.group(1).expandtabs(4)) // 2
                marker = item.group(2)
                parts = [item.group(3)]
                index += 1
                while index < count and lines[index].strip() \
                        and not ITEM.match(lines[index]) \
                        and not HEADING.match(lines[index]) \
                        and not lines[index].startswith("```"):
                    parts.append(lines[index].strip())
                    index += 1
                items.append((depth, marker, " ".join(parts)))
                if index < count and not lines[index].strip():
                    index += 1
                    if index >= count or not ITEM.match(lines[index]):
                        break
            blocks.append(("list", items))
            continue

        if not stripped:
            index += 1
            continue

        parts = [line.strip()]
        index += 1
        while index < count and lines[index].strip() \
                and not HEADING.match(lines[index]) \
                and not ITEM.match(lines[index]) \
                and not lines[index].startswith("```") \
                and not lines[index].strip().startswith(("|", ">", "!")):
            parts.append(lines[index].strip())
            index += 1
        blocks.append(("para", " ".join(parts)))
    return blocks


# --------------------------------------------------------------- 代码上色

SPAN = re.compile(r'<span class="([a-z])">(.*?)</span>', re.S)
CODE_COLOR = {"k": "kw", "t": "ty", "s": "st", "c": "cm", "n": "nu", "p": "fg"}


def code_runs(code, language):
    """复用 `build_site` 的词法着色，把它的 HTML 还原成带颜色的 Run。

    不另写一套着色器：网页课件和 .pptx 的关键字判定必须一致，否则同一段代码
    在两处高亮不同，学生会以为是两份代码。
    """
    if language == "cpp":
        marked = build_site.highlight_cpp(code)
    elif language == "python":
        marked = build_site.highlight_python(code)
    else:
        marked = html.escape(code)
    runs, pos = [], 0
    for match in SPAN.finditer(marked):
        if match.start() > pos:
            runs.append((None, html.unescape(marked[pos:match.start()])))
        runs.append((CODE_COLOR.get(match.group(1)), html.unescape(match.group(2))))
        pos = match.end()
    if pos < len(marked):
        runs.append((None, html.unescape(marked[pos:])))
    return runs


def code_line_runs(runs, size):
    """把整段代码的 Run 序列按换行切成每行一组。"""
    lines, current = [], []
    for color, text in runs:
        pieces = text.split("\n")
        for offset, piece in enumerate(pieces):
            if offset:
                lines.append(current)
                current = []
            if piece:
                current.append(W.Run(piece, mono=True, size=size,
                                     color=W.COLORS[color] if color else W.COLORS["fg"]))
    lines.append(current)
    return lines


# --------------------------------------------------------------- 排版

class Layout:
    """一页的竖直流式排版：块按顺序往下堆，堆不下就整页缩一档字号。

    PowerPoint 没有「自动排版」，每个形状都要绝对坐标，所以高度必须自己算。
    估高偏保守（宁可留白，不可溢出），因为溢出在投影上是**看不见的内容**，
    而留白只是不好看。
    """

    def __init__(self, scale, images, media):
        self.scale = scale
        self.images = images          # {源文件绝对路径: 包内文件名}
        self.media = media            # {包内文件名: bytes}
        self.shapes = []
        self.slide_images = []
        self.y = BODY_TOP
        self.overflow = 0

    def size(self, base):
        return max(700, int(base * self.scale))

    def gap(self, points):
        self.y += int(points * PT * self.scale)

    def place(self, shape):
        self.shapes.append(shape)

    def room(self):
        return BODY_BOTTOM - self.y

    def note_overflow(self):
        if self.y > BODY_BOTTOM:
            self.overflow = max(self.overflow, self.y - BODY_BOTTOM)

    # ---- 各类块

    def heading(self, level, text, ctx):
        size = self.size(SZ_H2 if level <= 2 else SZ_H3)
        runs = inline_runs(text, ctx, base_bold=True)
        for run in runs:
            run.color = W.COLORS["fg"] if level <= 2 else W.COLORS["muted"]
        self.gap(7)
        height = text_height(runs, size, BODY_W)
        self.place(W.TextBox(MARGIN_X, self.y, BODY_W, height,
                             [W.Para(runs, size=size, color=W.COLORS["fg"])],
                             name="小标题"))
        self.y += height
        self.gap(3)

    def paragraph(self, text, ctx):
        size = self.size(SZ_BODY)
        runs = inline_runs(text, ctx)
        if not runs:
            return
        height = text_height(runs, size, BODY_W)
        self.place(W.TextBox(MARGIN_X, self.y, BODY_W, height,
                             [W.Para(runs, size=size)], name="正文"))
        self.y += height
        self.gap(4)

    def quote(self, text, ctx):
        size = self.size(SZ_BODY - 150)
        runs = inline_runs(text, ctx)
        for run in runs:
            if run.color is None:
                run.color = W.COLORS["muted"]
        inset = int(0.14 * W.EMU_PER_INCH)
        width = BODY_W - inset - int(0.12 * W.EMU_PER_INCH)
        height = text_height(runs, size, width) + int(0.08 * W.EMU_PER_INCH)
        self.gap(4)
        self.place(W.Rect(MARGIN_X, self.y, int(0.035 * W.EMU_PER_INCH), height,
                          W.COLORS["line"], name="引用线"))
        self.place(W.TextBox(MARGIN_X + inset, self.y, width, height,
                             [W.Para(runs, size=size, color=W.COLORS["muted"])],
                             name="引用"))
        self.y += height
        self.gap(4)

    def bullets(self, items, ctx):
        """要点列表。**有序列表的序号是内容，不是装饰**——「第 3 步」不能变成一个圆点，
        所以序号原样排成一段前缀 Run，而不是交给 PowerPoint 的自动编号（自动编号会
        从 1 重新数，跨页续讲时对不上）。"""
        size = self.size(SZ_BODY)
        paras, height = [], 0
        for depth, marker, text in items:
            runs = inline_runs(text, ctx)
            if not runs:
                continue
            indent = min(depth, 2)
            width = BODY_W - (indent + 1) * int(0.3 * W.EMU_PER_INCH)
            ordered = marker not in ("-", "*")
            if ordered:
                runs = [W.Run(f"{marker} ", bold=True, color=W.COLORS["accent"])] + runs
            paras.append(W.Para(runs, size=size,
                                bullet=None if ordered else BULLETS[indent],
                                indent=indent, space_before=int(size * 0.28)))
            height += text_height(runs, size, width) + int(size * 0.28 / 100 * PT)
        if not paras:
            return
        self.gap(3)
        self.place(W.TextBox(MARGIN_X, self.y, BODY_W, height, paras, name="要点"))
        self.y += height
        self.gap(4)

    def math(self, tex, ctx):
        size = self.size(SZ_BODY + 200)
        runs = [W.Run(tex_to_text(tex, ctx.unknown_tex), italic=True)]
        height = text_height(runs, size, BODY_W)
        self.gap(5)
        self.place(W.TextBox(MARGIN_X, self.y, BODY_W, height,
                             [W.Para(runs, size=size, align="ctr")], name="公式"))
        self.y += height
        self.gap(5)

    def rule(self):
        self.gap(5)
        self.place(W.Rect(MARGIN_X, self.y, BODY_W, 9525, W.COLORS["line"], name="分隔线"))
        self.y += 9525
        self.gap(5)

    def table(self, rows, aligns, ctx):
        size = self.size(SZ_TABLE)
        columns = max(len(row) for row in rows)
        widths = [0.0] * columns
        for row in rows:
            for index, cell in enumerate(row):
                widths[index] = max(widths[index], em_width(_strip_marks(cell)))
        # 每列至少留 3 个汉字宽，免得「是/否」列被挤成一条缝
        widths = [max(w, 3.0) for w in widths]
        total = sum(widths)
        scaled = [int(BODY_W * w / total) for w in widths]
        scaled[-1] += BODY_W - sum(scaled)

        cells, heights = [], []
        for index, row in enumerate(rows):
            line = []
            tallest = 1
            for position in range(columns):
                text = row[position] if position < len(row) else ""
                runs = inline_runs(text, ctx, base_bold=(index == 0))
                align = aligns[position] if position < len(aligns) else "l"
                line.append((runs, "ctr" if align == "ctr" else
                             "r" if align == "r" else None))
                tallest = max(tallest, wrapped_lines(runs, size,
                                                     scaled[position] - 91440))
            cells.append(line)
            heights.append(int(tallest * size / 100 * PT * 1.42) + 64008)
        self.gap(4)
        self.place(W.Table(MARGIN_X, self.y, scaled, cells, heights, size=size))
        self.y += sum(heights)
        self.gap(7)

    def code(self, language, label, body):
        size = self.size(SZ_CODE)
        lines = code_line_runs(code_runs(body, language), size)
        while lines and not lines[-1]:
            lines.pop()
        if not lines:
            return
        widest = max((sum(em_width(r.text) for r in line) for line in lines), default=1)
        pad = int(0.11 * W.EMU_PER_INCH)
        usable = (BODY_W - 2 * pad) / (size / 100 * PT)
        if widest > usable:            # 长行不折行，只把这一块的字号再压小
            size = max(700, int(size * usable / widest))
            lines = code_line_runs(code_runs(body, language), size)
            while lines and not lines[-1]:
                lines.pop()
        paras = []
        if label:
            paras.append(W.Para([W.Run(label, mono=True, size=max(700, int(size * 0.78)),
                                       color=W.COLORS["muted"])],
                                size=max(700, int(size * 0.78)), line=100000))
        for line in lines:
            paras.append(W.Para(line or [W.Run(" ", mono=True, size=size)],
                                size=size, line=104000))
        height = int(len(paras) * size / 100 * PT * 1.36) + 2 * pad
        self.gap(4)
        self.place(W.TextBox(MARGIN_X, self.y, BODY_W, height, paras, name="代码",
                             fill=W.COLORS["codebg"], line=W.COLORS["line"],
                             inset=pad, radius=True))
        self.y += height
        self.gap(4)

    def picture(self, alt, src, base, ctx):
        path = (base / src).resolve()
        if not path.is_file():
            ctx.problems.append(f"插图不存在：{src}")
            return
        blob = path.read_bytes()
        try:
            pixel_w, pixel_h = image_size(blob)
        except ValueError:
            ctx.problems.append(f"认不出的图片格式：{src}")
            return
        name = self.images.get(path)
        if name is None:
            name = f"image{len(self.images) + 1}{path.suffix.lower()}"
            self.images[path] = name
            self.media[name] = blob
        rel = f"rId{100 + len(self.slide_images)}"
        if not any(existing == name for _, existing in self.slide_images):
            self.slide_images.append((rel, name))
        else:
            rel = next(r for r, existing in self.slide_images if existing == name)

        caption_size = self.size(SZ_CAPTION)
        caption_runs = inline_runs(alt, ctx) if alt else []
        caption_h = (text_height(caption_runs, caption_size, BODY_W) + int(0.05 * W.EMU_PER_INCH)
                     if caption_runs else 0)
        self.gap(4)
        room = max(int(0.9 * W.EMU_PER_INCH), self.room() - caption_h - int(0.1 * W.EMU_PER_INCH))
        # 图也跟着整页的缩放走：页面稀疏时把图放大，比留一大片白有用。
        width = min(BODY_W, int(pixel_w / 96 * W.EMU_PER_INCH * 1.35 * self.scale))
        height = int(width * pixel_h / pixel_w)
        if height > room:
            height = room
            width = int(height * pixel_w / pixel_h)
        self.place(W.Picture(MARGIN_X + (BODY_W - width) // 2, self.y, width, height,
                             rel, name=alt or "插图"))
        self.y += height
        if caption_runs:
            self.gap(2)
            for run in caption_runs:
                if run.color is None:
                    run.color = W.COLORS["muted"]
            self.place(W.TextBox(MARGIN_X, self.y, BODY_W, caption_h,
                                 [W.Para(caption_runs, size=caption_size,
                                         color=W.COLORS["muted"], align="ctr")],
                                 name="图注"))
            self.y += caption_h
        self.gap(4)


class Ctx:
    """渲染期收集问题：缺图、认不出的 TeX、挤不下的页。构建照跑，最后一起报。"""

    def __init__(self):
        self.problems = []
        self.unknown_tex = set()
        self.crowded = []
        self.split = []


def title_band(title, ctx, cover=False):
    """标题 + 底下那条口红色的线。封面页字更大、居中。"""
    size = SZ_TITLE if not cover else int(SZ_TITLE * 1.25)
    runs = inline_runs(title, ctx, base_bold=True)
    for run in runs:
        run.color = W.COLORS["fg"]
    height = text_height(runs, size, BODY_W, spacing=1.2)
    shapes = [W.TextBox(MARGIN_X, TITLE_Y, BODY_W, height,
                        [W.Para(runs, size=size, align="ctr" if cover else None)],
                        name="标题")]
    line_y = TITLE_Y + height + int(0.07 * W.EMU_PER_INCH)
    bar_w = BODY_W if not cover else int(BODY_W * 0.34)
    bar_x = MARGIN_X if not cover else MARGIN_X + (BODY_W - bar_w) // 2
    shapes.append(W.Rect(bar_x, line_y, bar_w, int(0.035 * W.EMU_PER_INCH),
                         W.COLORS["accent"], name="标题线"))
    return shapes, line_y + int(0.035 * W.EMU_PER_INCH) + int(0.22 * W.EMU_PER_INCH)


def footer(deck_title, number, total):
    left = W.TextBox(MARGIN_X, W.SLIDE_H - int(0.48 * W.EMU_PER_INCH),
                     int(BODY_W * 0.7), int(0.3 * W.EMU_PER_INCH),
                     [W.Para([W.Run(deck_title, color=W.COLORS["muted"])], size=SZ_FOOT,
                             color=W.COLORS["muted"])], name="页脚")
    right = W.TextBox(MARGIN_X + int(BODY_W * 0.7), W.SLIDE_H - int(0.48 * W.EMU_PER_INCH),
                      int(BODY_W * 0.3), int(0.3 * W.EMU_PER_INCH),
                      [W.Para([W.Run(f"{number} / {total}", color=W.COLORS["muted"])],
                              size=SZ_FOOT, color=W.COLORS["muted"], align="r")],
                      name="页码")
    return [left, right]


def run_layout(body_blocks, scale, ctx, images, media, cover, title, subtitle):
    """按给定字号档排一遍，返回 (Layout, 标题形状, 标题下沿)。不落盘，可反复试。"""
    layout = Layout(scale, images, media)
    head, start = title_band(title, ctx, cover=cover) if title else ([], BODY_TOP)
    layout.y = start
    if cover and subtitle:
        runs = inline_runs(subtitle, ctx)
        for run in runs:
            run.color = W.COLORS["muted"]
        height = text_height(runs, layout.size(SZ_H3), BODY_W)
        layout.place(W.TextBox(MARGIN_X, layout.y, BODY_W, height,
                               [W.Para(runs, size=layout.size(SZ_H3),
                                       color=W.COLORS["muted"], align="ctr")],
                               name="副标题"))
        layout.y += height
        layout.gap(10)
    for block in body_blocks:
        kind = block[0]
        if kind == "heading":
            layout.heading(block[1], block[2], ctx)
        elif kind == "para":
            layout.paragraph(block[1], ctx)
        elif kind == "list":
            layout.bullets(block[1], ctx)
        elif kind == "table":
            layout.table(block[1], block[2], ctx)
        elif kind == "code":
            layout.code(block[1], block[2], block[3])
        elif kind == "image":
            layout.picture(block[1], block[2], SLIDES, ctx)
        elif kind == "quote":
            layout.quote(block[1], ctx)
        elif kind == "math":
            layout.math(block[1], ctx)
        elif kind == "rule":
            layout.rule()
    layout.note_overflow()
    return layout, head


# 拆页时用的字号档：比「勉强塞下」大一点，因为拆完每页都空得下。
SPLIT_SCALE = 0.94


def _code_seam(lines):
    """代码切两半时，切在离中点最近的空行或顶格行上。

    随手从中间切会让下半页以一个孤零零的 `}` 开头——读的人得回上一页才知道那是谁的。
    """
    middle = len(lines) // 2
    blanks = [i for i in range(1, len(lines)) if not lines[i].strip()]
    if blanks:
        return min(blanks, key=lambda i: abs(i - middle))
    # 退而求其次：顶格的一行，但**不能是收尾的花括号**——那一行属于上半段。
    starts = [i for i in range(1, len(lines))
              if lines[i].strip() and not lines[i][:1].isspace()
              and lines[i].strip().strip("});,") ]
    return min(starts, key=lambda i: abs(i - middle)) if starts else middle


def divide(block):
    """一整块自己就占满一屏时，把这一块再切两半；切不动就返回 None。

    列表按条目切，代码按行切，表格按行切**并且每半都带上表头**——
    没有表头的下半张表在投影上是读不懂的。
    """
    kind = block[0]
    if kind == "list" and len(block[1]) > 1:
        half = len(block[1]) // 2
        return [("list", block[1][:half]), ("list", block[1][half:])]
    if kind == "code":
        lines = block[3].split("\n")
        if len(lines) > 1:
            half = _code_seam(lines)
            return [("code", block[1], block[2], "\n".join(lines[:half])),
                    ("code", block[1], "", "\n".join(lines[half:]))]
    if kind == "table" and len(block[1]) > 2:
        header, body = block[1][0], block[1][1:]
        half = len(body) // 2
        return [("table", [header] + body[:half], block[2]),
                ("table", [header] + body[half:], block[2])]
    return None


def paginate(body_blocks, ctx, cover, title, subtitle):
    """一页装不下就拆成几页，而不是让内容跑出画面。

    **投影没有滚动条**：网页课件里挤不下还能滚一下，.pptx 里超出画面的内容
    就是学生看不见的内容。所以这里宁可多一页——原书的一页讲稿变成「…（续）」两页，
    也不肯把最后三行切掉。整页试到最小字号仍装不下，才动手拆。
    """
    def measure(blocks, scale):
        layout, _ = run_layout(blocks, scale, Ctx(), {}, {}, cover, title, subtitle)
        return layout.overflow == 0

    candidates = SCALES[NEUTRAL:] if cover else SCALES
    for scale in candidates:
        if measure(body_blocks, scale):
            return [body_blocks]
    pages, rest = [], list(body_blocks)
    while rest:
        take = len(rest)
        while take > 1 and not measure(rest[:take], SPLIT_SCALE):
            take -= 1
        if take == 1 and not measure(rest[:1], SPLIT_SCALE):
            pieces = divide(rest[0])
            if pieces:                 # 一整块就占满一屏：把这一块自己切两半
                rest = pieces + rest[1:]
                continue
        pages.append(rest[:take])
        rest = rest[take:]
    return pages


def build_slide(blocks, notes, deck_title, subtitle, number, total, ctx,
                images, media, cover=False):
    """一页课件源 → 一张或几张幻灯片（装不下就拆，见 `paginate`）。"""
    body_blocks = list(blocks)
    title = ""
    if body_blocks and body_blocks[0][0] == "heading" and body_blocks[0][1] == 1:
        title = body_blocks.pop(0)[2]

    chunks = paginate(body_blocks, ctx, cover, title, subtitle)
    if len(chunks) > 1:
        ctx.split.append((deck_title, title or "（无标题）", len(chunks)))
    slides = []
    for position, chunk in enumerate(chunks):
        page_title = title if position == 0 else f"{title}（续）"
        candidates = SCALES[NEUTRAL:] if (cover and position == 0) else SCALES
        last = None
        for scale in candidates:
            layout, head = run_layout(chunk, scale, ctx, images, media,
                                      cover and position == 0, page_title,
                                      subtitle if position == 0 else "")
            last = (layout, head)
            if not layout.overflow:
                break
        layout, head = last
        # 内容不满一屏时整体下移半个空白：全堆在顶上、底下空一大片，投影时很难看。
        # 只挪正文，标题与页脚是固定的。上限 0.85 英寸，免得短页飘到画面中间。
        slack = BODY_BOTTOM - layout.y
        if slack > 0:
            shift = (slack // 2 if cover and position == 0
                     else min(slack // 2, int(0.85 * W.EMU_PER_INCH)))
            for shape in layout.shapes:
                shape.y += shift
        slide = W.Slide(head + layout.shapes, notes if position == 0 else [])
        slide.images = layout.slide_images
        slides.append(slide)
        if layout.overflow:
            ctx.crowded.append((deck_title, page_title,
                                round(layout.overflow / W.EMU_PER_INCH, 2)))
    return slides


def build_deck(path, ctx):
    """一份 `book/slides/*.md` → 一个 .pptx。"""
    meta, text = build_slides.split_front_matter(path.read_text(encoding="utf-8"))
    deck_title = meta.get("title", path.stem)
    subtitle = meta.get("subtitle", "")
    pages = build_slides.split_slides(text)
    total = len(pages)
    images, media, slides = {}, {}, []
    for number, raw in enumerate(pages, 1):
        lines, notes = build_slides.take_notes(raw)
        blocks = parse_blocks(lines)
        slides.extend(build_slide(blocks, notes_text(notes, ctx), deck_title,
                                  subtitle, number, total,
                                  ctx, images, media, cover=(number == 1)))
    # 页脚的「n / 总数」要按真实张数写——拆过页之后它和课件源的页数不再相等。
    for position, slide in enumerate(slides, 1):
        slide.shapes.extend(footer(deck_title, position, len(slides)))
    out = OUT_DIR / f"{path.stem}.pptx"
    W.write(out, slides, title=deck_title, images=media)
    return out, len(slides)


# --------------------------------------------------------------- 构建与校验

def sources():
    return sorted(p for p in SLIDES.glob("*.md") if p.name != "README.md")


def source_digest():
    """源文件 + 它们引用的插图 + 两个渲染器自身。任何一样变了，产物就该重做。"""
    digest = hashlib.sha256()
    paths = list(sources()) + [Path(__file__).resolve(),
                               Path(__file__).resolve().with_name("pptx_writer.py")]
    for path in paths:
        digest.update(rel_label(path).encode("utf-8"))
        digest.update(path.read_bytes())
    for path in sorted(referenced_images()):
        digest.update(rel_label(path).encode("utf-8"))
        digest.update(path.read_bytes())
    return digest.hexdigest()


def referenced_images():
    seen = set()
    for path in sources():
        for src in re.findall(r"!\[[^\]]*\]\(([^)\s]+)", path.read_text(encoding="utf-8")):
            if src.startswith(("http://", "https://", "data:")):
                continue
            target = (SLIDES / src).resolve()
            if target.is_file():
                seen.add(target)
    return seen


def build(check_only=False, only=None):
    ctx = Ctx()
    if check_only:
        if not INFO.is_file():
            print(f"❌ 还没有生成过课件 .pptx；修法：python3 {rel_label(Path(__file__))}",
                  file=sys.stderr)
            return 1
        info = json.loads(INFO.read_text(encoding="utf-8"))
        want = source_digest()
        missing = [name for name in info.get("decks", {})
                   if not (OUT_DIR / name).is_file()]
        if missing:
            print(f"❌ 课件 .pptx 缺文件：{'、'.join(missing)}", file=sys.stderr)
            return 1
        if info.get("source_sha256") != want:
            print(f"❌ 课件 .pptx 已过期：源文件摘要应为 {want[:12]}，"
                  f"sidecar 仍是 {str(info.get('source_sha256'))[:12]}", file=sys.stderr)
            print(f"   修法：python3 {rel_label(Path(__file__))}", file=sys.stderr)
            return 1
        print(f"✅ 课件 .pptx 与课件源一致：{len(info['decks'])} 份、"
              f"{sum(info['decks'].values())} 页")
        return 0

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    decks = {}
    targets = [p for p in sources() if only is None or only in p.name]
    for path in targets:
        out, pages = build_deck(path, ctx)
        decks[out.name] = pages
        print(f"  {out.name:<26} {pages:>3} 页  {out.stat().st_size // 1024:>5} KB")
    if only is None:
        stale = [p for p in OUT_DIR.glob("*.pptx") if p.name not in decks]
        for path in stale:
            path.unlink()
        INFO.write_text(json.dumps(
            {"decks": decks, "pages": sum(decks.values()),
             "source_sha256": source_digest()},
            ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    for problem in ctx.problems:
        print(f"❌ {problem}", file=sys.stderr)
    if ctx.unknown_tex:
        print(f"⚠️  认不出的 TeX 宏（已按名字原样印出）："
              f"{' '.join(sorted(ctx.unknown_tex))}", file=sys.stderr)
    if ctx.split:
        print(f"ℹ️  {len(ctx.split)} 页内容超过一屏，已自动拆成「…（续）」：", file=sys.stderr)
        for deck, title, parts in ctx.split[:12]:
            print(f"     {deck} · {title} → {parts} 页", file=sys.stderr)
    if ctx.crowded:
        print(f"⚠️  {len(ctx.crowded)} 页缩到最小仍然超出画面，建议在课件源里拆页：",
              file=sys.stderr)
        for deck, title, inches in ctx.crowded[:12]:
            print(f"     {deck} · {title}（超出约 {inches} 英寸）", file=sys.stderr)
    if ctx.problems:
        return 1
    print(f"✅ book/slides/pptx/  {len(decks)} 份课件、{sum(decks.values())} 页"
          f"  入口 {rel_label(OUT_DIR)}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="把课件源渲染成 .pptx")
    parser.add_argument("--check", action="store_true", help="只校验产物是否最新")
    parser.add_argument("--only", help="只做文件名含该串的那一份")
    args = parser.parse_args()
    return build(check_only=args.check, only=args.only)


if __name__ == "__main__":
    raise SystemExit(main())
