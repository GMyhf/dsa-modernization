#!/usr/bin/env python3
"""sync_book.py — 用 code/ 里的真实源码填充书稿里的代码块。

`check_doc.py` 的 R3 只负责**验**：书稿里的 cpp / python 块必须和 `file=` 指向的源码逐字一致。
本脚本负责**写**：把源码灌进去。于是「改了代码忘了改书」这件事从「靠自觉」
变成了「跑一条命令」。

书稿里这样写（块体可以是空的）：

    ```cpp file=code/ch03/array_stack/modern.hpp#push
    ```

`--write` 之后块体会被替换成 `modern.hpp` 里 `// >>> push` … `// <<< push` 之间的内容。

用法:
  python3 tools/sync_book.py            # 只报告哪些块会变（不落盘）
  python3 tools/sync_book.py --write
"""
import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOOK = ROOT / "book"
sys.path.insert(0, str(ROOT / "tools"))

from check_doc import iter_blocks, normalize, parse_info, read_slice, rel_label  # noqa: E402


def sync_file(path: Path, write=False):
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    edits = []  # (start_idx, end_idx, new_body_lines)
    for block in iter_blocks(text):
        lang, ref, anchor = parse_info(block["info"])
        if lang not in ("cpp", "python") or not ref:  # D-025：python 块同样由本脚本写
            continue
        target = ROOT / ref
        if not target.is_file():
            print(f"❌ {rel_label(path)}:{block['start']}  file={ref} 不存在")
            return False, 0
        if anchor:
            content, err = read_slice(target, anchor)
            if err:
                print(f"❌ {rel_label(path)}:{block['start']}  {err}")
                return False, 0
        else:
            content = target.read_text(encoding="utf-8")
        wanted = normalize(content).split("\n")
        if wanted != [line.rstrip() for line in block["body"]]:
            # body 在 lines 里的下标区间：start 是围栏行的 1-based 行号
            edits.append((block["start"], block["end"] - 1, wanted, ref, anchor))

    if not edits:
        return True, 0
    for start, _, _, ref, anchor in edits:
        mark = f"{ref}#{anchor}" if anchor else ref
        print(f"{'✏️ ' if write else '· '}{rel_label(path)}:{start}  ← {mark}")
    if write:
        for start, end, wanted, _, _ in reversed(edits):
            lines[start:end] = wanted
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return True, len(edits)


def main():
    parser = argparse.ArgumentParser(description="把 code/ 的源码同步进书稿代码块")
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--write", action="store_true", help="真的改文件；不加则只报告")
    opts = parser.parse_args()

    targets = (
        [Path(p) if Path(p).is_absolute() else ROOT / p for p in opts.paths]
        if opts.paths
        else (sorted(BOOK.rglob("*.md")) if BOOK.is_dir() else [])
    )
    if not targets:
        print("⚠️  book/ 下还没有书稿")
        return

    total, ok = 0, True
    for target in targets:
        good, count = sync_file(target, opts.write)
        ok = ok and good
        total += count
    if not ok:
        sys.exit(1)
    if total == 0:
        print(f"✅ {len(targets)} 个文件，代码块与 code/ 一致")
    elif opts.write:
        print(f"✅ 已同步 {total} 个代码块")
    else:
        print(f"⚠️  {total} 个代码块与源码不一致，跑 `--write` 同步（或先确认代码改对了）")
        sys.exit(1)


if __name__ == "__main__":
    main()
