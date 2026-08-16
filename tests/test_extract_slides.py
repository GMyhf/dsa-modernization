"""课件文本抽取器的单元测试。

这里测的不是「课件里写了什么」——那是参考资料。这里测的是**抽取本身别抽错**：
抽出来的文本会被人拿去和新书逐章对照（2026-08-16 那一轮就靠它查出一条台账造假），
一旦页号错位或备注张冠李戴，对照结论就是错的，而且没有任何东西会报警。

最容易被「顺手简化」掉的是备注的关系映射：`slide7.xml` 的备注**不一定**是
`notesSlide7.xml`——只有带备注的页才生成备注页，两边编号各自连续。
下面 `test_notes_are_matched_through_relationships` 专门盯着这一条。
"""
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import extract_slides  # noqa: E402


A = "http://schemas.openxmlformats.org/drawingml/2006/main"
REL_NS = "http://schemas.openxmlformats.org/package/2006/relationships"


def slide_xml(*paragraphs):
    # 必须转义：`<number>` 这种占位符里带尖括号，直接塞进去会把 XML 写坏。
    body = "".join(
        f'<a:p xmlns:a="{A}"><a:r><a:t>{escape(text)}</a:t></a:r></a:p>'
        for text in paragraphs
    )
    return f'<?xml version="1.0"?><root xmlns:a="{A}">{body}</root>'.encode("utf-8")


def rels_xml(notes_target=None):
    rel = ""
    if notes_target:
        rel = (
            f'<Relationship Id="rId1" Target="../notesSlides/{notes_target}" '
            'Type="http://schemas.openxmlformats.org/officeDocument/2006/'
            'relationships/notesSlide"/>'
        )
    return f'<?xml version="1.0"?><Relationships xmlns="{REL_NS}">{rel}</Relationships>'.encode(
        "utf-8"
    )


def make_pptx(path: Path, slides):
    """slides: [(段落列表, 备注段落列表 或 None), ...]"""
    with zipfile.ZipFile(path, "w") as z:
        notes_index = 0
        for index, (body, notes) in enumerate(slides, 1):
            z.writestr(f"ppt/slides/slide{index}.xml", slide_xml(*body))
            if notes is None:
                z.writestr(f"ppt/slides/_rels/slide{index}.xml.rels", rels_xml())
            else:
                notes_index += 1
                name = f"notesSlide{notes_index}.xml"
                z.writestr(f"ppt/slides/_rels/slide{index}.xml.rels", rels_xml(name))
                z.writestr(f"ppt/notesSlides/{name}", slide_xml(*notes))


class TestExtraction(unittest.TestCase):
    def extract(self, slides, name="deck.pptx"):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / name
            make_pptx(path, slides)
            return extract_slides.extract(path)

    def test_slides_keep_document_order(self):
        text = self.extract([(["第一页"], None), (["第二页"], None), (["第三页"], None)])
        self.assertLess(text.index("第一页"), text.index("第二页"))
        self.assertLess(text.index("第二页"), text.index("第三页"))
        self.assertIn("===== 幻灯片 2 =====", text)

    def test_slide_numbering_is_not_lexicographic(self):
        """10 页以上时按 slide10 < slide9 排序就错位了——页号是要被引用的。"""
        text = self.extract([([f"页{i}"], None) for i in range(1, 13)])
        self.assertLess(text.index("页9"), text.index("页10"))
        self.assertIn("===== 幻灯片 12 =====", text)

    def test_notes_are_matched_through_relationships(self):
        """**本文件最重要的一条。**

        只有第 2、3 页有备注，于是它们的备注分别是 notesSlide1 与 notesSlide2——
        编号和页号对不上。若把实现改成「slideN 配 notesSlideN」，
        第 2 页就会拿到 notesSlide2（其实属于第 3 页）的内容，而第 3 页没有备注。
        """
        text = self.extract(
            [
                (["无备注的第一页"], None),
                (["第二页"], ["这是第二页的讲稿"]),
                (["第三页"], ["这是第三页的讲稿"]),
            ]
        )
        second = text.index("第二页")
        third = text.index("第三页")
        self.assertLess(second, text.index("这是第二页的讲稿"))
        self.assertLess(text.index("这是第二页的讲稿"), third)
        self.assertLess(third, text.index("这是第三页的讲稿"))

    def test_slide_without_notes_gets_no_notes_section(self):
        # 断言认的是分隔线，不是「演讲者备注」四个字——文件头部本来就写着这几个字。
        text = self.extract([(["只有正文"], None)])
        self.assertNotIn("--- 演讲者备注 ---", text)

    def test_field_placeholders_are_dropped_everywhere(self):
        """母版的页码域抽出来是 `<number>`，正文和备注里都有，都不是内容。

        本批课件正文 1194 处、备注 263 处；不滤掉的话每一页都以它开头。
        """
        text = self.extract([(["<number>", "正文"], ["<number>", "真正的讲稿"])])
        self.assertIn("正文", text)
        self.assertIn("真正的讲稿", text)
        self.assertNotIn("<number>", text)

    def test_bare_numbers_are_dropped_from_notes_only(self):
        """裸数字的页码占位符只在备注里丢。

        **正文里不能丢**：孤立数字往往是图表里的一格（图1.4 那些索引值就是），
        丢了对照时就少了内容。这一条盯着的正是这个区别。
        """
        text = self.extract([(["37", "42", "线性索引"], ["17", "真正的讲稿"])])
        self.assertIn("37", text)
        self.assertIn("42", text)
        self.assertNotIn("\n17\n", text)

    def test_runs_inside_one_paragraph_are_joined(self):
        """PowerPoint 会把一句话按格式切成多个 run；不拼起来就会碎成好几行。"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "deck.pptx"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr(
                    "ppt/slides/slide1.xml",
                    f'<?xml version="1.0"?><root xmlns:a="{A}">'
                    f'<a:p><a:r><a:t>重量</a:t></a:r><a:r><a:t>权衡</a:t></a:r>'
                    f"<a:r><a:t>合并规则</a:t></a:r></a:p></root>".encode("utf-8"),
                )
                z.writestr("ppt/slides/_rels/slide1.xml.rels", rels_xml())
            text = extract_slides.extract(path)
        self.assertIn("重量权衡合并规则", text)

    def test_empty_paragraphs_are_dropped(self):
        text = self.extract([(["有内容", "", "   "], None)])
        self.assertEqual(text.count("===== 幻灯片"), 1)
        self.assertNotIn("\n\n\n", text)

    def test_header_records_provenance(self):
        """文本是产物，头部必须写清「原始课件才是依据」，否则会被当成一手材料引用。"""
        text = self.extract([(["x"], None)], name="第7章 图.pptx")
        self.assertIn("第7章 图.pptx", text)
        self.assertIn("extract_slides.py", text)
        self.assertIn("原始课件才是依据", text)


class TestAgainstTheCommittedText(unittest.TestCase):
    """入库的那批文本本身也当锚：抽取器改坏了，这里立刻能看出来。"""

    DIR = ROOT / "ref_数据结构与算法A 2021秋" / "课件文本"

    @unittest.skipUnless(DIR.is_dir(), "参考目录不在（浅克隆时可能没有）")
    def test_all_nineteen_decks_are_present(self):
        files = sorted(self.DIR.glob("*.txt"))
        self.assertEqual(len(files), 19, [f.name for f in files])

    @unittest.skipUnless(DIR.is_dir(), "参考目录不在")
    def test_notes_survived_extraction(self):
        """备注是这批文件里最值钱的部分。数字掉下来说明抽取器漏了东西。"""
        total = sum(
            f.read_text(encoding="utf-8").count("--- 演讲者备注 ---")
            for f in self.DIR.glob("*.txt")
        )
        self.assertEqual(total, 122)

    @unittest.skipUnless(DIR.is_dir(), "参考目录不在")
    def test_no_placeholder_noise_left(self):
        for f in self.DIR.glob("*.txt"):
            self.assertNotIn("<number>", f.read_text(encoding="utf-8"), f.name)


if __name__ == "__main__":
    unittest.main()
