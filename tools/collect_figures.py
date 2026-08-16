#!/usr/bin/env python3
"""从 dsa_raw.md 抽出插图与题注，下载到 book/assets/，写成 book/插图.md。

vendor_figures.py 只处理书稿里已经写出的热链。原书 292 张图几乎都还停在底稿里，
本脚本按章收集它们，用题注当 alt，让书稿可以离线看图。
"""
import argparse
import hashlib
import re
import sys
import urllib.request
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "dsa_raw.md"
ASSETS = ROOT / "book" / "assets"
OUT = ROOT / "book" / "插图.md"
TIMEOUT = 30

CHAPTER_RE = re.compile(r"^# 第(\d+)章")
IMAGE_RE = re.compile(r"!\[\]\((https://raw\.githubusercontent\.com/GMyhf/img/main/img/[^)]+)\)")
CAPTION_RE = re.compile(r"^(?:图\s*\d+(?:\.\d+)?|图\d+|（[a-z]）|\([a-z]\))", re.I)
# 「图X.Y ……」独占一行——这是原书题注的体例，比 CAPTION_RE 严，用于跨行找回。
NUMBERED_CAPTION_RE = re.compile(r"^图\s*\d+(?:\.\d+)?\s")
# 跨行找回时往下看几行。原书的图是浮动的，排版时正文绕着图走，
# OCR 展平之后题注常常被一两段正文顶到图后面好几行。
CAPTION_LOOKAHEAD = 8

CHAPTER_TITLES = {
    0: "封面与前言",
    1: "概论",
    2: "线性表",
    3: "栈与队列",
    4: "字符串",
    5: "二叉树",
    6: "树",
    7: "图",
    8: "内排序",
    9: "文件管理与外排序",
    10: "检索",
    11: "索引技术",
    12: "高级数据结构",
}


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "dsa-modernization/collect_figures"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:  # noqa: S310
        return resp.read()


def recover_caption(lines, image_index):
    """图后面紧邻几行里没有题注时，再往下找一段。返回 [] 或 [题注]。

    **为什么需要跨行找**：原书的图是浮动的，排版时正文绕着图走；OCR 把版面展平
    之后，题注常常被一两段正文顶到图后面好几行。相邻行判据因此够不着它——
    图1.4 的题注就隔着一行「存储区的数据」（那是从图里抠出来的标签），
    图7.1「用图描述通信网络」隔着两段正文。全书 14 张图是这种情况。

    **两条防止认错的判据**（都是量出来的，不是猜的）：

    1. 只认独占一行的 `图X.Y ……`（`NUMBERED_CAPTION_RE`），不认松散的「图……」；
    2. **题注后面紧跟着另一张图时，判它属于后面那张**。底稿第 6471 行的
       「图 7.23 带权图」正是这种——它下一行就是图 7.23 本身。少了这条判据，
       它会被错记到上一张图头上。

    扫描遇到下一张图或章节标题即停，所以两张连续的图不会互相抢题注。
    """
    for ahead in range(image_index + 1, min(image_index + 1 + CAPTION_LOOKAHEAD, len(lines))):
        text = lines[ahead].strip().lstrip(">").strip()
        if not text:
            continue
        if IMAGE_RE.search(text) or text.startswith("#"):
            return []
        if not NUMBERED_CAPTION_RE.match(text):
            continue
        # 题注属于它前面那张图还是后面那张？后面紧跟着图，就是后面那张的。
        for following in range(ahead + 1, len(lines)):
            if not lines[following].strip():
                continue
            if IMAGE_RE.search(lines[following]):
                return []
            break
        return [re.sub(r"\s+", " ", text)]
    return []


def collect():
    lines = RAW.read_text(encoding="utf-8").splitlines()
    chapter = 0
    figures = []
    for index, line in enumerate(lines):
        heading = CHAPTER_RE.match(line)
        if heading:
            chapter = int(heading.group(1))
        match = IMAGE_RE.search(line)
        if not match:
            continue
        url = match.group(1)
        captions = []
        for ahead in range(index + 1, min(index + 5, len(lines))):
            text = lines[ahead].strip().lstrip(">").strip()
            if not text:
                continue
            if IMAGE_RE.search(text):
                break
            if CAPTION_RE.search(text) or text.startswith("图"):
                captions.append(re.sub(r"\s+", " ", text))
                continue
            break
        if not captions:
            captions = recover_caption(lines, index)
        if captions:
            alt = "；".join(captions)
        elif chapter == 0:
            alt = f"前言插图（底稿第 {index + 1} 行，原书无独立题注）"
        else:
            alt = f"第{chapter}章插图（底稿第 {index + 1} 行，原书无独立题注）"
        figures.append({"chapter": chapter, "line": index + 1, "url": url, "alt": alt})
    return figures


def main():
    parser = argparse.ArgumentParser(description="把原书插图收到 book/assets/ 并生成图册")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--limit", type=int, default=0, help="只处理前 N 张，便于试跑")
    opts = parser.parse_args()

    figures = collect()
    if opts.limit:
        figures = figures[: opts.limit]
    print(f"底稿中 {len(figures)} 张图")
    if opts.dry_run:
        by_ch = defaultdict(int)
        for item in figures:
            by_ch[item["chapter"]] += 1
            print(f"  ch{item['chapter']:02d} L{item['line']}: {item['alt'][:60]}")
        print("分章", dict(by_ch))
        return

    ASSETS.mkdir(parents=True, exist_ok=True)
    new_files = 0
    failed = []
    for item in figures:
        try:
            blob = fetch(item["url"])
        except Exception as exc:  # 单张失败不让整本停
            failed.append((item["url"], str(exc)))
            item["local"] = None
            continue
        name = hashlib.sha256(blob).hexdigest()[:16] + ".jpg"
        target = ASSETS / name
        if not target.exists():
            target.write_bytes(blob)
            new_files += 1
        item["local"] = f"assets/{name}"
        print(f"  {item['local']}  ←  {item['alt'][:50]}")

    grouped = defaultdict(list)
    for item in figures:
        grouped[item["chapter"]].append(item)

    parts = [
        "# 原书插图",
        "",
        "> 从只读底稿 `dsa_raw.md` 抽出的 292 张图。字节落在 `book/assets/`，",
        "> alt 来自原书题注；少数图在底稿里没有独立题注，alt 会标明行号。",
        "> 正文各章只引用教学需要的几张；其余集中在本图册，避免把教程拆碎。",
        "",
    ]
    for chapter in sorted(grouped):
        heading = (
            "## 前言"
            if chapter == 0
            else f"## 第{chapter}章 {CHAPTER_TITLES.get(chapter, '')}".rstrip()
        )
        parts.append(heading)
        parts.append("")
        for item in grouped[chapter]:
            if item.get("local"):
                parts.append(f"![{item['alt']}]({item['local']})")
            else:
                parts.append(f"（下载失败，底稿第 {item['line']} 行）{item['alt']}")
            parts.append("")
            parts.append(f"{item['alt']}")
            parts.append("")
    OUT.write_text("\n".join(parts), encoding="utf-8")
    print(f"新落盘 {new_files} 个文件，图册 {OUT.relative_to(ROOT)}")
    if failed:
        print(f"失败 {len(failed)} 张：")
        for url, err in failed:
            print(f"  {url}: {err}")
        sys.exit(1)


if __name__ == "__main__":
    main()
