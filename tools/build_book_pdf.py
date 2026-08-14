#!/usr/bin/env python3
"""把 book/ 各章组装成一本带书签的学生用 PDF。

不改 dsa_raw.md。不把 292 张图的图册整本塞进去（那会变成图集而不是教材）；
正文里已经嵌了教学需要的图。勘误作为附录。
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOOK = ROOT / "book"
PDF_DIR = BOOK / "pdf"
WORK = ROOT / ".build" / "book-pdf"
ASSEMBLED = WORK / "assembled.md"
PREAMBLE = PDF_DIR / "preamble.tex"
OUTPUT = PDF_DIR / "现代C++数据结构教程.pdf"

CHAPTERS = [
    BOOK / "现代C++数据结构教程.md",
    BOOK / "ch01-adt.md",
    BOOK / "ch02-linear-list.md",
    BOOK / "ch03-stack.md",
    BOOK / "ch04-string.md",
    BOOK / "ch05-binary-tree.md",
    BOOK / "ch06-tree.md",
    BOOK / "ch07-graph.md",
    BOOK / "ch08-sorting.md",
    BOOK / "ch09-external-sort.md",
    BOOK / "ch10-search.md",
    BOOK / "ch11-index.md",
    BOOK / "ch12-advanced.md",
    BOOK / "习题与参考答案.md",
    ROOT / "DSA_MOOC_solution.md",
    BOOK / "插图.md",
    BOOK / "勘误.md",
]

FRONT_MATTER = """---
title: 现代 C++ 数据结构教程
author:
  - 基于张铭、王腾蛟、赵海燕《数据结构与算法》重编
  - 高等教育出版社 2008 年版教学内容的现代化讲义
lang: zh-CN
documentclass: ctexbook
classoption:
  - UTF8
  - heading=true
  - scheme=plain
  - fontset=none
---

\\frontmatter

"""

FENCE_RE = re.compile(r"^```(?P<info>[^\n]*)$", re.M)
LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
COLLAB_RE = re.compile(r"\[(`?[^`\]]+`?)\]\(\.\./collab/[^)]+\)")
CODE_LINK_RE = re.compile(r"\[([^\]]+)\]\(\.\./code/[^)]+\)")


def strip_file_attr(info: str) -> str:
    parts = [p for p in info.split() if not p.startswith("file=")]
    return " ".join(parts).strip()


def rewrite_fences(text: str) -> str:
    lines = text.splitlines()
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("```"):
            info = line[3:].strip()
            lang = strip_file_attr(info)
            out.append("```" + lang)
            i += 1
            while i < len(lines) and lines[i].strip() != "```":
                out.append(lines[i])
                i += 1
            if i < len(lines):
                out.append("```")
            i += 1
            continue
        out.append(line)
        i += 1
    return "\n".join(out) + "\n"


def rewrite_links(text: str) -> str:
    text = COLLAB_RE.sub(r"\1", text)
    text = CODE_LINK_RE.sub(r"\1", text)

    def keep_local(match: re.Match) -> str:
        label, target = match.group(1), match.group(2)
        if target.startswith("assets/"):
            return match.group(0)
        if target.startswith("#"):
            return match.group(0)
        if target.startswith("http"):
            return match.group(0)
        # 章间相对链接改成纯文本，避免 PDF 里一堆断链
        return label

    return LINK_RE.sub(keep_local, text)


def drop_repo_meta(text: str, path: Path) -> str:
    """学生 PDF 不需要仓库地位说明和指向 collab 的工程注释。"""
    if path.name == "现代C++数据结构教程.md":
        # 封面已经印了书名，这篇只作不编号的前言，避免 TOC 里再出现一章同名。
        text = text.replace("# 现代 C++ 数据结构教程\n", "# 写给学生\n", 1)
        text = text.replace(
            "这是 `dsa_raw.md` 的可读替代稿：保留原书的章节脉络、数据结构与算法思想，移除 OCR\n"
            "> 噪声，并把所有示例统一为可编译、可测试的 C++17。原 OCR 底稿继续只作为考证材料保留，\n"
            "> 不应作为学习文本阅读。\n",
            "保留原书的章节脉络、数据结构与算法思想；全部示例统一为可编译、可测试的 C++17。\n",
        )
        text = text.replace(
            "每章正文位于本目录的独立 Markdown 文件。这样做有两个好处：阅读器能快速跳转，且每一段\n"
            "C++ 都能与 `code/` 下通过测试的源码逐字同步。代码块中的 `file=` 标注就是其来源。\n\n",
            "每一段印在书上的 C++ 都与配套源码逐字一致。"
            "建议按第 1 至第 12 章顺序阅读：先理解问题，再运行该章的示例程序，最后对照实现。\n\n",
        )
        text = re.sub(
            r"对照纸书时，编译级硬伤与算法错见 .*?\n\n",
            "对照 2008 年纸书时，编译级硬伤与算法错见书末「原书勘误」。\n\n",
            text,
            count=1,
        )
        text = text.replace(
            "完整的边界条件、未覆盖故障路径与\n"
            "递归深度风险见仓库中的未验证风险说明。\n",
            "递归实现在极深退化结构上存在栈溢出风险，第 5 章给出了实测数字。\n",
        )
        # 前言里的「目录」一节会和 LaTeX 自动目录重名，改成阅读地图。
        text = text.replace("## 目录\n", "## 各章一览\n", 1)
    # 只删连续的引用行，绝不能用 DOTALL，否则会把后面整章吃掉。
    text = re.sub(
        r"(?m)^> \*\*本文件的地位\*\*.*(?:\n>.*)*\n?",
        "",
        text,
    )
    # Times New Roman 没有这些 Unicode 下标，改成普通字符以免印成方框。
    subs = {
        "ₚ": "p",
        "ₙ": "n",
        "₀": "0",
        "₁": "1",
        "₂": "2",
        "₃": "3",
        "₄": "4",
        "₅": "5",
        "₆": "6",
        "₇": "7",
        "₈": "8",
        "₉": "9",
        "₊": "+",
        "₋": "-",
        "ₐ": "a",
        "ₑ": "e",
        "ₓ": "x",
        "ᵢ": "i",
    }
    for src, dst in subs.items():
        text = text.replace(src, dst)
    return text


def demote_figures(text: str) -> str:
    """不要让 pandoc 给插图再套一层「图 1:」，正文里已经有「图1.1 …」。

    路径写成绝对，因为 xelatex 在 .build/book-pdf/ 里跑。
    """

    def repl(match: re.Match) -> str:
        src = match.group(2)
        abs_src = (BOOK / src).resolve()
        return f"![]({abs_src})"

    return re.sub(r"!\[([^\]]*)\]\((assets/[^)]+)\)", repl, text)


def assemble() -> str:
    parts = [FRONT_MATTER]
    for path in CHAPTERS:
        if not path.is_file():
            raise SystemExit(f"missing {path}")
        body = path.read_text(encoding="utf-8")
        body = drop_repo_meta(body, path)
        body = rewrite_fences(body)
        body = rewrite_links(body)
        body = demote_figures(body)
        # 每章从新页开始，但前言那份总目录不要再套一层 chapter
        if path.name == "ch01-adt.md":
            parts.append("\\mainmatter\n")
        elif path.name != "现代C++数据结构教程.md":
            parts.append("\\newpage\n")
        if path.name == "勘误.md":
            parts.append("\\appendix\n")
        parts.append(body.rstrip() + "\n")
    return "\n".join(parts)


def run_xelatex(tex_path: Path) -> None:
    cmd = [
        "xelatex",
        "-interaction=nonstopmode",
        "-halt-on-error",
        tex_path.name,
    ]
    proc = subprocess.run(cmd, cwd=WORK, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout[-4000:])
        sys.stderr.write(proc.stderr[-2000:])
        raise SystemExit(proc.returncode)


def run_pandoc(md_path: Path, pdf_path: Path) -> None:
    if not shutil.which("pandoc"):
        raise SystemExit("需要 pandoc")
    if not shutil.which("xelatex"):
        raise SystemExit("需要 xelatex（MacTeX / TeX Live）")
    tex_path = WORK / "book.tex"
    cmd = [
        "pandoc",
        str(md_path),
        "-o",
        str(tex_path),
        "--from",
        "markdown+tex_math_dollars+raw_tex+pipe_tables+grid_tables+fenced_code_attributes",
        "--toc",
        "--toc-depth=3",
        "--top-level-division=chapter",
        f"--include-in-header={WORK / 'preamble.tex'}",
        "--resource-path",
        str(BOOK),
        "-V",
        "documentclass=ctexbook",
        "-V",
        "classoption=UTF8",
        "-V",
        "classoption=fontset=none",
        "-V",
        "linestretch=1.15",
        "--highlight-style=tango",
    ]
    print(" ".join(cmd))
    proc = subprocess.run(cmd, cwd=WORK, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        raise SystemExit(proc.returncode)
    # 两遍：第一遍写目录与书签，第二遍把页码和大纲钉死。
    run_xelatex(tex_path)
    run_xelatex(tex_path)
    built = WORK / "book.pdf"
    if not built.is_file():
        raise SystemExit("xelatex 没有写出 book.pdf")
    shutil.move(str(built), str(pdf_path))
    print("xelatex ok")


def main() -> None:
    parser = argparse.ArgumentParser(description="组装并编译学生用 PDF")
    parser.add_argument("--assemble-only", action="store_true")
    opts = parser.parse_args()
    PDF_DIR.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    shutil.copy2(PREAMBLE, WORK / "preamble.tex")
    text = assemble()
    ASSEMBLED.write_text(text, encoding="utf-8")
    print(f"assembled {ASSEMBLED.relative_to(ROOT)} ({len(text.splitlines())} lines)")
    if opts.assemble_only:
        return
    run_pandoc(ASSEMBLED, OUTPUT)
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
