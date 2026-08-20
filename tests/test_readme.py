"""根 README 的「现状」块必须与闸门当场量出来的数字一致。

这份 README 是仓库的门面，也是最容易烂掉的一页：它把台账、规则数、单元数、页数
全抄了一遍，而抄下来的数字不会自己跟着仓库走。2026-08-20 的一次复核就撞上了——
上面还写着「16 个文件、16 条规则、301 项自测」，实际是 29 / 17 / 346。

所以这里不校对措辞，只做一件事：把 README 里的每个数字，换成从台账、check_doc、
code/ 目录、PDF sidecar 现场算出来的那个。数字一旦对不上，这条测试红，
而不是等下一个人读到过期的介绍。
"""
import io
import json
import re
import sys
import unittest
import contextlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import check_doc  # noqa: E402
import ledger     # noqa: E402

README = (ROOT / "README.md").read_text(encoding="utf-8")


def number_near(pattern):
    """README 里紧挨着某段字样的那个整数。"""
    match = re.search(pattern, README)
    assert match, f"README 里找不到 {pattern}"
    return int(match.group(1).replace(",", ""))


class TestReadmeNumbers(unittest.TestCase):
    def test_ledger_line_matches_the_ledger(self):
        state = ledger.analyze()
        self.assertEqual(state["problems"], [])
        self.assertEqual(number_near(r"台账\s+(\d+) 已现代化"), len(state["claimed"]))
        self.assertEqual(number_near(r"已现代化 / (\d+) 退场"), len(state["exclusions"]))
        self.assertEqual(number_near(r"退场 / (\d+) 待办"), len(state["pending"]))
        self.assertEqual(number_near(r"= (\d+) 条清单"), len(state["inventory"]))

    def test_document_counts_match_check_doc(self):
        pages = sorted((ROOT / "book").glob("*.md"))
        slides = sorted((ROOT / "book" / "slides").glob("*.md"))
        self.assertEqual(number_near(r"书稿\s+(\d+) 个文件"), len(pages) + len(slides))
        self.assertEqual(number_near(r"加 (\d+) 套课件"), len(slides))
        self.assertEqual(number_near(r"），(\d+) 条规则通过"), len(check_doc.RULES))
        self.assertEqual(number_near(r"（`check_doc\.py`）——(\d+) 条规则"), len(check_doc.RULES))

    def test_unit_count_matches_the_code_tree(self):
        units = [p for p in (ROOT / "code").glob("*/*/unit.json")]
        self.assertEqual(number_near(r"代码\s+(\d+) 个单元"), len(units))
        self.assertEqual(number_near(r"退出码 0、(\d+)/\d+ 单元"), len(units))

    def test_pdf_numbers_come_from_the_sidecar(self):
        info = json.loads((ROOT / "book" / "pdf" / "build-info.json").read_text(encoding="utf-8"))
        size = (ROOT / "book" / "pdf" / "数据结构与算法.pdf").stat().st_size / 1024 / 1024
        self.assertEqual(number_near(r"book/pdf/，(\d+) 页"), info["pages"])
        self.assertEqual(number_near(r"下载卡片（(\d+) 页"), info["pages"])
        self.assertAlmostEqual(float(re.search(r"页 / ([\d.]+) MB", README).group(1)), size, places=1)

    def test_slide_page_count_matches_the_registry(self):
        total = sum(len(titles) for titles in check_doc.slide_pages().values())
        self.assertEqual(number_near(r"book/slides/，(\d+) 页幻灯片"), total)

    def test_self_test_count_is_the_real_one(self):
        """自测项数自己也要现场数——包括这一条。"""
        loader = unittest.TestLoader()
        with contextlib.redirect_stderr(io.StringIO()):
            suite = loader.discover(str(ROOT / "tests"), pattern="test_*.py")
        self.assertEqual(number_near(r"自测\s+(\d+) 项"), suite.countTestCases())
        self.assertEqual(number_near(r"单元测试，(\d+) 项"), suite.countTestCases())


if __name__ == "__main__":
    unittest.main()
