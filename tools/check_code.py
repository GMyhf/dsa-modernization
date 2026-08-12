#!/usr/bin/env python3
"""check_code.py — 把 code/ 下每个单元真正编译、真正跑一遍。

原书的 C++ 是 2008 年的教材写法：裸 `new[]`、没有析构、`bool pop(T& item)` 用出参、
`int` 当下标、没有 const 正确性、更没有测试。现代化之后如果只是「看起来更现代」，
那就什么都没证明。这个脚本是本项目最硬的仲裁：

  1. 严格编译：-Wall -Wextra -Wpedantic -Werror（教材代码最爱的隐式转换、
     未使用参数、有符号/无符号比较，在这里全部是错误）
  2. Debug 构建带 ASan + UBSan，-fno-sanitize-recover=all：越界和 UB 当场崩
  3. Release 构建 -O2 再跑一遍：只在某个优化档下成立的测试不算数
  4. test.cpp 自带断言，退出码非 0 即失败

用法:
  python3 tools/check_code.py                    # 全部单元
  python3 tools/check_code.py code/ch03/array_stack
  python3 tools/check_code.py --keep             # 保留 .build/ 便于手工复现
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

CODE = ROOT / "code"

BASE_FLAGS = ["-Wall", "-Wextra", "-Wpedantic", "-Werror"]
PROFILES = [
    (
        "debug+asan+ubsan",
        ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
    ),
    ("release-O2", ["-O2"]),
]
TIMEOUT_SEC = 120


def compiler():
    return shutil.which("g++") or shutil.which("clang++")


# D-001（collab/DECISION_LOG.md，人已拍板）里能被机器守住的两条：
#   第 3 条：数据结构类内部严禁 I/O；
#   第 2 条：不得用 STL 容器直接替代该章节要讲的手写实现。
# 只查 modern.*（实现），不查 test.cpp——测试里用 vector/iostream 是正当的。
D001_FORBIDDEN = [
    (re.compile(r"#\s*include\s*<(iostream|cstdio)>"), "D-001§3 实现文件不得引入 I/O 头文件"),
    (re.compile(r"\bstd::(cout|cerr|clog|printf)\b"), "D-001§3 数据结构类内部严禁 I/O"),
    (
        re.compile(r"#\s*include\s*<(vector|stack|queue|deque|list|forward_list|map|set|"
                   r"unordered_map|unordered_set)>"),
        "D-001§2 不得用 STL 容器替代本章要讲的手写实现",
    ),
]


def strip_comments_and_strings(source: str) -> str:
    """用空白替换注释与字符串，保留换行与预处理指令的行号。"""
    out, i, state = [], 0, "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "code" and ch == "/" and nxt == "/":
            state = "line-comment"
            out.extend("  ")
            i += 2
        elif state == "code" and ch == "/" and nxt == "*":
            state = "block-comment"
            out.extend("  ")
            i += 2
        elif state == "code" and ch == '"':
            state = "string"
            out.append(" ")
            i += 1
        elif state == "code" and ch == "'":
            state = "char"
            out.append(" ")
            i += 1
        elif state == "line-comment":
            out.append("\n" if ch == "\n" else " ")
            state = "code" if ch == "\n" else state
            i += 1
        elif state == "block-comment" and ch == "*" and nxt == "/":
            state = "code"
            out.extend("  ")
            i += 2
        elif state == "block-comment":
            out.append("\n" if ch == "\n" else " ")
            i += 1
        elif state in {"string", "char"} and ch == "\\":
            out.extend("  " if nxt else " ")
            i += 2 if nxt else 1
        elif state in {"string", "char"} and ((state == "string" and ch == '"') or (state == "char" and ch == "'")):
            state = "code"
            out.append(" ")
            i += 1
        elif state in {"string", "char"}:
            out.append("\n" if ch == "\n" else " ")
            i += 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def check_d001(unit_dir: Path, meta):
    """返回违反 D-001 的问题列表。unit.json 的 d001_exceptions 可豁免，但必须写理由。"""
    exceptions = {
        re.sub(r"\s+", "", token): reason.strip()
        for token, reason in meta.get("d001_exceptions", {}).items()
        if isinstance(reason, str) and reason.strip()
    }
    problems = []
    for name in ("modern.hpp", "modern.cpp"):
        path = unit_dir / name
        if not path.is_file():
            continue
        source = strip_comments_and_strings(path.read_text(encoding="utf-8"))
        for lineno, code in enumerate(source.splitlines(), 1):
            # C++ 允许 `std :: cout` 和 `#  include <vector>`；静态闸门不能因为
            # 空白不同就漏掉同一条违规。去掉空白只用于本轮 D-001 token 匹配。
            normalized = re.sub(r"\s+", "", code)
            for pattern, desc in D001_FORBIDDEN:
                m = pattern.search(normalized)
                if not m:
                    continue
                token = m.group(0).strip()
                reason = exceptions.get(token) or exceptions.get(m.group(1) if m.groups() else "")
                if reason:
                    continue  # 有豁免且写了理由
                problems.append(f"  ❌ {name}:{lineno} {desc}: `{token}`")
    return problems


def discover(paths):
    if paths:
        dirs = []
        for p in paths:
            d = Path(p) if Path(p).is_absolute() else ROOT / p
            if not (d / "unit.json").is_file():
                print(f"❌ {p} 不是一个单元（缺 unit.json）")
                sys.exit(2)
            dirs.append(d)
        return dirs
    return sorted(p.parent for p in CODE.rglob("unit.json")) if CODE.is_dir() else []


def build_and_run(unit_dir: Path, workdir: Path, keep=False):
    """返回 (ok, 输出片段列表)。"""
    rel = rel_label(unit_dir)
    meta = json.loads((unit_dir / "unit.json").read_text(encoding="utf-8"))
    std = meta.get("standard", "c++17")  # D-001
    extra = meta.get("flags", [])
    sources = [str(unit_dir / "test.cpp")]
    # modern.cpp 需要一起编译；modern.hpp 由 test.cpp #include
    if (unit_dir / "modern.cpp").is_file():
        sources.append(str(unit_dir / "modern.cpp"))
    sources += [str(unit_dir / s) for s in meta.get("extra_sources", [])]

    logs, ok = [], True
    d001 = check_d001(unit_dir, meta)
    if d001:
        ok = False
        logs.extend(d001)
    for name, flags in PROFILES:
        binary = workdir / f"{unit_dir.name}-{name}"
        cmd = [
            compiler(),
            f"-std={std}",
            *BASE_FLAGS,
            *flags,
            *extra,
            f"-I{unit_dir}",
            *sources,
            "-o",
            str(binary),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT_SEC)
        if proc.returncode != 0:
            ok = False
            logs.append(f"  ❌ [{name}] 编译失败\n{indent(proc.stdout + proc.stderr)}")
            continue
        run = subprocess.run(
            [str(binary)], capture_output=True, text=True, cwd=unit_dir, timeout=TIMEOUT_SEC
        )
        if run.returncode != 0:
            ok = False
            logs.append(
                f"  ❌ [{name}] 测试失败（退出码 {run.returncode}）\n{indent(run.stdout + run.stderr)}"
            )
        else:
            tail = run.stdout.strip().splitlines()[-1:] or ["(无输出)"]
            logs.append(f"  ✅ [{name}] {tail[0][:80]}")
    if keep:
        logs.append(f"  产物保留在 {workdir}")
    return ok, [f"{rel}  «{meta.get('title', '')}»", *logs]


def indent(text, prefix="     ", head=12, tail=28):
    """失败输出掐头去尾。

    掐中间而不是只留开头：被测代码如果在循环里打日志，几百行噪声会把
    最后那句 `FAIL: ...` 顶出视野——变异自检时就踩过这个坑。
    """
    lines = text.strip().splitlines()
    if len(lines) > head + tail:
        lines = lines[:head] + [f"... 中间省略 {len(lines) - head - tail} 行 ..."] + lines[-tail:]
    body = "\n".join(prefix + line for line in lines)
    return body or prefix + "(无输出)"


def main():
    parser = argparse.ArgumentParser(description="编译并运行 code/ 下的现代化单元")
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--keep", action="store_true", help="保留编译产物")
    opts = parser.parse_args()

    if not compiler():
        print("❌ 找不到 g++ 或 clang++")
        sys.exit(2)

    units = discover(opts.paths)
    if not units:
        print("⚠️  code/ 下还没有单元，跳过（脚手架已就位，等第一个清单现代化）")
        return

    workdir = Path(ROOT / ".build") if opts.keep else Path(tempfile.mkdtemp(prefix="dsa-check-"))
    workdir.mkdir(parents=True, exist_ok=True)
    failed, blocks = [], []
    for unit in units:
        try:
            ok, log = build_and_run(unit, workdir, opts.keep)
        except subprocess.TimeoutExpired:
            ok, log = False, [rel_label(unit), f"  ❌ 超过 {TIMEOUT_SEC}s 未结束"]
        blocks.append("\n".join(log))
        if not ok:
            failed.append(rel_label(unit))
    if not opts.keep:
        shutil.rmtree(workdir, ignore_errors=True)

    print("\n".join(blocks))
    print(
        f"\n{'❌' if failed else '✅'} {len(units) - len(failed)}/{len(units)} 个单元通过"
        f"（每个 {len(PROFILES)} 种构建：{', '.join(n for n, _ in PROFILES)}）"
    )
    if failed:
        print("失败: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
