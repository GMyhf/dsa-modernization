#!/usr/bin/env python3
"""figcrop.py — 从原书扫描件裁出插图，让「书上这张图从哪来」有出处。

为什么要有这个工具：

`book/assets/` 里的 292 张图来自上游 OCR 图库（文件名就是内容哈希，
`collect_figures.py --check` 逐字节核对）。但 `book/assets/combined/` 下的 43 张
**不在那套核对里**：原书印成一整张的图，OCR 把它按子图切碎、还把「(a) 无向完全图」
这类子题注扯进正文，于是 2026-08-23 那一轮把碎片重新拼起来、**把子题注用程序重新
画了回去**。结果是对的，但那些字是我们排的、版式是我们选的，**整个过程没有任何
记录**——既不在闸门里，也不在决策记录里。

2026-09-04 扫描件进仓库之后，这一步可以省掉：直接从印刷页上裁一张干净的原图。
本工具把「裁哪一页、裁哪一块」写进 `collab/figures_scan.json`，于是：

    图号 → (书页, 裁剪框, dpi) → PNG      # 可复算
    PNG  → sha256                          # 可核对

`--check` 只做后一件事：核对每张已登记的图与其记录的哈希一致。**它不重新裁**——
重裁需要扫描件，而扫描件不入库（26MB），闸门不能因为某台机器上没有它就变红。

用法:
  python3 tools/figcrop.py --list                 # 列出登记表与现状
  python3 tools/figcrop.py --page 173             # 渲染整页，用来量裁剪框
  python3 tools/figcrop.py --make 图7.15          # 按登记表裁一张（需要扫描件）
  python3 tools/figcrop.py --make-all             # 裁全部登记过的图
  python3 tools/figcrop.py --check                # 逐张核对哈希（闸门用）
  python3 tools/figcrop.py --pending              # 书稿里还在用 combined/ 的图
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具
import pdfref  # noqa: E402  共用扫描件定位与页码偏移

BOOK = ROOT / "book"
SCAN_DIR = BOOK / "assets" / "scan"
REGISTRY = ROOT / "collab" / "figures_scan.json"
PROBE_DIR = ROOT / ".build" / "figcrop"

# 裁剪框按这个分辨率标定；改了它，登记表里所有的框都要重标。
DPI = 200
COMBINED_RE = re.compile(r"assets/combined/([A-Za-z0-9._-]+\.png)")
SCAN_RE = re.compile(r"assets/scan/([A-Za-z0-9._-]+\.png)")


def load_registry():
    if not REGISTRY.is_file():
        return {"_doc": [], "figures": []}
    return json.loads(REGISTRY.read_text(encoding="utf-8"))


def save_registry(data):
    REGISTRY.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crop(pdf: Path, printed_page: int, box, out: Path, dpi=DPI):
    """按 (x, y, w, h) 像素框裁一张 PNG。坐标是 dpi 下整页光栅的像素。"""
    out.parent.mkdir(parents=True, exist_ok=True)
    pdf_page = printed_page + pdfref.PAGE_OFFSET
    prefix = out.parent / f"tmp-{out.stem}"
    x, y, w, h = box
    subprocess.run(
        ["pdftoppm", "-r", str(dpi), "-f", str(pdf_page), "-l", str(pdf_page), "-png",
         "-x", str(x), "-y", str(y), "-W", str(w), "-H", str(h), str(pdf), str(prefix)],
        check=True,
    )
    produced = sorted(out.parent.glob(f"tmp-{out.stem}-*.png"))
    if not produced:
        raise RuntimeError(f"pdftoppm 没有产出书页 {printed_page} 的裁剪结果")
    produced[0].replace(out)
    return out


def book_files():
    """书稿正文加课件——课件也引用 assets/scan/，漏掉它会把在用的图判成没人用。"""
    return sorted(BOOK.glob("*.md")) + sorted((BOOK / "slides").glob("*.md"))


def used_paths(pattern):
    """书稿里实际引用到的图片文件名 → 引用它的页面。"""
    used = {}
    for path in book_files():
        for name in pattern.findall(path.read_text(encoding="utf-8", errors="replace")):
            used.setdefault(name, []).append(path.name)
    return used


def cmd_pending():
    used = used_paths(COMBINED_RE)
    if not used:
        print("✅ 书稿里已经没有 assets/combined/ 的引用了")
        return 0
    print(f"书稿里还在用拼接图的有 {len(used)} 张：")
    for name in sorted(used):
        print(f"  {name:<20} ← {'、'.join(sorted(set(used[name])))}")
    return 0


def cmd_list(data):
    figures = data.get("figures", [])
    print(f"{'图号':<10}{'书页':>6}{'文件':<22}{'状态'}")
    for item in figures:
        target = SCAN_DIR / item["file"]
        if not target.is_file():
            state = "缺文件"
        elif sha256_of(target) != item.get("sha256"):
            state = "哈希不符"
        else:
            state = "ok"
        print(f"{item['id']:<10}{item['page']:>6}  {item['file']:<20}{state}")
    print(f"共 {len(figures)} 张已登记；书稿仍在用 combined/ 的见 --pending")
    return 0


EXERCISE_HEADS = ("## 习题", "## 上机题")
FIG_MENTION = re.compile(r"(原书)?图\s*(\d+\.\d+)")


def dangling_exercise_figures():
    """习题里点名的图，本章里必须真的有一张图。

    正文可以把原书的插图改排成表格或分步清单——内容没丢，形式变了，这是本项目的常态。
    **习题不行**：题面写着「对于图 7.26 所示的带权有向图……」而书里根本没有这张图，
    题目就做不了。所以只对 `## 习题` / `## 上机题` 两节较真，判据是「本章文件里画过它」
    （题干常引用正文早就画过的图）。留一个出口：写成「原书图 9.2」表示这张图在原书上、
    题面已把需要的数据抄进正文，不算欠图。
    """
    missing = []
    for path in sorted(BOOK.glob("ch*.md")):
        chapter = re.match(r"ch(\d+)", path.name).group(1).lstrip("0")
        lines = path.read_text(encoding="utf-8").split("\n")
        drawn = {f"图{num}" for line in lines if line.lstrip().startswith("![")
                 for _, num in FIG_MENTION.findall(line)}
        section, seen = None, set()
        for lineno, line in enumerate(lines, 1):
            if line.startswith("## "):
                section = line[3:].strip() if line.startswith(EXERCISE_HEADS) else None
            if section is None or line.lstrip().startswith("!["):
                continue
            for prefix, num in FIG_MENTION.findall(line):
                if prefix or not num.startswith(chapter + "."):
                    continue
                if f"图{num}" not in drawn and f"图{num}" not in seen:
                    seen.add(f"图{num}")
                    missing.append((path.name, section, f"图{num}", lineno))
    return missing


def cmd_check(data, used=None):
    """三件事：登记过的图字节没变；书稿引用的图都登记过；习题点名的图确实画在书上。

    `used` 只在自测里被替换掉——那时要单独检验「哈希对不对」，
    不该把真实书稿的引用一起拖进来。
    """
    figures = data.get("figures", [])
    problems = []
    for item in figures:
        for field in ("id", "file", "page", "box", "dpi", "sha256", "by", "date"):
            if field not in item:
                problems.append(f"{item.get('id', '?')}: 登记缺字段 {field}")
        target = SCAN_DIR / item.get("file", "")
        if not target.is_file():
            problems.append(f"{item.get('id')}: 文件不存在 {rel_label(target)}")
            continue
        digest = sha256_of(target)
        if digest != item.get("sha256"):
            problems.append(f"{item.get('id')}: {item['file']} 内容变了"
                            f"（登记 {str(item.get('sha256'))[:12]}，实际 {digest[:12]}）")
    used = used_paths(SCAN_RE) if used is None else used
    registered = {item.get("file") for item in figures}
    for name in sorted(used):
        if name not in registered:
            problems.append(f"书稿引用了 assets/scan/{name}，但它没有登记出处")
    for name, section, fig, lineno in dangling_exercise_figures():
        problems.append(f"{name}:{lineno} 「{section}」点名 {fig}，但这一章里没有这张图"
                        f"（题目做不了；确实只是提及原书请写成「原书{fig}」）")
    for message in problems:
        print(f"❌ {message}")
    if problems:
        return 1
    still = len(used_paths(COMBINED_RE))
    print(f"✅ 扫描件裁图一致：{len(figures)} 张登记在案、哈希全部匹配"
          + (f"；另有 {still} 张仍是拼接图（--pending 列出）" if still else "；已无拼接图"))
    return 0


def cmd_make(data, wanted, pdf):
    figures = {item["id"]: item for item in data.get("figures", [])}
    targets = list(figures) if wanted is None else [wanted]
    for fig_id in targets:
        item = figures.get(fig_id)
        if item is None:
            print(f"❌ 登记表里没有 {fig_id}", file=sys.stderr)
            return 1
        out = SCAN_DIR / item["file"]
        crop(pdf, item["page"], item["box"], out, item.get("dpi", DPI))
        item["sha256"] = sha256_of(out)
        print(f"✅ {fig_id} → {rel_label(out)}  {out.stat().st_size // 1024} KB")
    save_registry(data)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="从原书扫描件裁出插图")
    ap.add_argument("--list", action="store_true", help="列出登记表")
    ap.add_argument("--pending", action="store_true", help="列出书稿里还在用的拼接图")
    ap.add_argument("--check", action="store_true", help="核对已登记裁图的哈希")
    ap.add_argument("--make", metavar="图号", help="按登记表裁一张")
    ap.add_argument("--make-all", action="store_true", help="裁全部登记过的图")
    ap.add_argument("--page", type=int, help="渲染整页到 .build/figcrop/，用来量裁剪框")
    ap.add_argument("--dpi", type=int, default=DPI)
    ap.add_argument("--pdf", help="扫描件路径（默认找仓库根目录的原版 PDF）")
    args = ap.parse_args(argv)

    data = load_registry()
    if args.pending:
        return cmd_pending()
    if args.list:
        return cmd_list(data)
    if args.check:
        return cmd_check(data)

    pdf = pdfref.find_pdf(args.pdf)
    if pdf is None:
        print("❌ 找不到扫描件；把原版 PDF 放进仓库根目录，或用 --pdf/DSA_PDF 指定。",
              file=sys.stderr)
        return 1
    if args.page:
        made = pdfref.render(pdf, [args.page], args.dpi, PROBE_DIR)
        for path in made:
            print(rel_label(path))
        return 0
    if args.make or args.make_all:
        return cmd_make(data, args.make, pdf)
    ap.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
