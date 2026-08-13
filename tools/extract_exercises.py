#!/usr/bin/env python3
"""从只读底稿抽出各章「本章小结 / 习题 / 上机题」，接到 book/chXX 末尾。

只做无争议的 OCR 修补（全角标点、拆开的 ++），不改题意。
已存在「## 本章小结」的章节不会重复追加。
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "dsa_raw.md"

CHAPTER_FILES = {
    1: ROOT / "book/ch01-adt.md",
    2: ROOT / "book/ch02-linear-list.md",
    3: ROOT / "book/ch03-stack.md",
    4: ROOT / "book/ch04-string.md",
    5: ROOT / "book/ch05-binary-tree.md",
    6: ROOT / "book/ch06-tree.md",
    7: ROOT / "book/ch07-graph.md",
    8: ROOT / "book/ch08-sorting.md",
    9: ROOT / "book/ch09-external-sort.md",
    10: ROOT / "book/ch10-search.md",
    11: ROOT / "book/ch11-index.md",
    12: ROOT / "book/ch12-advanced.md",
}

CHAPTER_RE = re.compile(r"^# 第(\d+)章")
TAIL_RE = re.compile(r"^## (本章小结|习题|上机题)\s*$")
REF_RE = re.compile(r"^# 参考文献")


def clean(text: str) -> str:
    text = text.replace("；", ";").replace("：", ":")
    text = re.sub(r"\+\s+\+", "++", text)
    text = re.sub(r"-\s+-", "--", text)
    text = text.replace("i + +", "i++").replace("j + +", "j++")
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip() + "\n"


def split_chapters(lines: list[str]) -> dict[int, list[str]]:
    chapters: dict[int, list[str]] = {}
    current = 0
    buf: list[str] = []
    for line in lines:
        heading = CHAPTER_RE.match(line)
        if heading:
            if current:
                chapters[current] = buf
            current = int(heading.group(1))
            buf = []
            continue
        if REF_RE.match(line):
            if current:
                chapters[current] = buf
            break
        buf.append(line)
    else:
        if current:
            chapters[current] = buf
    return chapters


def extract_tail(lines: list[str]) -> str | None:
    start = None
    for i, line in enumerate(lines):
        if line.strip() == "## 本章小结":
            start = i
            break
    if start is None:
        return None
    return clean("".join(lines[start:]))


def main() -> None:
    lines = RAW.read_text(encoding="utf-8").splitlines(keepends=True)
    chapters = split_chapters(lines)
    for number, path in CHAPTER_FILES.items():
        body = path.read_text(encoding="utf-8")
        if re.search(r"^## 本章小结\s*$", body, re.M):
            print(f"skip ch{number:02d}: already has 本章小结")
            continue
        tail = extract_tail(chapters.get(number, []))
        if not tail:
            print(f"skip ch{number:02d}: no tail in raw")
            continue
        path.write_text(body.rstrip() + "\n\n" + tail, encoding="utf-8")
        print(f"append ch{number:02d}: {len(tail.splitlines())} lines")


if __name__ == "__main__":
    main()
