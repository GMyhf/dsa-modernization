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
