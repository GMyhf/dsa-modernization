#!/usr/bin/env python3
"""vendor_figures.py — 把书稿里热链在上游的插图拉到本地。

`dsa_raw.md` 里 292 张图全是 `raw.githubusercontent.com/GMyhf/img/...` 的外链，
没有本地副本、没有 alt 文本。外链会烂，仓库要能离线自证，所以现代化的书稿
（`book/`）只许引本地文件——这条由 `tools/check_doc.py` 的 R4 把守。

本脚本只搬字节，不写意思：下载 → 按内容 sha256 命名去重 → 就地改写链接。
**alt 文本仍然是空的，R4 会继续报红**，直到有人真的看图写出图注。
这是故意的：工具能保证图还在，不能保证图讲了什么。

用法:
  python3 tools/vendor_figures.py book/ch03-stack.md --dry-run
  python3 tools/vendor_figures.py book/ch03-stack.md
"""
import argparse
import hashlib
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "book" / "assets"
IMAGE_RE = re.compile(r"!\[(?P<alt>[^\]]*)\]\((?P<src>https?://[^)\s]+)\)")
TIMEOUT = 30


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "dsa-modernization/vendor_figures"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:  # noqa: S310 固定 http(s)
        return resp.read()


def main():
    parser = argparse.ArgumentParser(description="把书稿里的远端插图落到 book/assets/")
    parser.add_argument("paths", nargs="+", help="markdown 文件")
    parser.add_argument("--dry-run", action="store_true")
    opts = parser.parse_args()

    ASSETS.mkdir(parents=True, exist_ok=True)
    total_new = total_seen = 0
    for raw_path in opts.paths:
        path = Path(raw_path) if Path(raw_path).is_absolute() else ROOT / raw_path
        if not path.is_file():
            print(f"❌ 没有这个文件: {raw_path}")
            sys.exit(2)
        text = path.read_text(encoding="utf-8")
        matches = list(IMAGE_RE.finditer(text))
        if not matches:
            print(f"· {path.relative_to(ROOT)}: 没有远端图片")
            continue

        replacements = {}
        for m in matches:
            url = m.group("src")
            total_seen += 1
            if url in replacements:
                continue
            if opts.dry_run:
                print(f"  would fetch {url}")
                continue
            try:
                blob = fetch(url)
            except Exception as exc:  # 网络问题要说清楚是哪张图
                print(f"❌ 下载失败 {url}: {exc}")
                sys.exit(1)
            suffix = Path(url).suffix.lower() or ".jpg"
            name = hashlib.sha256(blob).hexdigest()[:16] + suffix
            target = ASSETS / name
            if not target.exists():
                target.write_bytes(blob)
                total_new += 1
            replacements[url] = f"assets/{name}"

        if opts.dry_run:
            continue
        new_text = IMAGE_RE.sub(
            lambda m: f"![{m.group('alt')}]({replacements.get(m.group('src'), m.group('src'))})",
            text,
        )
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            print(f"✅ {path.relative_to(ROOT)}: {len(replacements)} 个链接已改为本地路径")

    if not opts.dry_run:
        print(f"共处理 {total_seen} 处引用，新落盘 {total_new} 个文件到 book/assets/")
        print("⚠️  alt 文本没动。看图写图注是人的活，check_doc R4 会一直红着提醒你。")


if __name__ == "__main__":
    main()
