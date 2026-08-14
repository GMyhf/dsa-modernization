"""勘误台账的单元测试。

和 `test_ledger.py` 一个规矩：**每条规则都要有一个会红的用例**。
只测通过路径的校验器等于没有校验器——这条勘误映射存在的全部意义，就是它会在
「有人把某条勘误的回归测试删了」时变红。
"""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import errata  # noqa: E402


def write_manifest(tmp, entries):
    path = Path(tmp) / "errata.json"
    path.write_text(json.dumps({"errata": entries}, ensure_ascii=False), encoding="utf-8")
    return path


def make_test(tmp, name, text):
    unit = Path(tmp) / "code" / name
    unit.mkdir(parents=True, exist_ok=True)
    (unit / "test.cpp").write_text(text, encoding="utf-8")
    return unit


RUNTIME = {"id": "E14", "source": "s", "listing": "算法3.6", "summary": "x", "kind": "runtime"}
PROSE = {"id": "R00", "source": "s", "listing": "1.4.2", "summary": "x", "kind": "prose",
         "reason": "文字问题", "by": "claude", "date": "2026-08-14"}


class TestManifestShape(unittest.TestCase):
    def test_rejects_bad_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_manifest(tmp, [{**RUNTIME, "id": "错误14"}])
            _, problems = errata.load(path)
        self.assertTrue(any("不合法" in p for p in problems))

    def test_rejects_duplicate_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_manifest(tmp, [RUNTIME, dict(RUNTIME)])
            _, problems = errata.load(path)
        self.assertTrue(any("重复条目" in p for p in problems))

    def test_rejects_unknown_kind(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_manifest(tmp, [{**RUNTIME, "kind": "maybe"}])
            _, problems = errata.load(path)
        self.assertTrue(any("kind=" in p for p in problems))

    def test_missing_field_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            entry = {k: v for k, v in RUNTIME.items() if k != "summary"}
            path = write_manifest(tmp, [entry])
            _, problems = errata.load(path)
        self.assertTrue(any("缺少 summary" in p for p in problems))

    def test_prose_needs_reason_by_date(self):
        """不写测试就得留下出处——和 exclusions.json 一个规矩。"""
        with tempfile.TemporaryDirectory() as tmp:
            entry = {k: v for k, v in PROSE.items() if k != "by"}
            path = write_manifest(tmp, [entry])
            _, problems = errata.load(path)
        self.assertTrue(any("必须写 by" in p for p in problems))

    def test_prose_with_full_provenance_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_manifest(tmp, [PROSE])
            entries, problems = errata.load(path)
        self.assertEqual(problems, [])
        self.assertEqual(entries[0]["id"], "R00")

    def test_compile_kind_needs_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_manifest(tmp, [{**RUNTIME, "id": "E01", "kind": "compile"}])
            _, problems = errata.load(path)
        self.assertTrue(any("evidence" in p for p in problems))


class TestTestScanning(unittest.TestCase):
    def test_finds_errata_ids_in_assertions(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_test(tmp, "ch03/a", 'check(x, "勘误E14 算法3.6：21! 抛 overflow_error");')
            make_test(tmp, "ch04/b", 'check(y, "勘误R10 算法4.6：下标与标准库一致");')
            found = errata.tests_mentioning(Path(tmp) / "code")
        self.assertEqual(sorted(found), ["E14", "R10"])
        self.assertEqual(len(found["E14"]), 1)

    def test_ignores_unrelated_text(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_test(tmp, "ch03/a", 'check(x, "勘误表里说过这件事");')
            found = errata.tests_mentioning(Path(tmp) / "code")
        self.assertEqual(found, {})

    def test_same_id_in_two_units_is_collected(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_test(tmp, "ch03/a", 'check(x, "勘误E22 五法则");')
            make_test(tmp, "ch04/b", 'check(y, "勘误E22 五法则");')
            found = errata.tests_mentioning(Path(tmp) / "code")
        self.assertEqual(len(found["E22"]), 2)


class TestLiveManifest(unittest.TestCase):
    """对着仓库真实的 errata.json 跑——它必须一直是绿的。"""

    def test_repository_manifest_is_consistent(self):
        state = errata.analyze()
        self.assertEqual(state["problems"], [], "\n".join(state["problems"]))

    def test_every_runtime_entry_has_a_test(self):
        state = errata.analyze()
        missing = [e["id"] for e in state["entries"]
                   if e.get("kind") in errata.NEEDS_TEST and not e.get("tests")]
        self.assertEqual(missing, [], f"这些勘误声称有回归测试却找不到: {missing}")

    def test_report_renders(self):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            print(errata.format_report(errata.analyze()))
        text = buffer.getvalue()
        self.assertIn("勘误台账", text)
        self.assertIn("E14", text)


if __name__ == "__main__":
    unittest.main()
