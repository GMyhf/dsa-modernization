"""台账工具的单元测试。

闸门的第一步是「先自证工具没坏」——如果 ledger 把 105 条清单少数了一条，
后面所有「已覆盖 / 待办」的结论都是假的。
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import ledger  # noqa: E402


class TestParseInventory(unittest.TestCase):
    def test_parses_kind_number_and_end_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "raw.md"
            raw.write_text(
                "前言\n"
                "【算法3.3】改进的进栈操作。\n"
                "```cpp\ncode\n```\n"
                "【算法3.3结束】\n"
                "【代码 5.8】某某。\n"      # OCR 里空格位置不稳定
                "正文没有结束标记\n",
                encoding="utf-8",
            )
            items = ledger.parse_inventory(raw)
        self.assertEqual([i["id"] for i in items], ["算法3.3", "代码5.8"])
        self.assertEqual(items[0]["chapter"], 3)
        self.assertTrue(items[0]["has_end"])
        self.assertFalse(items[1]["has_end"], "没有结束标记的清单必须被标出来")
        self.assertEqual(items[0]["line"], 2)

    def test_tolerates_ocr_damaged_end_marker(self):
        """原书里有「法3.3结束】」这种被吃掉前缀的结束标记，也该算配对上。"""
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "raw.md"
            raw.write_text("【算法3.3】x\n法3.3结束】\n", encoding="utf-8")
            items = ledger.parse_inventory(raw)
        self.assertTrue(items[0]["has_end"])

    def test_real_book_inventory_is_stable(self):
        """对着真书本体的回归锚：105 条（算法 70 / 代码 35），5 条缺结束标记。

        这个数字变了，要么 dsa_raw.md 被改了（它是只读底稿），要么解析退化了。
        两种情况都必须有人来看一眼，不该悄悄通过。
        """
        items = ledger.parse_inventory()
        self.assertEqual(len(items), 105)
        self.assertEqual(sum(1 for i in items if i["kind"] == "算法"), 70)
        self.assertEqual(sum(1 for i in items if i["kind"] == "代码"), 35)
        self.assertEqual(
            sorted(i["id"] for i in items if not i["has_end"]),
            sorted(["算法2.11", "代码3.1", "代码5.8", "算法7.6", "算法7.9"]),
        )
        self.assertEqual(len({i["id"] for i in items}), 105, "清单编号不该重复")


class TestUnitsAndExclusions(unittest.TestCase):
    def make_unit(self, root: Path, name, **overrides):
        d = root / name
        d.mkdir(parents=True)
        meta = {"id": name, "title": "t", "listings": [
            {"id": "算法3.3", "anchor": "anchor", "test": "test"}
        ], "standard": "c++20"}
        meta.update(overrides)
        (d / "unit.json").write_text(json.dumps(meta), encoding="utf-8")
        for f in ("legacy.md", "test.cpp", "modern.hpp"):
            (d / f).write_text("x", encoding="utf-8")
        return d

    def test_detects_id_directory_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "array_stack", id="wrong_name")
            _, problems = ledger.load_units(Path(tmp))
        self.assertTrue(any("与目录名" in p for p in problems))

    def test_detects_missing_required_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self.make_unit(Path(tmp), "u")
            (unit / "test.cpp").unlink()
            (unit / "modern.hpp").unlink()
            _, problems = ledger.load_units(Path(tmp))
        self.assertTrue(any("缺少 test.cpp" in p for p in problems))
        self.assertTrue(any("modern.hpp 或 modern.cpp" in p for p in problems))

    def test_detects_unknown_standard_and_empty_listings(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", standard="c++98", listings=[])
            _, problems = ledger.load_units(Path(tmp))
        self.assertTrue(any("standard" in p for p in problems))
        self.assertTrue(any("listings" in p for p in problems))

    def test_empty_listings_needs_beyond_book(self):
        """第11章、Trie/Patricia 这类新增实现没有清单可认领，但必须自报家门。"""
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", listings=[], beyond_book="第11章原书没有清单")
            units, problems = ledger.load_units(Path(tmp))
        self.assertEqual(problems, [])
        self.assertEqual(units[0]["listings"], [])

    def test_blank_beyond_book_does_not_count(self):
        """空字符串糊弄不过去——否则「忘了填」和「本来就没有」又混在一起了。"""
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", listings=[], beyond_book="   ")
            _, problems = ledger.load_units(Path(tmp))
        self.assertTrue(any("beyond_book" in p for p in problems))

    def test_beyond_book_units_do_not_inflate_coverage(self):
        """新增单元不认领任何清单，105 的等式因此一动不动。"""
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", listings=[], beyond_book="原书无对应清单")
            units, _ = ledger.load_units(Path(tmp))
        claimed = [listing for unit in units for listing in unit["listings"]]
        self.assertEqual(claimed, [])

    def test_object_listing_id_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", listings=[{"id": "算法3.3", "anchor": "a", "test": "t"}])
            units, problems = ledger.load_units(Path(tmp))
        self.assertEqual(problems, [])
        self.assertEqual(units[0]["listings"][0]["id"], "算法3.3")

    def test_bare_string_listing_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.make_unit(Path(tmp), "u", listings=["算法3.3"])
            _, problems = ledger.load_units(Path(tmp))
        self.assertTrue(any("必须使用 {id, anchor, test} 对象" in p for p in problems), problems)

    def test_exclusion_without_reason_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "exclusions.json"
            path.write_text(
                json.dumps({"exclusions": [{"listing": "算法3.3", "by": "codex"}]}),
                encoding="utf-8",
            )
            _, problems = ledger.load_exclusions(path)
        self.assertTrue(any("reason" in p for p in problems), "退场不写理由必须报错")
        self.assertTrue(any("date" in p for p in problems))

    def test_missing_exclusions_file_is_not_an_error(self):
        entries, problems = ledger.load_exclusions(Path("/nonexistent/exclusions.json"))
        self.assertEqual((entries, problems), ({}, []))


class TestRepoState(unittest.TestCase):
    def test_repository_ledger_is_consistent(self):
        state = ledger.analyze()
        self.assertEqual(state["problems"], [], "仓库当前台账不一致")

    def test_every_claimed_listing_exists_in_the_book(self):
        state = ledger.analyze()
        known = {i["id"] for i in state["inventory"]}
        self.assertTrue(set(state["claimed"]) <= known)

    def test_counts_add_up(self):
        state = ledger.analyze()
        self.assertEqual(
            len(state["inventory"]),
            len(state["claimed"]) + len(state["exclusions"]) + len(state["pending"]),
            "已覆盖 + 退场 + 待办 必须等于清单总数，不能有清单凭空消失",
        )


if __name__ == "__main__":
    unittest.main()
