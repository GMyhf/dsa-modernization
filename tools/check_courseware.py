#!/usr/bin/env python3
"""check_courseware.py — 把 courseware/ 的自带闸门接进 `handoff.py --verify`。

存在的理由：`courseware/` 是仓库里**唯一**允许第三方依赖的目录
（python-pptx + Pillow，见 PLAN T-065），所以它的闸门 `courseware/verify.py`
一直留在主闸门之外，靠人记得手跑。2026-09-05 复核时量了一下这条缝有多宽：
Codex 那一轮给 `verify.py` 补的 8 项回归测试（其中 4 项在旧实现上真的红）
在 `tools/handoff.py --verify` 的 13 步里**一次都不会跑**；
`courseware/*.py` 连 `py_compile` 都没进过。闸门之外的检查等于没有闸门。

这个包装器本身不引入依赖——它只 shell 出去跑。缺件时**降级但出声**：
`pdfref.py` 缺扫描件、视频缺席时只提示不判红是同一套做法，闸门不该因为某台机器
少装一个库就变红；但降级必须打印出来，否则「装了却没跑」和「跑过且全绿」
在交接记录上长得一模一样。`--require` 把任何一次降级变成红，给 CI 用。

**渲染检查默认就跑**，不是可选项：第 7 项是唯一看得见「两段文字压在一起」的判据，
而版面事故正是这套课件出过三次的事故。实测 12 章 378 页约 33 秒（check_code 要 126 秒），
这点时间买不到不跑它的理由；真要跳过用 `--no-render`。

用法:
  python3 tools/check_courseware.py             # 语法 + 自测 + verify.py 全部 9 项
  python3 tools/check_courseware.py --no-render # 跳过第 7 项（本地快速循环）
  python3 tools/check_courseware.py --require   # 任何降级都判红（CI）
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

COURSEWARE = ROOT / "courseware"
RENDER_TOOLS = ("soffice", "pdftotext", "pdffonts")


def has_pptx() -> bool:
    """探针单独起进程：主闸门自己不许 import 第三方库。"""
    probe = subprocess.run([sys.executable, "-c", "import pptx"],
                           capture_output=True, text=True)
    return probe.returncode == 0


def missing_render_tools():
    return [name for name in RENDER_TOOLS if shutil.which(name) is None]


def run(step, label, cwd=ROOT, tail_lines=4):
    proc = subprocess.run(step, cwd=cwd, capture_output=True, text=True)
    body = (proc.stdout + proc.stderr).strip()
    if proc.returncode == 0:
        print(f"  ✅ {label}")
        for line in body.splitlines()[-tail_lines:]:
            print(f"     {line}")
        return True
    print(f"  ❌ {label}")
    print(body)
    return False


def main(argv=None):
    ap = argparse.ArgumentParser(description="courseware/ 闸门的包装器")
    ap.add_argument("--no-render", dest="render", action="store_false",
                    help="跳过第 7 项渲染检查（默认跑）")
    ap.add_argument("--require", action="store_true",
                    help="缺件时判红而不是降级")
    args = ap.parse_args(argv)

    if not COURSEWARE.is_dir():
        print(f"❌ 找不到 {rel_label(COURSEWARE)}/", file=sys.stderr)
        return 1

    degraded = []
    py_files = sorted(str(p.relative_to(ROOT)) for p in COURSEWARE.rglob("*.py"))
    ok = run([sys.executable, "-m", "py_compile", *py_files],
             f"python3 -m py_compile <{len(py_files)} 个 courseware 文件>")

    if not has_pptx():
        degraded.append("没有 python-pptx，自测与 verify.py 全部未跑"
                        "（装法：pip install python-pptx）")
        return report(ok, degraded, args, scope="什么都没跑")

    ok &= run([sys.executable, "-m", "unittest", "discover",
               "-s", "courseware", "-p", "test_verify.py"],
              "python3 -m unittest discover -s courseware -p test_verify.py")

    render = args.render
    if render:
        absent = missing_render_tools()
        if absent:
            degraded.append(f"缺 {'、'.join(absent)}，第 7 项渲染检查未跑"
                            f"（装法：apt install libreoffice poppler-utils）")
            render = False
    elif args.require:
        degraded.append("--no-render 明确跳过了第 7 项渲染检查")

    cmd = [sys.executable, "verify.py"] + (["--render"] if render else [])
    ok &= run(cmd, "python3 courseware/verify.py" + (" --render" if render else ""),
              cwd=COURSEWARE, tail_lines=3)
    scope = "第 1–9 项" if render else "第 1–6、8、9 项（渲染检查未跑）"
    return report(ok, degraded, args, scope)


def report(ok, degraded, args, scope):
    for msg in degraded:
        print(f"  ⚠️  降级：{msg}")
    if degraded and args.require:
        print("❌ courseware 闸门降级，而 --require 不接受降级。")
        return 1
    if not ok:
        return 1
    if degraded:
        print(f"⚠️  courseware 闸门降级通过（{scope}）—— 不能据此声称课件已验收。")
    else:
        print(f"✅ courseware 闸门通过：{scope}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
