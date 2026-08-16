"""插图收集器的单元测试。

这里测的是**题注归属**：一张图的 alt 写错了，图册和书稿里的无障碍文本就是错的，
而且没有任何编译器会报。原书的图是浮动的，排版时正文绕着图走；OCR 把版面展平
之后题注常常被顶到图后面好几行——跨行去找它，就有认错的风险，所以判据要被钉住。
"""
import sys
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


if __name__ == "__main__":
    unittest.main()
