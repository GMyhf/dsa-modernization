"""正文保全度台账的单元测试。

这个工具只输出一个数字（book 汉字 / raw 汉字），所以它的全部风险都在
「切分对不对」上：切错了，数字照样是数字，人却看不出来。下面三类用例
分别钉住三次真实踩过的坑：围栏极性、章末小结被吞、节内小标题被漏。
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import fidelity  # noqa: E402


class TestSectionVolumes(unittest.TestCase):
    def volumes(self, text):
        return fidelity.section_volumes(text.splitlines())

    def test_counts_prose_and_skips_code(self):
        v = self.volumes(
            "## 8.4 交换排序\n"
            "冒泡排序反复交换相邻元素。\n"
            "```cpp\n"
            "// 交换相邻两项\n"
            "swap(a, b);\n"
            "```\n"
        )
        self.assertEqual(v, {"8.4": len("交换排序冒泡排序反复交换相邻元素")})

    def test_subsection_merges_into_parent(self):
        v = self.volumes(
            "## 8.4 交换排序\n引子一句。\n"
            "### 8.4.1 冒泡排序\n正文两句。\n"
            "### 8.4.2 快速排序\n再来一句。\n"
        )
        self.assertEqual(set(v), {"8.4"}, "三级节要并进二级节")

    def test_chapter_tail_ends_the_section(self):
        """`## 本章小结` 之后的字不属于最后一节——否则 10.3 会吞掉整章尾巴。"""
        v = self.volumes(
            "### 10.3.6 散列方法的应用\n正文。\n"
            "## 本章小结\n小结的字不算进 10.3。\n"
            "## 习题\n题目也不算。\n"
        )
        self.assertEqual(v["10.3"], len("散列方法的应用正文"))

    def test_unnumbered_subheading_stays_inside_section(self):
        """新书在节内加的教学小标题（三级及更深）算这一节的正文。"""
        v = self.volumes(
            "## 5.3 二叉树的存储结构\n引子。\n"
            "### 为什么这一节没有 Python 版\n理由一句。\n"
            "#### 教学版：完整实现\n再一句。\n"
        )
        self.assertEqual(
            v["5.3"],
            len("二叉树的存储结构引子为什么这一节没有版理由一句教学版完整实现再一句"),
        )

    def test_fence_polarity_is_global(self):
        """节的起点落在代码块中间时，不能从「不在围栏内」重新起步。"""
        text = (
            "## 1.1 引子\n开头一句。\n"
            "```cpp\n"
            "## 这行在代码块里，不是标题\n"
            "```\n"
            "## 1.2 正题\n结尾一句。\n"
        )
        v = self.volumes(text)
        self.assertEqual(v["1.1"], len("引子开头一句"))
        self.assertEqual(v["1.2"], len("正题结尾一句"))


class TestRatchet(unittest.TestCase):
    def rows(self, *pairs):
        return [
            {"section": s, "chapter": int(s.split(".")[0]), "raw": 100,
             "book": int(r * 100), "ratio": r}
            for s, r in pairs
        ]

    def test_drop_below_baseline_is_red(self):
        state = {"baseline": {"8.4": 0.60}, "waivers": []}
        self.assertEqual(fidelity.check(self.rows(("8.4", 0.40)), state), 1)

    def test_small_wording_change_is_not_red(self):
        state = {"baseline": {"8.4": 0.60}, "waivers": []}
        self.assertEqual(fidelity.check(self.rows(("8.4", 0.59)), state), 0)

    def test_unregistered_section_is_red(self):
        state = {"baseline": {}, "waivers": []}
        self.assertEqual(fidelity.check(self.rows(("8.4", 0.40)), state), 1)

    def test_waiver_exempts_a_section(self):
        state = {"baseline": {}, "waivers": [{"section": "8.4", "reason": "r",
                                              "by": "Claude", "date": "2026-09-04"}]}
        self.assertEqual(fidelity.check(self.rows(("8.4", 0.10)), state), 0)

    def test_update_only_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            state_path = Path(tmp) / "fidelity.json"
            original = fidelity.STATE
            fidelity.STATE = state_path
            try:
                state = {"baseline": {"8.4": 0.60}, "waivers": []}
                fidelity.update(self.rows(("8.4", 0.40), ("8.5", 0.30)), state)
                saved = json.loads(state_path.read_text(encoding="utf-8"))
            finally:
                fidelity.STATE = original
        self.assertEqual(saved["baseline"]["8.4"], 0.60, "基线只升不降")
        self.assertEqual(saved["baseline"]["8.5"], 0.30)


class TestRegistryIsHonest(unittest.TestCase):
    def test_every_section_has_a_baseline_or_a_signed_waiver(self):
        rows = fidelity.collect()
        state = fidelity.load_state()
        baseline = state.get("baseline", {})
        for waiver in state.get("waivers", []):
            for field in ("section", "reason", "by", "date"):
                self.assertIn(field, waiver, "豁免必须写清理由、署名、日期")
        skip = fidelity.waived(state)
        missing = [r["section"] for r in rows
                   if r["section"] not in baseline and r["section"] not in skip]
        self.assertEqual(missing, [], "新出现的节要登记基线")


if __name__ == "__main__":
    unittest.main()
