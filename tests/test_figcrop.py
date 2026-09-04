"""扫描件裁图登记表的单元测试。

这批图和 `book/assets/` 那 292 张不一样：那些的文件名就是内容哈希，
`collect_figures.py` 一比就知道有没有被动过；而 `assets/scan/` 下的图是**裁出来的**，
文件名是人起的。所以它的可信度全靠两件事：

* 登记表说得出「裁自哪一页、哪一块、什么 dpi」——图是可复算的；
* `--check` 逐张核对 sha256——图被人手改过就会红。

下面的用例钉住的就是这两条，外加「书稿引用的每张 scan 图都必须登记」。
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import figcrop  # noqa: E402


class TestRegistryShape(unittest.TestCase):
    def test_every_entry_has_a_reproducible_recipe(self):
        data = figcrop.load_registry()
        self.assertTrue(data.get("figures"), "登记表不该是空的")
        for item in data["figures"]:
            for field in ("id", "file", "page", "box", "dpi", "sha256", "by", "date"):
                self.assertIn(field, item, f"{item.get('id')} 缺 {field}")
            self.assertEqual(len(item["box"]), 4, "裁剪框是 [x, y, w, h]")
            self.assertTrue(all(isinstance(v, int) and v >= 0 for v in item["box"]))
            self.assertGreater(item["page"], 0)
            self.assertEqual(len(item["sha256"]), 64)

    def test_files_exist_and_match_their_hash(self):
        self.assertEqual(figcrop.cmd_check(figcrop.load_registry()), 0)

    def test_ids_and_files_are_unique(self):
        figures = figcrop.load_registry()["figures"]
        ids = [f["id"] for f in figures]
        files = [f["file"] for f in figures]
        self.assertEqual(len(ids), len(set(ids)), "图号不能重复登记")
        self.assertEqual(len(files), len(set(files)), "文件名不能重复登记")


class TestCheckCatchesTampering(unittest.TestCase):
    """核心判据：改了图的字节，闸门必须红。"""

    def run_check_with(self, entries, scan_dir):
        original = figcrop.SCAN_DIR
        figcrop.SCAN_DIR = scan_dir
        try:
            return figcrop.cmd_check({"figures": entries}, used={})
        finally:
            figcrop.SCAN_DIR = original

    def test_changed_bytes_are_red(self):
        with tempfile.TemporaryDirectory() as tmp:
            scan = Path(tmp)
            target = scan / "fig-x.png"
            target.write_bytes(b"pretend this is a png")
            entry = {"id": "图X.1", "file": "fig-x.png", "page": 1, "box": [0, 0, 1, 1],
                     "dpi": 200, "sha256": figcrop.sha256_of(target),
                     "by": "t", "date": "2026-09-04"}
            self.assertEqual(self.run_check_with([entry], scan), 0)
            target.write_bytes(b"someone edited the figure by hand")
            self.assertEqual(self.run_check_with([entry], scan), 1)

    def test_missing_file_is_red(self):
        with tempfile.TemporaryDirectory() as tmp:
            entry = {"id": "图X.1", "file": "gone.png", "page": 1, "box": [0, 0, 1, 1],
                     "dpi": 200, "sha256": "0" * 64, "by": "t", "date": "2026-09-04"}
            self.assertEqual(self.run_check_with([entry], Path(tmp)), 1)


class TestBookReferences(unittest.TestCase):
    def test_no_stitched_figures_left(self):
        """拼接图已经全部换成扫描裁图——书稿里不该再出现 assets/combined/。"""
        self.assertEqual(figcrop.used_paths(figcrop.COMBINED_RE), {})

    def test_every_referenced_scan_figure_is_registered(self):
        used = set(figcrop.used_paths(figcrop.SCAN_RE))
        registered = {f["file"] for f in figcrop.load_registry()["figures"]}
        self.assertEqual(used - registered, set(), "书稿引用了没有登记出处的裁图")

    def test_exercises_do_not_reference_missing_figures(self):
        """习题点名的图必须真的画在书上——否则题目做不了。"""
        self.assertEqual(figcrop.dangling_exercise_figures(), [])

    def test_registry_covers_the_book(self):
        """反向：登记了却没人引用的裁图，说明书稿改动时漏掉了它。"""
        used = set(figcrop.used_paths(figcrop.SCAN_RE))
        registered = {f["file"] for f in figcrop.load_registry()["figures"]}
        self.assertEqual(registered - used, set(), "有裁图登记了但书稿没用")


class TestDanglingExerciseFigures(unittest.TestCase):
    """判据本身：题面点名一张书里没有的图，必须被抓出来；写成「原书图」则放过。"""

    def run_on(self, body):
        with tempfile.TemporaryDirectory() as tmp:
            book = Path(tmp)
            (book / "ch07-graph.md").write_text(body, encoding="utf-8")
            original = figcrop.BOOK
            figcrop.BOOK = book
            try:
                return figcrop.dangling_exercise_figures()
            finally:
                figcrop.BOOK = original

    def test_missing_figure_in_exercise_is_caught(self):
        found = self.run_on("## 习题\n\n1. 对于图 7.26 所示的带权有向图，写出其相邻矩阵。\n")
        self.assertEqual([f[2] for f in found], ["图7.26"])

    def test_figure_drawn_anywhere_in_the_chapter_counts(self):
        body = ("## 7.3 图的存储结构\n\n![图 7.26 图例](assets/scan/fig-7-26.png)\n\n"
                "## 习题\n\n1. 对于图 7.26 所示的带权有向图，写出其相邻矩阵。\n")
        self.assertEqual(self.run_on(body), [])

    def test_original_book_reference_is_the_escape_hatch(self):
        found = self.run_on("## 上机题\n\n1. 用原书图 9.2 的输入做守门测试。\n")
        self.assertEqual(found, [])

    def test_body_sections_are_not_policed(self):
        """正文可以把插图改排成表格，这是本项目的常态，不该红。"""
        found = self.run_on("## 7.3 图的存储结构\n\n图 7.10 的相邻矩阵改排成了表格。\n")
        self.assertEqual(found, [])


if __name__ == "__main__":
    unittest.main()
