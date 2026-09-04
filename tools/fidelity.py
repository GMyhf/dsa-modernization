#!/usr/bin/env python3
"""fidelity.py — 正文保全度台账：原书讲了多少，新书还剩多少。

为什么要有这个工具：

已有的闸门守住了「代码能不能跑」（check_code）、「书上印的是不是就是编译过的那份」
（R3）、「原书的节还在不在」（R10/R11）。**没有一条守住「原书的话还在不在」**——
一节可以标题在、代码在、编号在，正文却从原书的两页压成三行。R10 只问「有没有同号的
节」，答案是「有」；人翻开却发现例子、推导、图注、为什么这么设计，全没了。

2026-09-04 第一次量它：新书正文汉字量是原书的 **51%**，第 8、10、11 章不到 30%。
「不够细致」不是感觉，是可以数出来的。

怎么数：

    raw    ← dsa_raw.md 里该节的汉字数（剥掉代码块——代码另有 R3 守着）
    book   ← book/*.md 里同号节的汉字数（同样剥掉代码块）
    ratio  ← book / raw

比值不是越高越好，也不必是 1：新书本来就会删掉 2008 年的过时叙述、合并小节、
换掉整段代码讲解。所以这里**不设统一及格线**，而是设**棘轮**：
`collab/fidelity.json` 记下每节当前的比值，`--check` 只在比值**掉下去**时判红。
往回抽内容要么带着修好的比值，要么在 waivers 里签字说明。

用法:
  python3 tools/fidelity.py                 # 按缺口排序打印保全度报告
  python3 tools/fidelity.py --chapter 8     # 只看某一章
  python3 tools/fidelity.py --check         # 棘轮校验：比基线掉了就红（闸门用）
  python3 tools/fidelity.py --update        # 把基线抬到当前值（只升不降）
  python3 tools/fidelity.py --json          # 机器可读
"""
import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT  # noqa: E402  同目录工具

RAW = ROOT / "dsa_raw.md"
BOOK = ROOT / "book"
STATE = ROOT / "collab" / "fidelity.json"

HEAD_RE = re.compile(r"^(#{1,6})\s+(.*)$")
SECTION_RE = re.compile(r"^(#{1,6})\s+(\d+(?:\.\d+)+)\s*(.*)$")
CHAPTER_RE = re.compile(r"^#\s*第\s*(\d+)\s*章")
# 每章末尾这三节不带编号，因此第一版的 fidelity 完全没量过它们。
# 2026-09-04 补量：小结 17%、习题 23%、上机题 12%——**三块都比正文更空**，
# 而在此之前没有任何闸门在看。键写成「8.本章小结」，与「8.4」并列排序。
TAIL_SECTIONS = ("本章小结", "习题", "上机题")
CJK_RE = re.compile(r"[一-鿿]")
FENCE_RE = re.compile(r"^\s*(```|~~~)")
# 棘轮容差：汉字数会因为一次措辞微调抖动一两个字，不该因此判红。
TOLERANCE = 0.02


def prose_mask(lines):
    """整篇扫一遍，标出每行是不是正文（围栏代码块内与围栏本身都不算）。

    必须整篇扫：按节切完再各自从「不在围栏内」起步，只要某节的起点落在代码块中间，
    这一节的正文与代码就会整体反过来算——第 10.3 节第一次量出 18127 汉字（比原书
    整章还多）就是这么来的。
    """
    mask = []
    in_fence = False
    for line in lines:
        if FENCE_RE.match(line):
            in_fence = not in_fence
            mask.append(False)
            continue
        mask.append(not in_fence)
    return mask


def cjk_count(lines, mask=None):
    if mask is None:
        text = "\n".join(lines)
    else:
        text = "\n".join(line for line, keep in zip(lines, mask) if keep)
    return len(CJK_RE.findall(text))


def section_volumes(lines):
    """把一份 Markdown 按节切开，返回 {'8.4': 汉字数, '8.习题': 汉字数}。

    三条切分规则，都是被真实文稿逼出来的：

    * 带编号的标题（`## 8.4` / `### 8.4.2`）开一个新桶，三级节并进二级节；
    * 二级的 `## 本章小结` / `## 习题` / `## 上机题` 各开一个桶，键是「章号.标题」；
    * 其余不带编号的标题，二级（`## 练习路径`）结束当前节，三级及更深的
      （`### 为什么这一节没有 Python 版`、`#### 教学版：完整实现`）
      是新书在节内加的教学小标题，正文继续算在这一节头上。

    少了第二条，章末三节会被算进最后一个带编号的节（10.3 第一次量出 18127 汉字），
    而且它们自己永远不会被量；少了第三条的后半句，新书自己补的讲解全被漏掉。
    """
    mask = prose_mask(lines)
    volumes = {}
    key = None
    start = 0
    chapter = None

    def flush(end):
        if key is not None:
            volumes[key] = volumes.get(key, 0) + cjk_count(lines[start:end], mask[start:end])

    for i, line in enumerate(lines):
        head = HEAD_RE.match(line)
        if not head or not mask[i]:
            continue
        found = CHAPTER_RE.match(line)
        if found:
            chapter = int(found.group(1))
        numbered = SECTION_RE.match(line)
        title = head.group(2).strip()
        if numbered:
            flush(i)
            key = ".".join(numbered.group(2).split(".")[:2])
            start = i
        elif len(head.group(1)) == 2 and title in TAIL_SECTIONS and chapter is not None:
            flush(i)
            key = f"{chapter}.{title}"
            start = i
        elif len(head.group(1)) <= 2:
            flush(i)
            key = None
    flush(len(lines))
    return volumes


def sort_key(key):
    """「8.4」排在「8.本章小结」前面；章末三节按小结 → 习题 → 上机题。"""
    chapter, _, rest = key.partition(".")
    if rest in TAIL_SECTIONS:
        return (int(chapter), 1, TAIL_SECTIONS.index(rest))
    return (int(chapter), 0, int(rest) if rest.isdigit() else 0)


def collect():
    """返回 [{'section','chapter','raw','book','ratio'}]，按原书顺序。"""
    raw_lines = RAW.read_text(encoding="utf-8").splitlines()
    raw = section_volumes(raw_lines)
    book = {}
    for path in sorted(BOOK.glob("ch*.md")):
        for key, value in section_volumes(path.read_text(encoding="utf-8").splitlines()).items():
            book[key] = book.get(key, 0) + value
    rows = []
    for key in sorted(raw, key=sort_key):
        rc = raw[key]
        bc = book.get(key, 0)
        rows.append({
            "section": key,
            "chapter": int(key.partition(".")[0]),
            "raw": rc,
            "book": bc,
            "ratio": round(bc / rc, 3) if rc else 0.0,
        })
    return rows


def load_state():
    if not STATE.is_file():
        return {"baseline": {}, "waivers": []}
    return json.loads(STATE.read_text(encoding="utf-8"))


def save_state(state):
    STATE.write_text(json.dumps(state, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def waived(state):
    return {w["section"] for w in state.get("waivers", [])}


def pad(text, width):
    """按显示宽度左对齐——章末三节的键含汉字，用 len() 会把整列排歪。"""
    shown = sum(2 if unicodedata.east_asian_width(ch) in "WF" else 1 for ch in text)
    return text + " " * max(0, width - shown)


def report(rows, chapter=None):
    rows = [r for r in rows if chapter is None or r["chapter"] == chapter]
    print(f"{pad('节', 12)}{'原书':>7}{'新书':>7}{'保全':>7}  {'缺口':>6}")
    for r in sorted(rows, key=lambda r: r["book"] - r["raw"]):
        gap = r["raw"] - r["book"]
        mark = "  ⚠️" if r["ratio"] < 0.5 else ""
        print(f"{pad(r['section'], 12)}{r['raw']:>7}{r['book']:>7}{r['ratio']:>7.2f}  {gap:>6}{mark}")
    total_raw = sum(r["raw"] for r in rows)
    total_book = sum(r["book"] for r in rows)
    if total_raw:
        print(f"\n合计：原书 {total_raw} 汉字，新书 {total_book} 汉字，"
              f"保全 {total_book / total_raw:.0%}，缺口 {total_raw - total_book}")


def check(rows, state):
    baseline = state.get("baseline", {})
    skip = waived(state)
    regressions = []
    unknown = []
    for r in rows:
        if r["section"] in skip:
            continue
        if r["section"] not in baseline:
            unknown.append(r["section"])
            continue
        if r["ratio"] < baseline[r["section"]] - TOLERANCE:
            regressions.append((r["section"], baseline[r["section"]], r["ratio"]))
    for section, was, now in regressions:
        print(f"❌ {section} 正文被抽薄：保全度 {was:.2f} → {now:.2f}")
    for section in unknown:
        print(f"❌ {section} 没有基线：跑 tools/fidelity.py --update 登记")
    if regressions or unknown:
        return 1
    total_raw = sum(r["raw"] for r in rows)
    total_book = sum(r["book"] for r in rows)
    thin = sum(1 for r in rows if r["ratio"] < 0.5 and r["section"] not in skip)
    print(f"✅ 正文保全度未回退：{len(rows)} 节，整体 {total_book / total_raw:.0%}，"
          f"其中 {thin} 节仍不足原书一半")
    return 0


def update(rows, state):
    baseline = state.setdefault("baseline", {})
    raised = 0
    for r in rows:
        old = baseline.get(r["section"])
        if old is None or r["ratio"] > old:
            baseline[r["section"]] = r["ratio"]
            raised += 1
    state["baseline"] = {k: baseline[k] for k in sorted(baseline, key=sort_key)}
    save_state(state)
    print(f"✅ 基线已更新：{raised} 节抬高或新登记，共 {len(baseline)} 节")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="正文保全度台账：原书讲了多少，新书还剩多少")
    ap.add_argument("--chapter", type=int, help="只看某一章")
    ap.add_argument("--check", action="store_true", help="棘轮校验，回退即红")
    ap.add_argument("--update", action="store_true", help="把基线抬到当前值")
    ap.add_argument("--json", action="store_true", help="机器可读")
    args = ap.parse_args(argv)

    rows = collect()
    state = load_state()
    if args.json:
        print(json.dumps(rows, ensure_ascii=False, indent=2))
        return 0
    if args.update:
        return update(rows, state)
    if args.check:
        return check(rows, state)
    report(rows, args.chapter)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
