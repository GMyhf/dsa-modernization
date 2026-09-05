"""courseware/ 的闸门必须真的在闸门里。

2026-09-05 的复核抓到的不是某条检查写错了，而是一整套检查**没有被任何人跑**：
`courseware/verify.py` 的 9 项、以及刚给它补的 8 项回归测试，
在 `tools/handoff.py --verify` 的步骤表里一次都没出现过。
「装了却不跑」和「压根没写」在交接记录上长得一模一样——都是绿的。

所以这里钉住三件事：
* 步骤表里有 `check_courseware.py`（这是那个洞本身）；
* 闸门没有把渲染检查关掉（第 7 项是唯一看得见「两段文字压在一起」的判据）；
* 缺 python-pptx 时降级退 0、`--require` 时判红——闸门不该因为少装一个库变红，
  但降级必须**说出口**，不能悄悄当成通过。
"""
import io
import sys
import unittest
import contextlib
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import check_courseware  # noqa: E402
import handoff           # noqa: E402


def steps_as_text():
    return [" ".join(step) for step in handoff.verify_steps(py_files=["x.py"])]


class TestTheGateRunsIt(unittest.TestCase):
    def test_the_step_table_contains_the_courseware_gate(self):
        self.assertTrue(
            any("tools/check_courseware.py" in line for line in steps_as_text()),
            "courseware/ 的闸门必须出现在 handoff.py 的步骤表里",
        )

    def test_the_gate_does_not_opt_out_of_the_render_check(self):
        """第 7 项是唯一看得见「两段文字压在一起」的判据，闸门里不许关掉它。"""
        self.assertNotIn("python3 tools/check_courseware.py --no-render", steps_as_text())

    def test_the_wrapper_stays_dependency_free(self):
        """tools/ 是零第三方依赖区：这个包装器只许 shell 出去，不许 import pptx。"""
        source = (ROOT / "tools" / "check_courseware.py").read_text(encoding="utf-8")
        for line in source.splitlines():
            stripped = line.strip()
            if stripped.startswith(("import ", "from ")):
                self.assertNotIn("pptx", stripped)
                self.assertNotIn("PIL", stripped)


class TestDegradation(unittest.TestCase):
    def run_main(self, argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = check_courseware.main(argv)
        return code, out.getvalue()

    def test_missing_python_pptx_degrades_out_loud(self):
        with patch.object(check_courseware, "has_pptx", return_value=False):
            code, out = self.run_main([])
        self.assertEqual(code, 0, "少装一个库不该让整条闸门变红")
        self.assertIn("降级", out, "降级必须说出口，否则和通过分不出来")

    def test_require_turns_a_missing_dependency_red(self):
        with patch.object(check_courseware, "has_pptx", return_value=False):
            code, _ = self.run_main(["--require"])
        self.assertEqual(code, 1)

    def test_missing_libreoffice_degrades_but_keeps_the_rest(self):
        """少装 LibreOffice 只该丢掉第 7 项，不该让整条闸门变红——但必须说出口。"""
        with patch.object(check_courseware, "has_pptx", return_value=True), \
             patch.object(check_courseware, "missing_render_tools", return_value=["soffice"]), \
             patch.object(check_courseware, "run", return_value=True) as runner:
            code, out = self.run_main([])
        self.assertEqual(code, 0)
        self.assertIn("降级", out)
        self.assertIn("soffice", out)
        called = [" ".join(map(str, call.args[0])) for call in runner.call_args_list]
        self.assertFalse([c for c in called if "--render" in c],
                         "缺 LibreOffice 时不该还去调渲染检查")

    def test_render_is_on_by_default(self):
        """第 7 项默认就跑：它是唯一看得见「两段文字压在一起」的判据。"""
        with patch.object(check_courseware, "has_pptx", return_value=True), \
             patch.object(check_courseware, "missing_render_tools", return_value=[]), \
             patch.object(check_courseware, "run", return_value=True) as runner:
            code, _ = self.run_main([])
        self.assertEqual(code, 0)
        called = [" ".join(map(str, call.args[0])) for call in runner.call_args_list]
        self.assertTrue([c for c in called if c.endswith("verify.py --render")],
                        f"默认应当跑 verify.py --render，实际跑了 {called}")

    def test_probe_does_not_import_pptx_into_this_process(self):
        check_courseware.has_pptx()
        self.assertNotIn("pptx", sys.modules)


if __name__ == "__main__":
    unittest.main()
