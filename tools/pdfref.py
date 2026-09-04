#!/usr/bin/env python3
"""pdfref.py — 把原书扫描件当作「原文到底怎么写的」的最终裁判。

为什么要有这个工具：

`dsa_raw.md` 是 OCR 底稿，它证明的是「原书大致写了什么」，不是「原书逐字写了什么」。
公式被拆散、`∞` 认成 8、右花括号认成 1、整段右侧注释被甩到正文里——凡是要**照抄原书
文字**的活儿，底稿都不够格当出处。2026-09-04 人把 2008 年原版的扫描 PDF
（396 页，纯图像，无文字层）放进仓库根目录，从此有了逐字级别的出处。

PDF 是扫描图像，没有文字层（`pdftotext` 输出全空），所以这个工具不「提取文字」，
它只做一件事：**把指定的书页渲染成图片，交给能看图的人或模型去读。**

    书页页码 = PDF 页码 − 14
        证据：PDF 第 15 页是印着页码 1 的「第1章 概论」首页；
              PDF 第 200 页是印着页码 186 的【算法7.10】Prim 算法。

节 → 页码的索引不是手写的，是从 `dsa_raw.md` 前言里的目录（带点线页码）解析出来的，
和 ledger.py 的做法一样：手写的对照表会腐烂，算出来的不会。

用法:
  python3 tools/pdfref.py --section 8.4        # 渲染 8.4 节所在的全部书页
  python3 tools/pdfref.py --section 8.4 --context 1   # 前后各多给一页
  python3 tools/pdfref.py --pages 186-189      # 直接按书页页码渲染
  python3 tools/pdfref.py --listing 算法7.10    # 按清单编号定位到它所在的节
  python3 tools/pdfref.py --list               # 打印解析出来的目录索引
  python3 tools/pdfref.py --check              # 只校验：PDF 在不在、目录解析全不全

PDF 本身**不入库**（26MB 扫描件），所以 --check 在缺 PDF 时只提示、不判红：
闸门不能因为某台机器上没放扫描件就变红。
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

RAW = ROOT / "dsa_raw.md"
OUT_DIR = ROOT / ".build" / "pdfref"

# PDF 页码 − 书页页码。两处独立对照都给出 14，见模块头。
PAGE_OFFSET = 14
PDF_PAGES = 396

# 目录条目：`1.4.4 求解问题时数据结构的` / `第3 章 栈与队列………46` / `2. 3.2 双链表………40`
# OCR 会在数字之间塞空格，所以点号两侧都允许空白。
# OCR 还会把小数点认成逗号（`8,6.1 桶式排序`），把「第」整个吃掉（`6 章树……135`）。
TOC_SECTION_RE = re.compile(r"^\s*(\d+(?:\s*[.,]\s*\d+){1,2})\s*(.*)$")
TOC_CHAPTER_RE = re.compile(r"^\s*(?:第\s*)?(\d+)\s*章\s*(.*)$")
# 每章末尾这三条也要单独成条，否则它们会被当成上一节的续行，把页码带偏。
TOC_TAIL_RE = re.compile(r"^\s*(本章小结|习题|上机题|参考文献)")
# 行尾页码：允许后面挂着点线、空格、软换行留下的两个空格
TAIL_PAGE_RE = re.compile(r"[…\.\s]*?(\d{1,3})\s*$")
# 正文里的节标题（底稿）
SECTION_HEAD_RE = re.compile(r"^#{1,3}\s*(\d+\.\d+(?:\.\d+)?)\s+(.*)$")
CHAPTER_HEAD_RE = re.compile(r"^#\s*第(\d+)章\s*(.*)$")
LISTING_RE = re.compile(r"【\s*(算法|代码)\s*(\d+\.\d+[a-zA-Z]?)\s*】")


def find_pdf(explicit=None):
    """找到扫描件。顺序：显式参数 → 环境变量 DSA_PDF → 仓库根目录里的候选 PDF。"""
    if explicit:
        p = Path(explicit)
        return p if p.is_file() else None
    env = os.environ.get("DSA_PDF")
    if env and Path(env).is_file():
        return Path(env)
    for candidate in sorted(ROOT.glob("*.pdf")):
        if "数据结构与算法" in candidate.name:
            return candidate
    return None


def _toc_lines():
    """目录区间：从「目 录」那一行到第 1 章正文之前。"""
    lines = RAW.read_text(encoding="utf-8").splitlines()
    start = None
    for i, line in enumerate(lines):
        if line.strip().replace(" ", "") == "目录":
            start = i + 1
            break
    if start is None:
        return []
    end = len(lines)
    for i in range(start, len(lines)):
        if lines[i].startswith("# 第1章"):
            end = i
            break
    return lines[start:end]


def parse_toc():
    """解析目录，返回 {'1.4.4': 22, '第7章': 155, ...}（值是印在书上的页码）。

    OCR 把长题名折成两行（「求解问题时数据结构的 / 选择和评价…………22」），
    所以要把不带编号的续行并回上一条，再从那一条的末尾取页码。
    """
    entries = []  # [(key, [文本行...])]
    for raw_line in _toc_lines():
        line = raw_line.rstrip()
        if not line.strip():
            continue
        m = TOC_CHAPTER_RE.match(line)
        if m:
            entries.append((f"第{m.group(1)}章", [line]))
            continue
        m = TOC_SECTION_RE.match(line)
        if m:
            key = re.sub(r"\s+", "", m.group(1)).replace(",", ".")
            entries.append((key, [line]))
            continue
        m = TOC_TAIL_RE.match(line)
        if m:
            entries.append((f"{m.group(1)}@{len(entries)}", [line]))
            continue
        if entries:  # 续行：并回上一条
            entries[-1][1].append(line)
    index = {}
    unresolved = []
    for key, chunk in entries:
        if "@" in key:  # 章末三条只用来断开续行，不进索引
            continue
        tail = chunk[-1]
        m = TAIL_PAGE_RE.search(tail)
        if not m:
            unresolved.append(key)
            continue
        index.setdefault(key, int(m.group(1)))
    return index, unresolved


def section_order(index):
    """把目录条目排成阅读顺序，用来推算一节的结束页。"""

    def sort_key(key):
        m = re.match(r"^第(\d+)章$", key)
        if m:
            return (int(m.group(1)), [])
        parts = [int(x) for x in key.split(".")]
        return (parts[0], parts[1:])

    return sorted(index, key=sort_key)


def page_range(section, index, context=0):
    """一节覆盖的书页页码区间（闭区间）。结束页取下一条目录条目的页码。"""
    order = section_order(index)
    if section not in index:
        return None
    pos = order.index(section)
    first = index[section]
    last = first
    for nxt in order[pos + 1:]:
        if index[nxt] >= first:
            last = index[nxt]
            break
    return (max(1, first - context), last + context)


def listing_section(listing_id):
    """【算法7.10】落在哪一节：在底稿里找到它，往上回溯最近的节标题。"""
    lines = RAW.read_text(encoding="utf-8").splitlines()
    want = re.sub(r"\s+", "", listing_id)
    hit = None
    for i, line in enumerate(lines):
        for kind, number in LISTING_RE.findall(line):
            if f"{kind}{number}" == want:
                hit = i
                break
        if hit is not None:
            break
    if hit is None:
        return None
    for j in range(hit, -1, -1):
        m = SECTION_HEAD_RE.match(lines[j])
        if m:
            return m.group(1)
        m = CHAPTER_HEAD_RE.match(lines[j])
        if m:
            return f"第{m.group(1)}章"
    return None


def render(pdf, printed_pages, dpi=150, out_dir=OUT_DIR):
    """把书页页码列表渲染成 PNG，返回落地的文件列表。已存在的直接复用。"""
    out_dir.mkdir(parents=True, exist_ok=True)
    made = []
    for printed in printed_pages:
        pdf_page = printed + PAGE_OFFSET
        if not 1 <= pdf_page <= PDF_PAGES:
            continue
        target = out_dir / f"p{printed:03d}.png"
        if not target.is_file():
            prefix = out_dir / f"tmp{printed:03d}"
            subprocess.run(
                ["pdftoppm", "-r", str(dpi), "-f", str(pdf_page), "-l", str(pdf_page),
                 "-png", str(pdf), str(prefix)],
                check=True,
            )
            produced = sorted(out_dir.glob(f"tmp{printed:03d}-*.png"))
            if not produced:
                raise RuntimeError(f"pdftoppm 没有产出 PDF 第 {pdf_page} 页")
            produced[0].rename(target)
        made.append(target)
    return made


def parse_pages(spec):
    """`186-189` / `186` / `186,190-191` → [186, 187, ...]"""
    pages = []
    for chunk in spec.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        if "-" in chunk:
            a, b = chunk.split("-", 1)
            pages.extend(range(int(a), int(b) + 1))
        else:
            pages.append(int(chunk))
    return pages


def main(argv=None):
    ap = argparse.ArgumentParser(description="按节号或页码渲染原书扫描页")
    ap.add_argument("--section", help="书里的节号，如 8.4 或 8.4.2；也接受「第7章」")
    ap.add_argument("--listing", help="清单编号，如 算法7.10 / 代码3.2")
    ap.add_argument("--pages", help="书页页码，如 186-189")
    ap.add_argument("--context", type=int, default=0, help="节的前后各多取几页")
    ap.add_argument("--dpi", type=int, default=150)
    ap.add_argument("--pdf", help="扫描件路径（默认找仓库根目录的《数据结构与算法》PDF）")
    ap.add_argument("--list", action="store_true", help="打印目录索引")
    ap.add_argument("--check", action="store_true", help="只校验，不渲染")
    args = ap.parse_args(argv)

    index, unresolved = parse_toc()
    pdf = find_pdf(args.pdf)

    if args.list:
        for key in section_order(index):
            print(f"{key:<10} 书页 {index[key]}")
        return 0

    if args.check:
        print(f"目录索引：{len(index)} 条（章 {sum(1 for k in index if k.startswith('第'))}，"
              f"节 {sum(1 for k in index if not k.startswith('第'))}）")
        if unresolved:
            print(f"⚠️ {len(unresolved)} 条目录条目没解析出页码：{'、'.join(unresolved[:8])}")
        if pdf is None:
            print("ℹ️ 仓库里没有扫描件（26MB，不入库）。需要逐字核对原文时，"
                  "把 2008 年原版 PDF 放进仓库根目录，或用 DSA_PDF 指向它。")
        else:
            print(f"✅ 扫描件：{rel_label(pdf)}（书页 = PDF 页 − {PAGE_OFFSET}）")
        return 1 if unresolved else 0

    section = args.section
    if args.listing:
        section = listing_section(args.listing)
        if section is None:
            print(f"❌ 底稿里找不到 {args.listing}", file=sys.stderr)
            return 1
        print(f"{args.listing} 位于第 {section} 节")

    if args.pages:
        pages = parse_pages(args.pages)
    elif section:
        rng = page_range(section, index, args.context)
        if rng is None:
            print(f"❌ 目录里没有第 {section} 节", file=sys.stderr)
            return 1
        pages = list(range(rng[0], rng[1] + 1))
        print(f"第 {section} 节：书页 {rng[0]}–{rng[1]}（PDF 第 "
              f"{rng[0] + PAGE_OFFSET}–{rng[1] + PAGE_OFFSET} 页）")
    else:
        ap.print_help()
        return 2

    if pdf is None:
        print("❌ 找不到扫描件；把原版 PDF 放进仓库根目录，或用 --pdf/DSA_PDF 指定。",
              file=sys.stderr)
        return 1

    for path in render(pdf, pages, args.dpi):
        print(rel_label(path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
