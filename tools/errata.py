#!/usr/bin/env python3
"""勘误台账：每一条勘误必须指得出它的证据在哪。

和 `ledger.py` 一样是**派生**的，不是手抄的：

- `kind=runtime` / `memory`：清单编号必须出现在某个 `code/**/test.cpp` 的断言文字里。
  也就是说「这条勘误有回归测试」不是写在表格里的一句话，而是 grep 得到的事实。
  实现要是退回原书那种写法，那条断言就会变红。
- `kind=compile`：原书按印刷根本进不了编译器，写不成运行期断言。证据是对应
  `legacy.md` 里真实的 `error:` 输出，所以校验那个文件存在且确实含编译器报错。
- `kind=prose` / `na`：文字、公式、图示，或本书没有保留原文的段落。这类必须写
  `reason` / `by` / `date`，和 `exclusions.json` 一个规矩——**退场要留下出处**。

这样「勘误覆盖率」就不会随着表格年久失修而失真：漏了测试就报红，
而不是让一句「已吸收」永远挂在那里。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402

ERRATA = ROOT / "collab" / "errata.json"
CODE = ROOT / "code"

KINDS = ("runtime", "memory", "compile", "prose", "na")
NEEDS_TEST = ("runtime", "memory")
NEEDS_REASON = ("prose", "na")
ID_RE = re.compile(r"^[ER][0-9]{2}$")


def load(path=ERRATA):
    """读 collab/errata.json，返回 (entries, problems)。"""
    if not path.is_file():
        return [], [f"缺少 {rel_label(path)}"]
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [], [f"{rel_label(path)} 不是合法 JSON: {exc}"]

    entries, problems, seen = [], [], set()
    for entry in raw.get("errata", []):
        eid = entry.get("id")
        if not eid or not ID_RE.match(eid):
            problems.append(f"errata.json: id={eid!r} 不合法（应形如 E01 / R07）")
            continue
        if eid in seen:
            problems.append(f"errata.json[{eid}]: 重复条目")
            continue
        seen.add(eid)
        for field in ("source", "listing", "summary", "kind"):
            if not entry.get(field):
                problems.append(f"errata.json[{eid}]: 缺少 {field}")
        kind = entry.get("kind")
        if kind not in KINDS:
            problems.append(f"errata.json[{eid}]: kind={kind!r} 不在 {KINDS}")
        if kind in NEEDS_REASON:
            for field in ("reason", "by", "date"):
                if not entry.get(field):
                    problems.append(
                        f"errata.json[{eid}]: kind={kind} 必须写 {field}——不写测试就要留下出处"
                    )
        if kind == "compile" and not entry.get("evidence"):
            problems.append(f"errata.json[{eid}]: kind=compile 必须给出 evidence（legacy.md 路径）")
        entries.append(entry)
    return entries, problems


def tests_mentioning(code_root=CODE):
    """扫一遍 code/**/test.cpp，返回 {勘误编号: [出现的文件]}。"""
    found = {}
    if not code_root.is_dir():
        return found
    for test in sorted(code_root.rglob("test.cpp")):
        text = test.read_text(encoding="utf-8", errors="replace")
        for eid in set(re.findall(r"勘误([ER][0-9]{2})", text)):
            found.setdefault(eid, []).append(rel_label(test))
    return found


def analyze():
    entries, problems = load()
    mentioned = tests_mentioning()

    for entry in entries:
        eid, kind = entry.get("id"), entry.get("kind")
        if kind in NEEDS_TEST:
            if eid not in mentioned:
                problems.append(
                    f"errata.json[{eid}]: kind={kind} 却没有任何 test.cpp 提到「勘误{eid}」"
                    f"——这条勘误没有回归测试"
                )
            else:
                entry["tests"] = mentioned[eid]
        elif eid in mentioned:
            # 有测试当然更好，但要如实记下来，别让 kind 和事实对不上。
            entry["tests"] = mentioned[eid]

        if kind == "compile":
            evidence = ROOT / entry.get("evidence", "")
            if not evidence.is_file():
                problems.append(f"errata.json[{eid}]: evidence 指向的 {entry.get('evidence')} 不存在")
            elif "error:" not in evidence.read_text(encoding="utf-8", errors="replace"):
                problems.append(
                    f"errata.json[{eid}]: {entry.get('evidence')} 里找不到 `error:`"
                    f"——编译级勘误要有编译器的原话"
                )

    stray = sorted(set(mentioned) - {e.get("id") for e in entries})
    for eid in stray:
        problems.append(
            f"测试里提到了「勘误{eid}」，但 errata.json 里没有这一条（{', '.join(mentioned[eid])}）"
        )
    return {"entries": entries, "mentioned": mentioned, "problems": problems}


def format_report(state):
    entries = state["entries"]
    by_kind = {kind: [e for e in entries if e.get("kind") == kind] for kind in KINDS}
    covered = [e for e in entries if e.get("tests")]
    lines = [
        "勘误台账 · book/勘误.md → code/**/test.cpp",
        "",
        f"总计 {len(entries)} 条"
        f" | 有回归测试 {len(covered)}"
        f" | 编译级（证据在 legacy.md）{len(by_kind['compile'])}"
        f" | 文字/图示 {len(by_kind['prose'])}"
        f" | 本书未保留 {len(by_kind['na'])}",
        "",
        "| 编号 | 清单 | 类别 | 证据 |",
        "| --- | --- | --- | --- |",
    ]
    for entry in entries:
        if entry.get("tests"):
            evidence = "、".join(entry["tests"])
        elif entry.get("kind") == "compile":
            evidence = entry.get("evidence", "")
        else:
            evidence = entry.get("reason", "")[:40]
        lines.append(
            f"| {entry.get('id')} | {entry.get('listing')} | {entry.get('kind')} | {evidence} |"
        )
    if state["problems"]:
        lines += ["", "❌ 一致性问题:"] + [f"  - {p}" for p in state["problems"]]
    else:
        lines += ["", "✅ 勘误台账一致"]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="勘误 → 回归测试的映射")
    parser.add_argument("--check", action="store_true", help="只校验，有问题退出码 1")
    parser.add_argument("--json", action="store_true", help="机器可读输出")
    opts = parser.parse_args()

    state = analyze()
    if opts.json:
        print(json.dumps(state, ensure_ascii=False, indent=2))
    elif opts.check:
        for problem in state["problems"]:
            print(f"❌ {problem}")
        if not state["problems"]:
            covered = sum(1 for e in state["entries"] if e.get("tests"))
            print(f"✅ 勘误台账一致：{len(state['entries'])} 条，{covered} 条有回归测试")
    else:
        print(format_report(state))
    return 1 if state["problems"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
