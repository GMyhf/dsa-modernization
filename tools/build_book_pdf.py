#!/usr/bin/env python3
"""把 book/ 各章组装成一本带书签的学生用 PDF。

不改 dsa_raw.md。不把 292 张图的图册整本塞进去（那会变成图集而不是教材）；
正文里已经嵌了教学需要的图。勘误作为附录。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repo import rel_label  # noqa: E402

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

# 截断自检用：tex 里写了多少章、多少张图，排出来的目录和日志里就该有多少。
CHAPTER_RE = re.compile(r"\\chapter\{")
TOC_CHAPTER_RE = re.compile(r"\\contentsline \{chapter\}")
GRAPHIC_RE = re.compile(r"\\includegraphics(?:\[[^\]]*\])?\{([^}]+)\}")
PAGES_RE = re.compile(r"Output written on \S+ \((\d+) pages?")


FIGURE_REF_RE = re.compile(r"!\[[^\]]*\]\(([^)]+)\)")


def referenced_assets() -> list[Path]:
    """各章正文引用到、并且会被嵌进 PDF 的本地图片。

    **2026-08-17 复查补上**：第一版的摘要只覆盖 `.md` 与 preamble，
    于是「换掉一张插图」这件事不会让 `--check` 变红——而那 292 张图是**嵌在 PDF 里**的，
    `vendor_figures.py` 重新下载一张就足以让已发布的 PDF 与书稿分家。
    实测：给一张 jpg 尾部追加两个字节，`--check` 照样报「与源文件一致」。
    """
    seen, assets = set(), []
    for chapter in CHAPTERS:
        if not chapter.is_file():
            continue
        for target in FIGURE_REF_RE.findall(chapter.read_text(encoding="utf-8", errors="replace")):
            target = target.split()[0].strip()
            if target.startswith(("http://", "https://", "data:")):
                continue  # 远程热链进不了离线 PDF，R4 也不许它存在
            path = (chapter.parent / target).resolve()
            if path.is_file() and path not in seen:
                seen.add(path)
                assets.append(path)
    return sorted(assets)


def build_inputs() -> list[Path]:
    """会改变 PDF 内容的全部入参，顺序本身也是摘要的一部分。"""
    return [*CHAPTERS, PREAMBLE, Path(__file__).resolve(), *referenced_assets()]


def source_sha256() -> str:
    """对相对路径与文件内容一起取摘要，避免同内容文件换位而不报旧。"""
    digest = hashlib.sha256()
    for path in build_inputs():
        if not path.is_file():
            raise FileNotFoundError(path)
        try:
            label = path.resolve().relative_to(ROOT.resolve()).as_posix()
        except ValueError:
            label = path.name
        digest.update(label.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def check_current() -> int:
    """只读检查发布 PDF 是否由当前书稿构建，接口与 build_site --check 同形。"""
    info_path = PDF_DIR / "build-info.json"
    problems = []
    if not OUTPUT.is_file():
        problems.append(f"缺少 {rel_label(OUTPUT)}")
    if not info_path.is_file():
        problems.append(f"缺少 {rel_label(info_path)}")
        info = {}
    else:
        try:
            info = json.loads(info_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            problems.append(f"{rel_label(info_path)} 不是有效 JSON：{exc}")
            info = {}

    try:
        current = source_sha256()
    except (OSError, FileNotFoundError) as exc:
        problems.append(f"无法读取 PDF 构建输入：{exc}")
        current = ""
    recorded = info.get("source_sha256")
    if current and recorded != current:
        if recorded:
            problems.append(
                f"PDF 已过期：源文件摘要应为 {current[:12]}，sidecar 仍是 {recorded[:12]}"
            )
        else:
            problems.append("PDF 已过期：sidecar 没有 source_sha256")

    for field in ("pages", "chapters", "main_chapters", "figures"):
        if not isinstance(info.get(field), int) or info[field] <= 0:
            problems.append(f"sidecar 的 {field} 不是正整数")

    if problems:
        for problem in problems:
            print(f"❌ {problem}")
        print("   修法：python3 tools/build_book_pdf.py")
        return 1
    print(
        f"✅ PDF 与源文件一致：{info['pages']} 页、{info['chapters']} 章、"
        f"{info['figures']} 张图，sha256 {current[:12]}"
    )
    return 0


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


def drop_partial_pdf() -> None:
    """xelatex 中途停机也会写出一份「半本书」。留着它，早晚有人当成品拷走。

    2026-08-14 发布的那份 189 页 PDF 就是这么来的：MOOC 附录里一处 `$ S = \\sum…$`
    让 xelatex 停在第 187 页，291 张插图和勘误附录整整两章没排进去。
    """
    partial = WORK / "book.pdf"
    if partial.is_file():
        partial.unlink()
        sys.stderr.write(f"已删除半成品 {partial.relative_to(ROOT)}，不要拿它当成品\n")


def run_xelatex(tex_path: Path) -> None:
    cmd = [
        "xelatex",
        "-interaction=nonstopmode",
        "-halt-on-error",
        # 日志按 79 列折行会把长图片路径拆断，自检就数不准图了。
        "-max-print-line=1000",
        tex_path.name,
    ]
    proc = subprocess.run(cmd, cwd=WORK, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout[-4000:])
        sys.stderr.write(proc.stderr[-2000:])
        drop_partial_pdf()
        raise SystemExit(proc.returncode)


def verify_not_truncated(tex_path: Path, toc_path: Path, log_text: str) -> int:
    """退出码为 0 还不算数：确认整本书都排完了、图都嵌进去了，返回页数。

    三条都是从 book.tex 推出来的，不写死页数——章数和图数变了，期望值自己跟着变。
    `log_text` 必须是 book.log 的内容：嵌图记录只写进日志文件，终端输出里没有。
    """
    tex = tex_path.read_text(encoding="utf-8", errors="replace")
    problems = []

    want_chapters = len(CHAPTER_RE.findall(tex))
    toc = toc_path.read_text(encoding="utf-8", errors="replace") if toc_path.is_file() else ""
    got_chapters = len(TOC_CHAPTER_RE.findall(toc))
    if got_chapters != want_chapters:
        problems.append(
            f"目录只排到 {got_chapters} 章，book.tex 里有 {want_chapters} 章——书被截断了"
        )

    want_figures = {Path(src).name for src in GRAPHIC_RE.findall(tex)}
    got_figures = {name for name in want_figures if name in log_text}
    if got_figures != want_figures:
        missing = sorted(want_figures - got_figures)
        problems.append(
            f"{len(missing)}/{len(want_figures)} 张图没进 PDF，例如 {missing[:3]}"
        )

    match = PAGES_RE.search(log_text)
    if match is None:
        problems.append("日志里没有 Output written on … pages，xelatex 没正常收尾")
    pages = int(match.group(1)) if match else 0

    if problems:
        for line in problems:
            sys.stderr.write(f"PDF 自检失败：{line}\n")
        drop_partial_pdf()
        raise SystemExit(1)
    print(f"PDF 自检通过：{pages} 页、{want_chapters} 章、{len(want_figures)} 张图")
    return pages


def write_build_info(tex_path: Path, log_text: str, pages: int) -> Path:
    """把「这本 PDF 有多少页/章/图」写成 sidecar，供网页版的下载卡片显示。

    页数只在 xelatex 的日志里（PDF 自身的页树是压缩的，正则挖不出来），而日志在
    `.build/` 里不入库。与其让网页去猜或让人手写一个会过期的数字，不如在**唯一知道
    答案的时刻**把它落到磁盘上。不写时间戳：同一份书稿重排两次，这个文件应当一模一样。
    """
    tex = tex_path.read_text(encoding="utf-8", errors="replace")
    info = {
        "chapters": len(CHAPTER_RE.findall(tex)),
        "main_chapters": sum(1 for path in CHAPTERS if re.fullmatch(r"ch\d\d-.+\.md", path.name)),
        "figures": len({Path(src).name for src in GRAPHIC_RE.findall(tex)}),
        "pages": pages,
        "source_sha256": source_sha256(),
    }
    path = PDF_DIR / "build-info.json"
    path.write_text(json.dumps(info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {rel_label(path)} {info}")      # 测试会把 PDF_DIR 指到 /tmp，别用 relative_to
    return path


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
    # 自检不过就不许覆盖已发布的成品：宁可留着旧版，也不发一本缺章少图的书。
    log_text = (WORK / "book.log").read_text(encoding="utf-8", errors="replace")
    pages = verify_not_truncated(tex_path, WORK / "book.toc", log_text)
    write_build_info(tex_path, log_text, pages)
    # macOS 上目标 PDF 可能已存在；显式复制并替换，避免旧文件被保留。
    shutil.copy2(str(built), str(pdf_path))
    built.unlink()
    print("xelatex ok")


def main() -> int:
    parser = argparse.ArgumentParser(description="组装并编译学生用 PDF")
    parser.add_argument("--assemble-only", action="store_true")
    parser.add_argument("--check", action="store_true",
                        help="只检查已发布 PDF 是否由当前源文件构建")
    opts = parser.parse_args()
    if opts.check:
        return check_current()
    PDF_DIR.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    shutil.copy2(PREAMBLE, WORK / "preamble.tex")
    text = assemble()
    ASSEMBLED.write_text(text, encoding="utf-8")
    print(f"assembled {ASSEMBLED.relative_to(ROOT)} ({len(text.splitlines())} lines)")
    if opts.assemble_only:
        return 0
    run_pandoc(ASSEMBLED, OUTPUT)
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
