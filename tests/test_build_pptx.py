"""课件 .pptx 的自测。

这份产物有两处特别容易「看起来对、其实错」，用例就守这两处：

1. **它是个二进制包，坏了不一定报错。** 少一份 XML 部件、少一条关系，PowerPoint 会
   弹「需要修复」，而构建脚本一声不吭。所以这里把 zip 拆开，逐条核对必需的部件、
   关系与 content-type，而不是只看「文件生成了没有」。
2. **投影没有滚动条。** 一页排不下时如果悄悄溢出，讲台上不会有人发现——学生只是
   看不到最后三行。所以排版必须要么缩得下、要么拆成「…（续）」，用例钉死这一条。

另外还钉住了 TeX → Unicode 的几个判据：公式是内容，不是装饰，`$O(n\\log n)$`
在幻灯片上不能印成一串反斜杠。
"""
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import build_pptx  # noqa: E402
import pptx_writer as W  # noqa: E402


class TestTexToText(unittest.TestCase):
    """公式是内容。转不了的宏可以退化，但不能丢，也不能把 `\\` 印在屏幕上。"""

    def check(self, tex, want):
        self.assertEqual(build_pptx.tex_to_text(tex), want)

    def test_common_complexity_forms(self):
        self.check(r"O(n\log n)", "O(n log n)")
        self.check(r"\Theta(n^2)", "Θ(n²)")
        self.check(r"\Omega(n\log n)", "Ω(n log n)")

    def test_ceiling_floor_and_fractions(self):
        self.check(r"\lceil n/2\rceil", "⌈n/2⌉")
        self.check(r"\lfloor (i-1)/2 \rfloor", "⌊(i-1)/2⌋")
        self.check(r"\frac{n+1}{2}", "(n+1)/2")
        self.check(r"O(\sqrt{n})", "O(√n)")

    def test_subscripts_and_superscripts(self):
        self.check(r"\log_2 n", "log₂ n")
        self.check(r"2^{h}", "2ʰ")
        self.check(r"k_{i+1}", "kᵢ₊₁")

    def test_relations_get_breathing_room(self):
        self.check(r"2^i\le n", "2ⁱ ≤ n")
        self.check(r"x\in S", "x ∈ S")

    def test_unknown_macro_degrades_to_its_name(self):
        """认不出的宏按名字印出来并登记——**宁可难看，不可丢内容**。"""
        seen = set()
        self.assertEqual(build_pptx.tex_to_text(r"\wibble x", seen), "wibble x")
        self.assertEqual(seen, {"\\wibble"})

    def test_no_backslash_survives_the_real_decks(self):
        """全部课件里的行内公式，转换后都不该再带反斜杠。"""
        import re
        leftovers = []
        for path in build_pptx.sources():
            for tex in re.findall(r"\$([^$]+)\$", path.read_text(encoding="utf-8")):
                out = build_pptx.tex_to_text(tex)
                if "\\" in out:
                    leftovers.append((path.name, tex, out))
        self.assertEqual(leftovers, [])


class TestInline(unittest.TestCase):
    def test_bold_wrapping_math_is_parsed_through(self):
        """`**下界是 $\\Omega(n\\log n)$**`——粗体里套公式，曾经把美元符原样印上屏幕。"""
        runs = build_pptx.inline_runs(r"**下界是 $\Omega(n\log n)$**")
        self.assertTrue(all(run.bold for run in runs))
        self.assertNotIn("$", "".join(run.text for run in runs))
        self.assertIn("Ω(n log n)", "".join(run.text for run in runs))

    def test_inline_code_becomes_a_mono_run(self):
        runs = build_pptx.inline_runs("用 `std::optional` 表达")
        self.assertTrue(any(run.mono and run.text == "std::optional" for run in runs))

    def test_link_keeps_the_label_and_drops_the_url(self):
        runs = build_pptx.inline_runs("见 [第 3 章](ch03-stack.md) 的说明")
        joined = "".join(run.text for run in runs)
        self.assertIn("第 3 章", joined)
        self.assertNotIn("ch03-stack.md", joined)


class TestBlocks(unittest.TestCase):
    def test_ordered_list_keeps_its_numbers(self):
        """序号是内容：「第 3 步」不能变成一个圆点。"""
        blocks = build_pptx.parse_blocks(["1. 先建堆", "2. 再逐个弹出"])
        self.assertEqual(blocks[0][0], "list")
        self.assertEqual([marker for _, marker, _ in blocks[0][1]], ["1.", "2."])

    def test_fence_content_is_not_split_into_blocks(self):
        blocks = build_pptx.parse_blocks(
            ["```cpp file=code/x.hpp#a", "int a = 1;", "// - 不是列表", "```"])
        self.assertEqual(len(blocks), 1)
        self.assertEqual(blocks[0][0], "code")
        self.assertEqual(blocks[0][2], "code/x.hpp#a")
        self.assertIn("// - 不是列表", blocks[0][3])

    def test_table_is_recognised_with_alignments(self):
        blocks = build_pptx.parse_blocks(
            ["| 运算 | 代价 |", "| --- | ---: |", "| push | O(1) |"])
        self.assertEqual(blocks[0][0], "table")
        self.assertEqual(blocks[0][2], ["l", "r"])


class TestImageHeader(unittest.TestCase):
    """图的尺寸只读文件头——排版要按比例缩放，不能把图拉变形。"""

    def test_png_dimensions(self):
        sample = next((ROOT / "book" / "assets" / "scan").glob("*.png"))
        width, height = build_pptx.image_size(sample.read_bytes())
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)

    def test_unknown_format_raises(self):
        with self.assertRaises(ValueError):
            build_pptx.image_size(b"not an image at all")


class TestPackage(unittest.TestCase):
    """坏掉的 .pptx 不会自己喊疼，所以逐条核部件与关系。"""

    def deck(self):
        return sorted(build_pptx.OUT_DIR.glob("*.pptx"))

    def test_decks_exist(self):
        self.assertEqual(len(self.deck()), len(build_pptx.sources()))

    def test_every_package_has_the_required_parts(self):
        for path in self.deck():
            with zipfile.ZipFile(path) as pack:
                names = set(pack.namelist())
            for required in ("[Content_Types].xml", "_rels/.rels",
                             "ppt/presentation.xml", "ppt/_rels/presentation.xml.rels",
                             "ppt/theme/theme1.xml",
                             "ppt/slideMasters/slideMaster1.xml",
                             "ppt/slideLayouts/slideLayout1.xml",
                             "ppt/notesMasters/notesMaster1.xml"):
                self.assertIn(required, names, f"{path.name} 缺 {required}")
            self.assertTrue(any(n.startswith("ppt/slides/slide") for n in names))

    def test_every_slide_has_rels_and_a_content_type(self):
        for path in self.deck():
            with zipfile.ZipFile(path) as pack:
                names = set(pack.namelist())
                types = pack.read("[Content_Types].xml").decode("utf-8")
            slides = sorted(n for n in names
                            if n.startswith("ppt/slides/slide") and n.endswith(".xml"))
            for slide in slides:
                rels = slide.replace("ppt/slides/", "ppt/slides/_rels/") + ".rels"
                self.assertIn(rels, names, f"{path.name}: {slide} 没有关系文件")
                self.assertIn(f'PartName="/{slide}"', types,
                              f"{path.name}: {slide} 没有登记 content-type")

    def test_every_referenced_image_is_inside_the_package(self):
        """图是嵌进去的，不是链接——发出去的课件在别人机器上不能变成红叉。"""
        import re
        for path in self.deck():
            with zipfile.ZipFile(path) as pack:
                names = set(pack.namelist())
                for name in names:
                    if not name.startswith("ppt/slides/_rels/"):
                        continue
                    body = pack.read(name).decode("utf-8")
                    for target in re.findall(r'Target="\.\./media/([^"]+)"', body):
                        self.assertIn(f"ppt/media/{target}", names,
                                      f"{path.name}: 引用了不存在的 {target}")

    def test_notes_are_carried_over(self):
        """讲稿是课件的一半价值，不能在转 .pptx 的时候丢掉。"""
        with zipfile.ZipFile(build_pptx.OUT_DIR / "ch03-stack.pptx") as pack:
            notes = [n for n in pack.namelist() if n.startswith("ppt/notesSlides/notesSlide")]
        self.assertGreater(len(notes), 10)

    def test_build_is_reproducible(self):
        """同样的源产出同样的字节——否则 `--check` 只能靠 sidecar，改一行就得重排全书。"""
        first = (build_pptx.OUT_DIR / "ch03-stack.pptx").read_bytes()
        with tempfile.TemporaryDirectory() as tmp:
            original = build_pptx.OUT_DIR
            build_pptx.OUT_DIR = Path(tmp)
            try:
                out, _ = build_pptx.build_deck(build_pptx.SLIDES / "ch03-stack.md",
                                               build_pptx.Ctx())
                self.assertEqual(out.read_bytes(), first)
            finally:
                build_pptx.OUT_DIR = original


class TestPagination(unittest.TestCase):
    """投影没有滚动条：装不下就必须拆页，不能溢出。"""

    def render(self, lines):
        ctx = build_pptx.Ctx()
        blocks = build_pptx.parse_blocks(lines)
        slides = build_pptx.build_slide(blocks, [], "测试", "", 1, 1, ctx, {}, {})
        return slides, ctx

    def test_a_short_page_stays_one_slide(self):
        slides, ctx = self.render(["# 标题", "", "- 一条", "- 两条"])
        self.assertEqual(len(slides), 1)
        self.assertEqual(ctx.crowded, [])

    def test_an_overlong_page_is_split_not_clipped(self):
        lines = ["# 很长的一页", ""] + [f"- 第 {n} 条要点，写得足够长以便占满整整一行的宽度"
                                        for n in range(40)]
        slides, ctx = self.render(lines)
        self.assertGreater(len(slides), 1, "装不下就该拆页")
        self.assertEqual(ctx.crowded, [], "拆完之后不该再有溢出")
        self.assertEqual(len(ctx.split), 1)

    def test_continuation_slides_are_marked(self):
        lines = ["# 原标题", ""] + [f"- 第 {n} 条要点，写得足够长以便占满整整一行的宽度"
                                    for n in range(40)]
        slides, _ = self.render(lines)
        titles = []
        for slide in slides:
            for shape in slide.shapes:
                if getattr(shape, "name", "") == "标题":
                    titles.append("".join(run.text for para in shape.paras
                                          for run in para.runs))
        self.assertEqual(titles[0], "原标题")
        self.assertTrue(all(title.endswith("（续）") for title in titles[1:]))

    def test_real_decks_have_no_overflow(self):
        """入库的这一版：410 页里一页都不许溢出。"""
        ctx = build_pptx.Ctx()
        for path in build_pptx.sources():
            meta, text = __import__("build_slides").split_front_matter(
                path.read_text(encoding="utf-8"))
            for number, raw in enumerate(__import__("build_slides").split_slides(text), 1):
                lines, _ = __import__("build_slides").take_notes(raw)
                build_pptx.build_slide(build_pptx.parse_blocks(lines), [],
                                       meta.get("title", ""), meta.get("subtitle", ""),
                                       number, 1, ctx, {}, {}, cover=(number == 1))
        self.assertEqual(ctx.crowded, [])


class TestGate(unittest.TestCase):
    def test_check_agrees_with_the_committed_output(self):
        self.assertEqual(build_pptx.build(check_only=True), 0)


if __name__ == "__main__":
    unittest.main()
