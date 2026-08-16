#!/usr/bin/env python3
"""extract_slides.py — 把课件（.pptx）抽成可 grep 的纯文本。

**为什么要有这个脚本**：`ref_数据结构与算法A 2021秋/` 里的课件是同一批作者的教学
材料，它的教学骨架就是原书的骨架，所以拿它逐章对照能查出「新书悄悄少讲了什么」。
2026-08-16 的那一轮就靠它查出 6 条（含一条台账造假），详见 `参考资料说明.md`。
但二进制课件没法 grep、没法 diff、没法在交接包里引用，所以把文字抽出来入库。

**零第三方依赖**（红线第 8 条）：`.pptx` 就是一个 zip，里面是 XML，
用 `zipfile` + `xml.etree` 就够了，不需要 python-pptx。

**`.ppt`（2003 及以前的二进制格式）本脚本不认**，要先转一道：

    libreoffice --headless --convert-to pptx --outdir <目录> *.ppt

LibreOffice 是外部程序不是 Python 依赖；转出来的 `.pptx` 是中间产物，不入库，
入库的只有本脚本产出的 `.txt`。

用法:
  python3 tools/extract_slides.py <文件或目录> [-o 输出目录]
  python3 tools/extract_slides.py 课件.pptx --stdout
"""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import rel_label  # noqa: E402

# DrawingML：`<a:p>` 是一个段落，`<a:t>` 是段落里的一段文字。
# 表格的单元格 `<a:tc>` 里装的也是 `<a:p>`，所以按文档序遍历就能连表格一起拿到。
DRAWING_NS = "{http://schemas.openxmlformats.org/drawingml/2006/main}"
PARAGRAPH = DRAWING_NS + "p"
TEXT = DRAWING_NS + "t"

SLIDE_RE = re.compile(r"^ppt/slides/slide(\d+)\.xml$")
NOTES_RE = re.compile(r"^ppt/notesSlides/notesSlide(\d+)\.xml$")
# 母版上的「幻灯片编号」占位符抽出来是一段没被替换的**域代码**（`<number>` 之类），
# 正文和备注里都有（本批课件正文 1194 处、备注 263 处）。它不是内容，一律丢。
FIELD_PLACEHOLDER_RE = re.compile(r"^(?:<number>|‹#›|<#>|<日期/时间>|<页脚>)$")
# 备注里另有一个渲染成纯数字的页码占位符。**只在备注里丢裸数字**——
# 正文里的孤立数字可能是图表的一格（比如图1.4 那些索引值），动不得。
BARE_NUMBER_RE = re.compile(r"^\d{1,3}$")


def paragraphs(xml_bytes):
    """按文档序取出每个段落的文字。空段落丢掉。"""
    root = ET.fromstring(xml_bytes)
    out = []
    for node in root.iter(PARAGRAPH):
        text = "".join(run.text or "" for run in node.iter(TEXT))
        text = re.sub(r"\s+", " ", text).strip()
        if text and not FIELD_PLACEHOLDER_RE.match(text):
            out.append(text)
    return out


def notes_for_slide(archive, slide_name):
    """顺着关系文件找这一页的备注页。

    **不能按编号硬配**：`slide7.xml` 的备注不一定是 `notesSlide7.xml`——
    只有带备注的页才会生成备注页，编号是各自连续的。必须读
    `ppt/slides/_rels/slide7.xml.rels` 里那条指向 notesSlide 的关系。
    """
    rels = f"ppt/slides/_rels/{Path(slide_name).name}.rels"
    if rels not in archive.namelist():
        return []
    try:
        root = ET.fromstring(archive.read(rels))
    except ET.ParseError:
        return []
    for rel in root:
        target = rel.get("Target", "")
        if "notesSlide" in target:
            resolved = "ppt/" + target.replace("../", "")
            if resolved in archive.namelist():
                lines = paragraphs(archive.read(resolved))
                return [ln for ln in lines if not BARE_NUMBER_RE.match(ln)]
    return []


def extract(path: Path):
    """返回整份课件的纯文本。"""
    with zipfile.ZipFile(path) as archive:
        slides = sorted(
            (name for name in archive.namelist() if SLIDE_RE.match(name)),
            key=lambda n: int(SLIDE_RE.match(n).group(1)),
        )
        out = [
            f"# {path.name}",
            "",
            "> 由 tools/extract_slides.py 从课件抽出的纯文本，仅含文字与演讲者备注；",
            "> 图片、公式排版、动画一概丢失。**原始课件才是依据**，本文件只为便于检索与对照。",
            "",
            f"共 {len(slides)} 页。",
            "",
        ]
        for index, name in enumerate(slides, 1):
            body = paragraphs(archive.read(name))
            notes = notes_for_slide(archive, name)
            if not body and not notes:
                continue
            out.append(f"===== 幻灯片 {index} =====")
            out.extend(body)
            if notes:
                out.append("--- 演讲者备注 ---")
                out.extend(notes)
            out.append("")
        return "\n".join(out).rstrip() + "\n"


def main():
    parser = argparse.ArgumentParser(description="把 .pptx 课件抽成纯文本")
    parser.add_argument("paths", nargs="+", help=".pptx 文件或含 .pptx 的目录")
    parser.add_argument("-o", "--outdir", help="输出目录（默认与源文件同目录）")
    parser.add_argument("--stdout", action="store_true", help="打到标准输出，不落盘")
    opts = parser.parse_args()

    targets = []
    for raw in opts.paths:
        path = Path(raw)
        if path.is_dir():
            targets.extend(sorted(path.glob("*.pptx")))
        elif path.suffix.lower() == ".pptx":
            targets.append(path)
        else:
            print(f"❌ 跳过 {raw}：不是 .pptx（.ppt 请先用 libreoffice 转一道）")
    if not targets:
        print("⚠️  没有找到任何 .pptx")
        return 1

    failed = []
    for src in targets:
        try:
            text = extract(src)
        except (zipfile.BadZipFile, ET.ParseError, KeyError) as exc:
            failed.append((src, exc))
            print(f"❌ {rel_label(src)}: {exc}")
            continue
        if opts.stdout:
            print(text)
            continue
        outdir = Path(opts.outdir) if opts.outdir else src.parent
        outdir.mkdir(parents=True, exist_ok=True)
        dest = outdir / (src.stem + ".txt")
        dest.write_text(text, encoding="utf-8")
        pages = text.count("===== 幻灯片 ")
        notes = text.count("--- 演讲者备注 ---")
        print(f"✅ {rel_label(dest)}  {pages} 页" + (f"，{notes} 页有备注" if notes else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
