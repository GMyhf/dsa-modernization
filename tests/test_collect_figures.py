"""插图收集器的单元测试。

这里测的是**题注归属**：一张图的 alt 写错了，图册和书稿里的无障碍文本就是错的，
而且没有任何编译器会报。原书的图是浮动的，排版时正文绕着图走；OCR 把版面展平
之后题注常常被顶到图后面好几行——跨行去找它，就有认错的风险，所以判据要被钉住。
"""
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import collect_figures  # noqa: E402


IMG = "![](https://raw.githubusercontent.com/GMyhf/img/main/img/deadbeef.jpg)"
IMG2 = "![](https://raw.githubusercontent.com/GMyhf/img/main/img/cafebabe.jpg)"


def recover(lines):
    return collect_figures.recover_caption(lines, 0)


class TestCaptionRecovery(unittest.TestCase):
    def test_recovers_caption_separated_by_a_spilled_label(self):
        """图1.4 就是这种：中间隔着一行从图里抠出来的「存储区的数据」。"""
        lines = [IMG, "存储区的数据", "", "图 1.4 索引示例", ""]
        self.assertEqual(recover(lines), ["图 1.4 索引示例"])

    def test_recovers_caption_separated_by_body_prose(self):
        """图7.1 是这种：中间隔着两段正文，题注仍然属于上面那张图。"""
        lines = [
            IMG,
            "",
            "下面详细介绍图的定义和基本术语。",
            "",
            "简单地说，图由表示数据元素的集合V和表示数据之间关系的集合E组成。",
            "",
            "图 7.1 用图描述通信网络",
            "",
        ]
        self.assertEqual(recover(lines), ["图 7.1 用图描述通信网络"])

    def test_rejects_caption_that_belongs_to_the_next_image(self):
        """底稿第 6471 行「图 7.23 带权图」的下一行就是图 7.23 本身。

        少了这条判据，它会被错记到上一张图头上——**这是本测试存在的主要理由**。
        """
        lines = [IMG, "", "一些正文。", "", "图 7.23 带权图", IMG2, ""]
        self.assertEqual(recover(lines), [])

    def test_stops_at_the_next_image(self):
        lines = [IMG, "", IMG2, "", "图 9.9 后面那张图的题注", ""]
        self.assertEqual(recover(lines), [])

    def test_stops_at_a_heading(self):
        lines = [IMG, "", "# 第7章 图", "", "图 7.1 不该被上一章的图认领", ""]
        self.assertEqual(recover(lines), [])

    def test_does_not_reach_beyond_the_lookahead_window(self):
        """距离写死成 12，**不要**写成 CAPTION_LOOKAHEAD + 2。

        用常量算距离的话，常量被改大时这条断言会跟着变松，等于没测。
        """
        lines = [IMG] + [""] * 12 + ["图 3.3 太远了"]
        self.assertEqual(recover(lines), [])

    def test_lookahead_window_is_pinned(self):
        """8 这个数是量出来的，不是随手写的。

        放宽窗口就会够到更靠后的题注，而越靠后越可能是别的图的。
        真要改，得把全书 292 张图重新逐张核对一遍——所以先在这里绊一下。
        """
        self.assertEqual(collect_figures.CAPTION_LOOKAHEAD, 8)

    def test_ignores_loose_mentions_that_are_not_numbered_captions(self):
        """正文里的「图示」「如图所示」不是题注体例，不能拿来当 alt。"""
        lines = [IMG, "", "图示如下，可参考后文。", "", ""]
        self.assertEqual(recover(lines), [])

    def test_ignores_subfigure_labels(self):
        """`(c) 嵌套括号表示法` 这类是**子图标号**，不是整张图的题注。

        收集器的 CAPTION_RE 认这种形式（相邻行判据要用它），但跨行找回时不能认——
        隔了几行之后再看到一个 `(b)`，它多半属于别处。这条盯着的就是这个区别。
        """
        lines = [IMG, "", "一些正文。", "", "(b) 中间的一步", ""]
        self.assertEqual(recover(lines), [])
        self.assertTrue(
            collect_figures.CAPTION_RE.match("(b) 中间的一步"),
            "前提：相邻行判据确实认这种形式，所以跨行判据必须自己更严",
        )


class TestAgainstTheRealManuscript(unittest.TestCase):
    """拿只读底稿当锚：判据松了会多认，紧了会漏认，这两个数字都盯着。"""

    @classmethod
    def setUpClass(cls):
        cls.figures = collect_figures.collect()
        cls.by_line = {f["line"]: f for f in cls.figures}

    def test_total_figure_count_is_stable(self):
        self.assertEqual(len(self.figures), 292)

    def test_figure_1_4_caption_is_recovered(self):
        self.assertEqual(self.by_line[659]["alt"], "图 1.4 索引示例")

    def test_figure_7_17_caption_is_recovered_across_prose(self):
        self.assertEqual(self.by_line[6214]["alt"], "图 7.17 表示课程优先关系的有向无环图")

    def test_figure_7_23_caption_is_not_stolen_by_the_previous_image(self):
        """底稿第 6463 行那张图**没有**题注；6471 行的题注属于 6472 行那张。"""
        self.assertIn("原书无独立题注", self.by_line[6463]["alt"])

    def test_recovered_count_is_pinned(self):
        """判据一放松，这个数就会涨——涨了要有人重新逐张核对，而不是默认接受。"""
        untitled = [f for f in self.figures if "原书无独立题注" in f["alt"]]
        self.assertEqual(len(untitled), 26)


class TestOfflineCheck(unittest.TestCase):
    """`--check`：不联网也要能证明「图册还是底稿那批图」。

    **缘由**（2026-08-17 派生产物新鲜度盘点）：`collect_figures.py` 原来只有写入和
    `--dry-run`，重新生成还依赖网络，于是底稿题注变了、图册漏了一张、或者某个 jpg 被换掉，
    闸门都不会响——网站、课件、PDF 三条线都有 `--check`，唯独插图集没有。
    判据全部落在本地：底稿现在抽出的顺序与题注、sidecar 记的对应关系、图册的引用顺序，
    以及**文件名是不是等于文件内容的 SHA-256 前 16 位**。
    """

    def sandbox(self, tmp, alt="图 1.1 示意", body=b"\xff\xd8jpeg"):
        import hashlib
        import json

        root = Path(tmp)
        assets = root / "assets"
        assets.mkdir()
        name = hashlib.sha256(body).hexdigest()[:16] + ".jpg"
        (assets / name).write_bytes(body)
        raw = root / "raw.md"
        raw.write_text(
            "# 第1章 概论\n\n![](https://raw.githubusercontent.com/GMyhf/img/main/img/a.jpg)  \n"
            f"{alt}\n",
            encoding="utf-8",
        )
        atlas = root / "atlas.md"
        atlas.write_text(f"# 原书插图\n\n![{alt}](assets/{name})\n\n{alt}\n", encoding="utf-8")
        sidecar = assets / "figures.json"
        sidecar.write_text(json.dumps({"figures": [
            {"chapter": 1, "raw_line": 3,
             "url": "https://raw.githubusercontent.com/GMyhf/img/main/img/a.jpg",
             "file": f"assets/{name}", "alt": alt}]}, ensure_ascii=False), encoding="utf-8")
        return root, assets / name, atlas, sidecar, raw

    def run_check(self, root, raw, atlas, assets, sidecar):
        import contextlib
        import io

        saved = (collect_figures.RAW, collect_figures.OUT,
                 collect_figures.ASSETS, collect_figures.SIDECAR, collect_figures.ROOT)
        try:
            collect_figures.RAW = raw
            collect_figures.OUT = atlas
            collect_figures.ASSETS = assets
            collect_figures.SIDECAR = sidecar
            collect_figures.ROOT = root
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                result = collect_figures.check_current()
            return result, out.getvalue()
        finally:
            (collect_figures.RAW, collect_figures.OUT, collect_figures.ASSETS,
             collect_figures.SIDECAR, collect_figures.ROOT) = saved

    def test_consistent_atlas_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root, figure, atlas, sidecar, raw = self.sandbox(tmp)
            result, out = self.run_check(root, raw, atlas, figure.parent, sidecar)
        self.assertEqual(result, 0, out)
        self.assertIn("逐项相符", out)

    def test_replaced_image_bytes_are_caught(self):
        """图被换掉：文件名还在，内容变了——这是 vendor 重下一张图的典型后果。"""
        with tempfile.TemporaryDirectory() as tmp:
            root, figure, atlas, sidecar, raw = self.sandbox(tmp)
            figure.write_bytes(b"\xff\xd8different")
            result, out = self.run_check(root, raw, atlas, figure.parent, sidecar)
        self.assertEqual(result, 1)
        self.assertIn("内容与文件名对不上", out)

    def test_caption_change_in_the_manuscript_is_caught(self):
        """底稿题注改了而图册没跟上——图册的 alt 正是原书题注。"""
        with tempfile.TemporaryDirectory() as tmp:
            root, figure, atlas, sidecar, raw = self.sandbox(tmp)
            raw.write_text(raw.read_text(encoding="utf-8").replace("示意", "改过的题注"),
                           encoding="utf-8")
            result, out = self.run_check(root, raw, atlas, figure.parent, sidecar)
        self.assertEqual(result, 1)
        self.assertIn("题注变了", out)

    def test_missing_file_is_caught(self):
        with tempfile.TemporaryDirectory() as tmp:
            root, figure, atlas, sidecar, raw = self.sandbox(tmp)
            figure.unlink()
            result, out = self.run_check(root, raw, atlas, figure.parent, sidecar)
        self.assertEqual(result, 1)
        self.assertIn("不存在", out)

    def test_orphan_asset_is_caught(self):
        """没人引用的图片也要报——它多半是上一版留下的，白占仓库。"""
        with tempfile.TemporaryDirectory() as tmp:
            root, figure, atlas, sidecar, raw = self.sandbox(tmp)
            (figure.parent / "0123456789abcdef.jpg").write_bytes(b"orphan")
            result, out = self.run_check(root, raw, atlas, figure.parent, sidecar)
        self.assertEqual(result, 1)
        self.assertIn("没人引用", out)

    def test_committed_atlas_passes(self):
        """入库的 292 张当锚：以后谁换掉一张图或改了底稿题注，这里就红。"""
        import contextlib
        import io

        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            result = collect_figures.check_current()
        self.assertEqual(result, 0, out.getvalue())
        self.assertIn("292 张图", out.getvalue())

if __name__ == "__main__":
    unittest.main()
