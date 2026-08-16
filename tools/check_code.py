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


# 一个单元可以在结构上完全合规——unit.json 在、legacy.md 在、test.cpp 在、
# 认领的编号也都真实存在——而里面几乎什么都没有。台账会照样报「已现代化」。
# 这个空档是 2026-08-12 第一轮就写进 NOTES 的预言，同年同日被兑现：
# 一次提交用 542 行覆盖 61 条清单，第 8 章 17 条排序清单只有 11 项断言，
# 且 quick() = std::sort、heap() = std::make_heap、radix() 直接调 counting()。
#
# 「实现得对不对」没法机械判定，但「有没有东西」可以。下面两条守的是后者。
MIN_ASSERTIONS_PER_LISTING = 3
MIN_ASSERTIONS_PER_UNIT = 5
# 降级档的黄灯：Release 通过而 sanitizer 未运行时，太薄的测试必须显眼。
MIN_DEGRADED_ASSERTIONS = 10
MIN_LEGACY_LINES = 20
EVIDENCE_MARKERS = ("error:", "runtime error", "Sanitizer", "$ g++", "$ ./")
# 教学版是书稿正文整块印出来的那一份，读者最可能照抄。它比工程版少了移动语义与
# 强异常保证（D-012 的有意取舍），但「少考虑几件事」不等于「少验几条」——
# 教学版自己承诺的东西（LIFO、翻倍、深拷贝、空状态、零 I/O）必须逐条有断言守着。
MIN_TEACHING_ASSERTIONS = 10


def check_substance(unit_dir: Path, meta, assertions, teaching_assertions=None):
    """单元里到底有没有东西。返回 problems 列表。

    判据刻意保守：现有的扎实单元是每条清单 8–18 项断言，
    这里的下限只要求 3，够宽了。真有正当理由低于它，
    那是该写进 DECISION_LOG 的决定，不是又开一个逃生口。
    """
    problems = []
    listings = len(meta.get("listings", []))
    if assertions is not None and listings:
        need = max(MIN_ASSERTIONS_PER_LISTING * listings, MIN_ASSERTIONS_PER_UNIT)
        if assertions < need:
            problems.append(
                f"  ❌ 断言密度不足：{listings} 条清单只有 {assertions} 项断言"
                f"（下限 {need}）。清单被认领却几乎没有被验证。"
            )

    if (unit_dir / "teaching.hpp").is_file() and teaching_assertions is not None:
        if teaching_assertions < MIN_TEACHING_ASSERTIONS:
            problems.append(
                f"  ❌ 教学版只有 {teaching_assertions} 项断言（下限 {MIN_TEACHING_ASSERTIONS}）。"
                "正文整块印出来的那一份，不能比工程版验得松。"
            )

    legacy = unit_dir / "legacy.md"
    if legacy.is_file():
        text = legacy.read_text(encoding="utf-8")
        lines = len([ln for ln in text.splitlines() if ln.strip()])
        if lines < MIN_LEGACY_LINES:
            problems.append(
                f"  ❌ legacy.md 只有 {lines} 行实质内容（下限 {MIN_LEGACY_LINES}）。"
                "红线要求「每条缺陷都要有证据」，两行说明不构成证据。"
            )
        elif not any(marker in text for marker in EVIDENCE_MARKERS):
            problems.append(
                "  ❌ legacy.md 里没有任何可复现的证据"
                f"（找不到 {', '.join(EVIDENCE_MARKERS)} 之一）。"
                "「原书这样写不好」不是证据，编译器与 sanitizer 的输出才是。"
            )
    return problems


def check_d001(unit_dir: Path, meta):
    """返回违反 D-001 的问题列表。unit.json 的 d001_exceptions 可豁免，但必须写理由。"""
    exceptions = {
        re.sub(r"\s+", "", token): reason.strip()
        for token, reason in meta.get("d001_exceptions", {}).items()
        if isinstance(reason, str) and reason.strip()
    }
    problems = []
    # teaching.* 同样是「数据结构实现」，D-001 §2/§3 一视同仁：教学版可以少写
    # noexcept、少写 static_assert（D-012 的豁免只到这里为止），但绝不能在容器里
    # 打 cout，也不能改用 std::vector 把这一节讲的东西删掉。
    for name in ("modern.hpp", "modern.cpp", "teaching.hpp", "teaching.cpp"):
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


SANITIZER_PROFILE = "debug+asan+ubsan"


def sanitizer_preflight(std="c++17", flags=None, workdir=None):
    """先拿一个空程序试 sanitizer 能不能用。返回 (ok, 诊断输出)。

    存在的理由：红队那一轮在 macOS 上撞到 `sanitizer_malloc_mac.inc:189
    (!asan_init_is_running)`——**连空探针都挂**。那种情况下闸门会把每个单元
    都判红，读日志的人只会以为是自己的代码坏了。工具应当自己说清楚
    「是环境不可用」，而不是让人一个个单元去排除。
    """
    flags = flags or dict(PROFILES)[SANITIZER_PROFILE]
    tmp = workdir or tempfile.mkdtemp(prefix="dsa-preflight-")
    Path(tmp).mkdir(parents=True, exist_ok=True)
    src, binary = Path(tmp) / "preflight.cpp", Path(tmp) / "preflight"
    src.write_text("int main() { return 0; }\n", encoding="utf-8")
    try:
        build = subprocess.run(
            [compiler(), f"-std={std}", *flags, str(src), "-o", str(binary)],
            capture_output=True, text=True, timeout=TIMEOUT_SEC,
        )
        if build.returncode != 0:
            return False, "编译空探针即失败：\n" + (build.stdout + build.stderr).strip()
        run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=TIMEOUT_SEC)
        if run.returncode != 0:
            return False, (
                f"空探针运行失败（退出码 {run.returncode}）：\n"
                + (run.stdout + run.stderr).strip()
            )
        return True, "ok"
    except (subprocess.TimeoutExpired, OSError) as exc:
        return False, f"空探针未能完成：{exc}"
    finally:
        if workdir is None:
            shutil.rmtree(tmp, ignore_errors=True)


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


# 一个单元最多有三个可执行产物，每个都在两档 profile 下真编译真运行：
#
#   test       modern.hpp（工程版）的断言测试——一直都有。
#   teaching   teaching.hpp（教学版）的断言测试——D-012 引入。
#   demo       书稿「先跑一遍」印的那段 main——**在 D-012 之前从来没被编译过**，
#              R3 只保证它和文件逐字一致，不保证那个文件能编译。教学版正文
#              最依赖「抄下来就能跑」，这个洞必须堵上。
#
# 每个产物一个独立的可执行文件：三份源码各有自己的 main，不能链进同一个二进制。
def unit_programs(unit_dir: Path, test_sources):
    """返回 ([(kind, sources), ...], problems)。"""
    programs = [("test", test_sources)]
    problems = []

    teaching = unit_dir / "teaching.hpp"
    teaching_test = unit_dir / "teaching_test.cpp"
    if teaching.is_file() and not teaching_test.is_file():
        problems.append(
            "  ❌ 有 teaching.hpp 却没有 teaching_test.cpp：教学版是书稿正文印出来的那份，"
            "不能是唯一没人验的代码（D-012）。"
        )
    elif teaching_test.is_file() and not teaching.is_file():
        problems.append("  ❌ 有 teaching_test.cpp 却没有 teaching.hpp。")
    elif teaching.is_file():
        programs.append(("teaching", [str(teaching_test)]))

    demo = unit_dir / "demo.cpp"
    if demo.is_file():
        programs.append(("demo", [str(demo)]))
    return programs, problems


def build_and_run(unit_dir: Path, workdir: Path, keep=False, profiles=None, degraded=False):
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

    profiles = PROFILES if profiles is None else profiles
    logs, ok = [], True
    d001 = check_d001(unit_dir, meta)
    if d001:
        ok = False
        logs.extend(d001)

    programs, structure = unit_programs(unit_dir, sources)
    if structure:
        ok = False
        logs.extend(structure)

    assertions = None
    teaching_assertions = None
    for name, flags in profiles:
        for kind, srcs in programs:
            suffix = "" if kind == "test" else f"-{kind}"
            binary = workdir / f"{unit_dir.name}{suffix}-{name}"
            label = name if kind == "test" else f"{name}/{kind}"
            cmd = [
                compiler(),
                f"-std={std}",
                *BASE_FLAGS,
                *flags,
                *extra,
                f"-I{unit_dir}",
                f"-I{CODE}",  # 共享的测试探针：#include "support/fault_injection.hpp"
                *srcs,
                "-o",
                str(binary),
            ]
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT_SEC)
            if proc.returncode != 0:
                ok = False
                logs.append(f"  ❌ [{label}] 编译失败\n{indent(proc.stdout + proc.stderr)}")
                continue
            run = subprocess.run(
                [str(binary)], capture_output=True, text=True, cwd=unit_dir, timeout=TIMEOUT_SEC
            )
            if run.returncode != 0:
                ok = False
                what = "demo 运行失败" if kind == "demo" else "测试失败"
                logs.append(
                    f"  ❌ [{label}] {what}（退出码 {run.returncode}）"
                    f"\n{indent(run.stdout + run.stderr)}"
                )
                continue
            tail = run.stdout.strip().splitlines()[-1:] or ["(无输出)"]
            hit = re.search(r"(\d+)\s*项断言", run.stdout)
            if hit and kind == "test":
                assertions = int(hit.group(1))
            elif hit and kind == "teaching":
                teaching_assertions = int(hit.group(1))
            logs.append(f"  ✅ [{label}] {tail[0][:80]}")
    substance = check_substance(unit_dir, meta, assertions, teaching_assertions)
    if substance:
        ok = False
        logs.extend(substance)
    if degraded and assertions is not None and assertions < MIN_DEGRADED_ASSERTIONS:
        logs.append(
            f"  ⚠️ 降级档测试偏薄：只有 {assertions} 项断言（建议至少 {MIN_DEGRADED_ASSERTIONS}）；"
            "sanitizer 未运行，Release-O2 不能覆盖内存与 UB。"
        )
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
    parser.add_argument(
        "--allow-degraded",
        action="store_true",
        help="仅当 sanitizer 环境自检失败时生效：跳过该档、只跑 Release，并把降级大声记在输出里",
    )
    opts = parser.parse_args()

    if not compiler():
        print("❌ 找不到 g++ 或 clang++")
        sys.exit(2)

    units = discover(opts.paths)
    if not units:
        print("⚠️  code/ 下还没有单元，跳过（脚手架已就位，等第一个清单现代化）")
        return

    # 先自检 sanitizer 环境。挂了就直说是环境挂了，别让人误以为是单元的代码坏了。
    profiles, degraded_note = PROFILES, None
    ok_env, env_out = sanitizer_preflight()
    if not ok_env:
        if not opts.allow_degraded:
            print("❌ sanitizer 环境自检失败——这不是某个单元的问题，是这台机器上跑不起来。")
            print(indent(env_out))
            print(
                "\n本档用的是：" + " ".join(dict(PROFILES)[SANITIZER_PROFILE])
                + "\n处理办法：换一台能跑 ASan 的机器，或确认无解后用 --allow-degraded"
                + "（只跑 Release，降级会写进输出与交接包，不会悄悄变绿）。"
            )
            sys.exit(2)  # 2 = 环境问题，区别于 1 = 代码问题
        profiles = [(n, f) for n, f in PROFILES if n != SANITIZER_PROFILE]
        degraded_note = (
            "⚠️  降级运行：sanitizer 档已跳过（环境自检失败），本次结果**不覆盖内存与 UB 检查**。\n"
            + indent(env_out, head=4, tail=6)
        )
        print(degraded_note + "\n")

    workdir = Path(ROOT / ".build") if opts.keep else Path(tempfile.mkdtemp(prefix="dsa-check-"))
    workdir.mkdir(parents=True, exist_ok=True)
    failed, blocks = [], []
    for unit in units:
        try:
            ok, log = build_and_run(
                unit, workdir, opts.keep, profiles, degraded=degraded_note is not None
            )
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
        f"（每个 {len(profiles)} 种构建：{', '.join(n for n, _ in profiles)}）"
    )
    if degraded_note:
        # 降级必须在结论旁边再喊一次：交接包里只贴尾部几行的人不能被瞒过去。
        print("⚠️  本次为降级运行，未跑 sanitizer 档——上面的绿不代表内存与 UB 干净。")
    if failed:
        print("失败: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
