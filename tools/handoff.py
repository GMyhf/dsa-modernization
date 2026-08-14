#!/usr/bin/env python3
"""handoff.py — 把一方的 git 改动整理成给另一方 AI 的 review 输入包。

用法:
  python3 tools/handoff.py --from claude --to codex
  python3 tools/handoff.py --from claude --to codex --base main
  python3 tools/handoff.py --from codex --to claude --range HEAD~3..HEAD --verify
  python3 tools/handoff.py --from claude --stdout        # 打印而不写文件
  python3 tools/handoff.py --verify                      # 只跑闸门

参数:
  --from <name>   交接方（claude|codex），默认 claude
  --to <name>     接收方，默认取另一方
  --base <ref>    审查 <ref>..HEAD 的全部改动
  --range <a..b>  显式 git range，优先级高于 --base
  --out <path>    输出路径，默认 collab/review-input.md
  --verify        附带跑闸门并把结果写进包里
  --stdout        打印到 stdout，不写文件

无 --base/--range 时自动推断：工作区有未提交改动 → 对比 HEAD；否则 → HEAD~1..HEAD。
只用 Python 标准库 + git，无第三方依赖。移植自 cs101.openjudge.cn/tools/handoff.py。
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COLLAB = ROOT / "collab"
OTHER = {"claude": "codex", "codex": "claude"}
MAX_DIFF_BYTES = 200_000  # 超过则截断，避免生成一个没法读的巨文件

CHECKLIST = """## Review 检查清单（本项目红线）

- [ ] **原文不可改**：`dsa_raw.md` 在 diff 里出现了吗？它是 OCR 原始底稿，只读。
      任何修订都应落在 `book/`，让原书与改法可以逐条对照。
- [ ] **书上的代码 = 能跑的代码**：书稿里的 `cpp` 块是否都带 `file=` 引用并逐字一致
      （`tools/check_doc.py` R3）？有没有为了排版好看而手改块内容、和 `code/` 漂移？
- [ ] **现代化是否真的成立**：`-Wall -Wextra -Wpedantic -Werror` 全绿吗？ASan/UBSan 跑过吗？
      新写的 `test.cpp` 是否覆盖了原书写法会挂而现代写法不会挂的那个点——
      **把断言改坏能立刻变红吗**（变异自检）？
- [ ] **D-001 风格公约**（`collab/DECISION_LOG.md`，人已拍板）：标准是 C++17 吗？
      实现里有没有混进 `std::cout` 或 STL 容器（`check_code.py` 会静态拦，但豁免项
      `d001_exceptions` 是否写了真理由）？空状态用 `optional`、真错误抛标准异常了吗？
      有没有成员变量与成员函数重名？
- [ ] **教学价值有没有被弄丢**：现代化不是把教材改写成 STL 调用集。
      原书要教的那个数据结构本身（指针操作、扩容策略、复杂度）还看得见吗？
      `legacy.md` 是否说清了「原书这样写 → 具体错在哪 / 落后在哪 → 现在这样写」？
- [ ] **编号与交叉引用**：章节号、算法/代码编号是否与原书一致？
      正文里的「见算法3.3」「如图3.3」是否仍然指得到东西（R5/R6/R7）？
- [ ] **台账销账**：`python3 tools/ledger.py --check` 通过吗？本轮做掉的清单是否已被
      某个 `code/**/unit.json` 认领？**决定不做的清单，是否在 `collab/exclusions.json`
      里留下了理由、署名和日期**——而不是悄悄消失？
- [ ] **插图**：新引的图是否已 vendored 到 `book/assets/` 且写了真图注（R4）？
- [ ] **可回归**：`python3 tools/handoff.py --verify` 是否真的跑过并全绿？
      交接记录里有没有贴出闸门尾部的计数，而不是「我觉得没问题」？"""


def git(args, soft=False, keep_output_on_error=False):
    """跑 git。soft=True 时失败返回空串。

    keep_output_on_error：`git diff --no-index` 在「有差异」时退出码就是 1，
    这时 stdout 里正是我们要的 diff，不能当失败丢掉。
    """
    proc = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0 and not (soft or keep_output_on_error):
        raise subprocess.CalledProcessError(proc.returncode, proc.args, proc.stdout, proc.stderr)
    if proc.returncode != 0 and not keep_output_on_error:
        return ""
    return proc.stdout.rstrip("\n")


def resolve_range(opts):
    if opts.range:
        return opts.range, "range"
    if opts.base:
        return f"{opts.base}..HEAD", "range"
    dirty = git(["status", "--porcelain"], soft=True)
    if dirty:
        return "HEAD", "worktree"  # git diff HEAD == 未提交(已跟踪)改动
    has_parent = git(["rev-parse", "--verify", "--quiet", "HEAD~1"], soft=True)
    if not has_parent:
        return "HEAD", "worktree"
    return "HEAD~1..HEAD", "range"


def collect(opts):
    rng, mode = resolve_range(opts)
    diff_args = ["diff", "HEAD"] if mode == "worktree" else ["diff", rng]
    data = {
        "range": rng,
        "mode": mode,
        "diff_args": diff_args,
        "branch": git(["rev-parse", "--abbrev-ref", "HEAD"], soft=True),
        "head_sha": git(["rev-parse", "--short", "HEAD"], soft=True),
        "stat": git([*diff_args, "--stat"], soft=True),
        "name_status": git([*diff_args, "--name-status"], soft=True),
        "untracked": git(["ls-files", "--others", "--exclude-standard"], soft=True),
        "log": git(["log", "--oneline", "--no-decorate", rng], soft=True) if mode == "range" else "",
    }
    diff = git(diff_args, soft=True)
    if mode == "worktree" and data["untracked"]:
        # 新增但还没 git add 的文件不在 `git diff HEAD` 里。第一轮交接时**所有东西都是新文件**，
        # 不补这一段，审查方拿到的是一份空 diff。用 --no-index 生成，不碰 index。
        diff = "\n".join([diff, *untracked_diffs(data["untracked"].splitlines())]).strip("\n")
    data["truncated"] = len(diff.encode()) > MAX_DIFF_BYTES
    if data["truncated"]:
        diff = diff.encode()[:MAX_DIFF_BYTES].decode(errors="replace")
    data["diff"] = diff
    return data


def untracked_diffs(paths):
    """把未跟踪文件也变成可读的 diff。二进制由 git 自己缩成一行 'Binary files ... differ'。"""
    out = []
    for path in paths:
        chunk = git(["diff", "--no-index", "--", "/dev/null", path], keep_output_on_error=True)
        if chunk:
            out.append(chunk)
    return out


def read_notes(who):
    path = COLLAB / f"NOTES-{who}.md"
    return path.read_text(encoding="utf-8").strip() if path.is_file() else ""


def read_open_items():
    path = COLLAB / "PLAN.md"
    if not path.is_file():
        return ""
    rows = [
        line
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.lstrip().startswith("| T-") and "Done" not in line
    ]
    return "\n".join(rows)


def run_verify():
    """交接闸门。顺序有意为之：先自证工具没坏，再用工具去证内容没坏。"""
    py_files = sorted(
        str(p.relative_to(ROOT)) for p in ROOT.glob("tools/*.py")
    ) + sorted(str(p.relative_to(ROOT)) for p in ROOT.glob("tests/*.py"))
    steps = [
        # 1. 工具自身的单元测试。闸门自己坏了，后面全绿也没有意义。
        ["python3", "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py", "-v"],
        ["python3", "-m", "py_compile", *py_files],
        # 2. 台账：105 条清单谁也不许悄悄消失。
        ["python3", "tools/ledger.py", "--check"],
        # 2b. 勘误台账：每条「跑起来是错的」勘误都要指得出那句会红的断言。
        ["python3", "tools/errata.py", "--check"],
        # 3. 书稿：OCR 残留、编号、插图、以及「书上代码 == code/ 里的代码」。
        ["python3", "tools/check_doc.py"],
        # 4. 代码：真编译、真跑、Werror + ASan/UBSan。本项目最硬的一条。
        ["python3", "tools/check_code.py"],
    ]
    outputs, ok = [], True
    for step in steps:
        proc = subprocess.run(step, cwd=ROOT, capture_output=True, text=True)
        label = " ".join(step if len(step) < 6 else [*step[:3], f"<{len(py_files)} files>"])
        body = (proc.stdout + proc.stderr).strip()
        if proc.returncode == 0:
            # 保留尾部计数：交接记录要贴的就是这几行，不能只写「通过」
            tail = "\n".join(body.splitlines()[-6:])
            outputs.append(f"$ {label}\n✅ ok\n{tail}" if tail else f"$ {label}\n✅ ok")
        else:
            outputs.append(f"$ {label}\n❌ 失败\n{body}")
            ok = False
    return ok, "\n\n".join(outputs)


def build(opts, data, verify_result):
    to = opts.to or OTHER[opts.sender]
    lines = [f"# Review 输入包 · {opts.sender} → {to}", ""]
    lines += [
        "> 由 `tools/handoff.py` 自动生成，不入库。审查方读完请把意见写进 "
        f"`collab/NOTES-{to}.md`，并在 `collab/HANDOFF.md` 追加一条交接记录。",
        "",
        "## 概况",
        "",
        f"- 分支: `{data['branch']}` @ `{data['head_sha']}`",
        f"- 对比范围: `{data['range']}`（{'未提交改动 vs HEAD' if data['mode'] == 'worktree' else '提交区间'}）",
    ]
    if data["truncated"]:
        lines.append(
            f"- ⚠️ diff 超过 {MAX_DIFF_BYTES} 字节已截断，完整改动请用 `git {' '.join(data['diff_args'])}` 查看"
        )
    lines.append("")

    open_items = read_open_items()
    if open_items:
        lines += ["## PLAN 中未完成的任务", "", "```", open_items, "```", ""]
    if data["log"]:
        lines += ["## 本区间提交", "", "```", data["log"], "```", ""]

    lines += ["## 改动文件", "", "```", data["name_status"] or "(无跟踪改动)", "```"]
    if data["untracked"]:
        lines += ["", "未跟踪(新增未 add)文件：", "```", data["untracked"], "```"]
    lines.append("")

    if data["stat"]:
        lines += ["<details><summary>diffstat</summary>", "", "```", data["stat"], "```", "", "</details>", ""]

    notes = read_notes(opts.sender)
    if notes:
        lines += [f"## 交接方留言（NOTES-{opts.sender}.md）", "", notes, ""]

    if verify_result:
        ok, out = verify_result
        lines += [f"## 闸门结果：{'✅ 通过' if ok else '❌ 失败'}", "", "```", out, "```", ""]

    lines += ["## 完整 Diff", "", "```diff", data["diff"] or "(空)", "```", "", CHECKLIST, ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(add_help=True, description="生成给另一方 AI 的 review 输入包")
    parser.add_argument("--from", dest="sender", default="claude", choices=["claude", "codex"])
    parser.add_argument("--to", choices=["claude", "codex"])
    parser.add_argument("--base")
    parser.add_argument("--range")
    parser.add_argument("--out")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--stdout", action="store_true")
    opts = parser.parse_args()

    only_verify = (
        opts.verify
        and not any([opts.to, opts.base, opts.range, opts.out, opts.stdout])
        and "--from" not in sys.argv
    )
    verify_result = run_verify() if opts.verify else None
    if only_verify:
        ok, out = verify_result
        print(out)
        sys.exit(0 if ok else 1)

    data = collect(opts)
    markdown = build(opts, data, verify_result)
    if opts.stdout:
        print(markdown)
        return
    out_path = (ROOT / opts.out) if opts.out else (COLLAB / "review-input.md")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(markdown, encoding="utf-8")
    rel = out_path.relative_to(ROOT)
    print(f"✅ 已生成 review 输入包: {rel}")
    print(f"   把它交给 {opts.to or OTHER[opts.sender]}，或让对方直接读这个文件。")
    if verify_result and not verify_result[0]:
        sys.exit(1)


if __name__ == "__main__":
    main()
