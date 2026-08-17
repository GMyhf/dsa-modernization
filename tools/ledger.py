#!/usr/bin/env python3
"""ledger.py — 清单台账：《数据结构与算法》原书 105 条清单，现代化到哪一步了。

状态是**算出来的**，不是手写的：

    inventory  ← 从 dsa_raw.md 解析 【算法X.Y】/【代码X.Y】
    covered    ← code/**/unit.json 的 listings 字段并集
    excluded   ← collab/exclusions.json（必须写理由、署名、日期）
    pending    ← inventory − covered − excluded

手写的进度表会腐烂，算出来的不会。两个 agent 谁都不能靠「我记得做过了」交差：
清单要么有一个能编译能跑的 code/ 单元认领，要么在 exclusions.json 里带理由退场，
没有第三种状态。

用法:
  python3 tools/ledger.py              # 打印覆盖率报告
  python3 tools/ledger.py --check      # 只做一致性校验，有问题退出码 1（交接闸门用）
  python3 tools/ledger.py --pending    # 只列还没人认领的清单
  python3 tools/ledger.py --json       # 机器可读
"""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

RAW = ROOT / "dsa_raw.md"
CODE = ROOT / "code"
EXCLUSIONS = ROOT / "collab" / "exclusions.json"

# 【算法3.3】 / 【代码 3.1】 —— OCR 里空格位置不稳定，所以到处 \s*
OPEN_RE = re.compile(r"【\s*(算法|代码)\s*([0-9]+\.[0-9]+[a-zA-Z]?)\s*】")
# 结束标记经常被 OCR 吃掉前缀（见 dsa_raw.md 的「法3.3结束】」），所以只认数字
END_RE = re.compile(r"([0-9]+\.[0-9]+[a-zA-Z]?)\s*结束")

REQUIRED_UNIT_FILES = ("unit.json", "legacy.md", "test.cpp")
# D-001 定的默认是 c++17；偏离要在 legacy.md 写明理由
DEFAULT_STANDARD = "c++17"
KNOWN_STANDARDS = ("c++17", "c++20", "c++23")


def parse_inventory(raw_path=RAW):
    """返回 [{'id','kind','number','chapter','line','has_end'}]，按出现顺序。"""
    if not raw_path.is_file():
        return []
    text = raw_path.read_text(encoding="utf-8")
    ends = set(END_RE.findall(text))
    # 行号：给人定位用，值得多扫一遍
    line_of = {}
    for lineno, line in enumerate(text.splitlines(), 1):
        for kind, number in OPEN_RE.findall(line):
            line_of.setdefault(f"{kind}{number}", lineno)
    items = []
    for kind, number in OPEN_RE.findall(text):
        items.append(
            {
                "id": f"{kind}{number}",
                "kind": kind,
                "number": number,
                "chapter": int(number.split(".")[0]),
                "line": line_of.get(f"{kind}{number}", 0),
                "has_end": number in ends,
            }
        )
    return items


def load_units(code_root=CODE):
    """读 code/**/unit.json。返回 (units, problems)。"""
    units, problems = [], []
    if not code_root.is_dir():
        return units, problems
    for manifest in sorted(code_root.rglob("unit.json")):
        rel = rel_label(manifest.parent)
        try:
            data = json.loads(manifest.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            problems.append(f"{rel}/unit.json 不是合法 JSON: {exc}")
            continue
        data["path"] = manifest.parent
        data["rel"] = rel
        if data.get("id") != manifest.parent.name:
            problems.append(
                f"{rel}: unit.json 的 id={data.get('id')!r} 与目录名 {manifest.parent.name!r} 不一致"
            )
        listings = data.get("listings")
        beyond = data.get("beyond_book")
        if not isinstance(listings, list):
            problems.append(f"{rel}: unit.json 缺少 listings 字段")
            data["listings"] = []
        elif not listings and not (isinstance(beyond, str) and beyond.strip()):
            # 有些内容原书根本没给清单（第11章一条都没有，12.3 的 Trie/Patricia 只有
            # 文字和图）。这类新增实现也要能入库，但必须自报家门：否则「忘了填 listings」
            # 和「本来就没有清单可认领」在台账里长得一模一样。
            problems.append(
                f"{rel}: listings 为空时必须写 beyond_book，说明原书没有对应清单"
            )
        elif listings:
            for entry in listings:
                if not isinstance(entry, dict):
                    problems.append(
                        f"{rel}: listings 条目 {entry!r} 必须使用 {{id, anchor, test}} 对象"
                    )
                    continue
                if not all(isinstance(entry.get(key), str) and entry[key].strip()
                           for key in ("id", "anchor", "test")):
                    problems.append(f"{rel}: listings 对象缺少非空的 id/anchor/test：{entry!r}")
        std = data.get("standard", DEFAULT_STANDARD)
        if std not in KNOWN_STANDARDS:
            problems.append(f"{rel}: standard={std!r} 不在 {KNOWN_STANDARDS}")
        for name in REQUIRED_UNIT_FILES:
            if not (manifest.parent / name).is_file():
                problems.append(f"{rel}: 缺少 {name}")
        # 实现文件可以叫 modern.hpp 或 modern.cpp，但必须有一个
        if not any((manifest.parent / f"modern.{ext}").is_file() for ext in ("hpp", "cpp")):
            problems.append(f"{rel}: 缺少 modern.hpp 或 modern.cpp")
        units.append(data)
    return units, problems


def load_exclusions(path=EXCLUSIONS):
    """读 collab/exclusions.json。返回 (dict[listing_id] -> entry, problems)。"""
    entries, problems = {}, []
    if not path.is_file():
        return entries, problems
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return entries, [f"collab/exclusions.json 不是合法 JSON: {exc}"]
    for entry in raw.get("exclusions", []):
        listing = entry.get("listing")
        if not listing:
            problems.append("exclusions.json: 有一条记录没写 listing")
            continue
        for field in ("reason", "by", "date"):
            if not entry.get(field):
                problems.append(f"exclusions.json[{listing}]: 缺少 {field}——退场必须留下出处")
        if listing in entries:
            problems.append(f"exclusions.json[{listing}]: 重复记录")
        entries[listing] = entry
    return entries, problems


def analyze():
    inventory = parse_inventory()
    known = {item["id"] for item in inventory}
    units, problems = load_units()
    exclusions, ex_problems = load_exclusions()
    problems += ex_problems

    claimed = {}
    for unit in units:
        for entry in unit.get("listings", []):
            listing = entry.get("id") if isinstance(entry, dict) else entry
            if not isinstance(listing, str):
                problems.append(f"{unit['rel']}: listings 条目必须是编号或对象")
                continue
            if listing not in known:
                problems.append(
                    f"{unit['rel']}: 认领的 {listing} 在 dsa_raw.md 里不存在（编号写错了？）"
                )
                continue
            if listing in claimed:
                problems.append(
                    f"{listing} 被两个单元同时认领: {claimed[listing]} 与 {unit['rel']}"
                )
                continue
            claimed[listing] = unit["rel"]

    for listing in exclusions:
        if listing not in known:
            problems.append(f"exclusions.json[{listing}]: 在 dsa_raw.md 里不存在")
        if listing in claimed:
            problems.append(
                f"{listing} 既被 {claimed[listing]} 认领又被列入 exclusions——只能二选一"
            )

    pending = [i["id"] for i in inventory if i["id"] not in claimed and i["id"] not in exclusions]
    return {
        "inventory": inventory,
        "units": units,
        "claimed": claimed,
        "exclusions": exclusions,
        "pending": pending,
        "problems": problems,
    }


def format_report(state):
    inv = state["inventory"]
    total = len(inv)
    done = len(state["claimed"])
    dropped = len(state["exclusions"])
    lines = [
        "清单台账 · dsa_raw.md → code/",
        "",
        f"总计 {total} 条（算法 {sum(1 for i in inv if i['kind'] == '算法')} / "
        f"代码 {sum(1 for i in inv if i['kind'] == '代码')}）"
        f" | 已现代化 {done} | 退场 {dropped} | 待办 {len(state['pending'])}",
        "",
        "| 章 | 清单数 | 已现代化 | 退场 | 待办 |",
        "| --- | --- | --- | --- | --- |",
    ]
    for chapter in sorted({i["chapter"] for i in inv}):
        ids = [i["id"] for i in inv if i["chapter"] == chapter]
        c = sum(1 for i in ids if i in state["claimed"])
        e = sum(1 for i in ids if i in state["exclusions"])
        lines.append(f"| 第{chapter}章 | {len(ids)} | {c} | {e} | {len(ids) - c - e} |")
    beyond = [u for u in state["units"] if not u.get("listings")]
    lines += ["", f"code/ 单元 {len(state['units'])} 个（其中 {len(beyond)} 个不对应原书清单）:"]
    for unit in state["units"]:
        claims = ", ".join(
            e.get("id", "?") if isinstance(e, dict) else e
            for e in unit.get("listings", [])
        ) or "原书无对应清单"
        lines.append(f"  - {unit['rel']}  ←  {claims}  [{unit.get('title', '')}]")
    broken = [i["id"] for i in inv if not i["has_end"]]
    if broken:
        lines += [
            "",
            f"原书里 {len(broken)} 条清单的「结束」标记被 OCR 吃掉，切片时要人工定边界:",
            "  " + ", ".join(broken),
        ]
    if state["problems"]:
        lines += ["", "❌ 一致性问题:"] + [f"  - {p}" for p in state["problems"]]
    else:
        lines += ["", "✅ 台账一致"]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="清单台账与覆盖率")
    parser.add_argument("--check", action="store_true", help="只校验一致性，有问题退出码 1")
    parser.add_argument("--pending", action="store_true", help="只列待办清单")
    parser.add_argument("--json", action="store_true", help="机器可读输出")
    opts = parser.parse_args()

    state = analyze()
    if opts.json:
        print(
            json.dumps(
                {
                    "total": len(state["inventory"]),
                    "claimed": state["claimed"],
                    "excluded": sorted(state["exclusions"]),
                    "pending": state["pending"],
                    "problems": state["problems"],
                },
                ensure_ascii=False,
                indent=2,
            )
        )
    elif opts.pending:
        print("\n".join(state["pending"]) if state["pending"] else "(无待办)")
    elif opts.check:
        if state["problems"]:
            print("\n".join(f"❌ {p}" for p in state["problems"]))
        else:
            print(
                f"✅ 台账一致：{len(state['claimed'])}/{len(state['inventory'])} 已现代化，"
                f"{len(state['exclusions'])} 退场，{len(state['pending'])} 待办"
            )
    else:
        print(format_report(state))
    sys.exit(1 if state["problems"] else 0)


if __name__ == "__main__":
    main()
