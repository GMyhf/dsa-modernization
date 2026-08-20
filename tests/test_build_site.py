"""网页版构建器的单元测试。

这份构建器有两个**不出声就会坏**的地方，每条自检都必须有一个「会红」的用例：

1. **代码块必须逐字通过渲染**。书稿的 ```cpp 块与 `code/` 下源码逐字一致是 R3 的契约；
   渲染时高亮只许加标签、不许改字节，否则读者照着网页抄下来的代码编译不过，
   而 R3 检查的是 Markdown、根本看不见 HTML 这一层。
2. **站内链接必须落到真实锚点**。书稿改个标题、网页就多一条死链，点了才知道。
"""
import contextlib
import io
import re
import sys
import unittest
import urllib.parse
from html import unescape
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import build_site  # noqa: E402


def strip_tags(markup):
    return unescape(re.sub(r"<[^>]+>", "", markup))


def render(markdown, ctx=None):
    ctx = ctx or build_site.Context()
    body, headings = render_with_headings(markdown, ctx)
    return body


def render_with_headings(markdown, ctx):
    return build_site.render_blocks(markdown.split("\n"), ctx, build_site.Anchors())


class TestCodeFidelity(unittest.TestCase):
    """高亮只加标签，不改一个字节。"""

    SAMPLE = ('#include <memory>\n'
              '// 注释里有 <尖括号> 与 "引号"\n'
              'template <typename T>\n'
              'class ArrayStack {\n'
              '    T* data_ = nullptr;   /* 裸指针 */\n'
              '    std::size_t n_ = 0x1F;\n'
              '};\n')

    def test_highlight_preserves_every_byte(self):
        self.assertEqual(strip_tags(build_site.highlight_cpp(self.SAMPLE)), self.SAMPLE)

    def test_highlight_actually_marks_keywords(self):
        # 反面用例：如果高亮什么都不做，上一条测试也会绿，所以这里盯住它真的干了活
        self.assertIn('<span class="k">class</span>', build_site.highlight_cpp(self.SAMPLE))

    def test_mangled_code_would_go_red(self):
        # 变异自检：假装高亮吃掉了一个分号，第一条断言必须红
        broken = build_site.highlight_cpp(self.SAMPLE).replace("nullptr", "", 1)
        self.assertNotEqual(strip_tags(broken), self.SAMPLE)

    def test_every_book_code_block_survives_rendering(self):
        """全书 180 个代码块逐字比对渲染前后。"""
        checked = 0
        for md, out, _ in build_site.PAGES:
            source = (build_site.BOOK / md).read_text(encoding="utf-8")
            blocks, lines, index = [], source.split("\n"), 0
            while index < len(lines):
                if lines[index].startswith("```"):
                    index += 1
                    body = []
                    while index < len(lines) and not lines[index].startswith("```"):
                        body.append(lines[index])
                        index += 1
                    blocks.append("\n".join(body))
                index += 1
            rendered = render(source)
            printed = [strip_tags(m) for m in
                       re.findall(r"<pre><code>(.*?)</code></pre>", rendered, re.S)]
            self.assertEqual(len(printed), len(blocks), f"{md} 的代码块数量对不上")
            for original, shown in zip(blocks, printed):
                self.assertEqual(shown, original, f"{md} 的代码块在渲染后变了字节")
            checked += len(blocks)
        self.assertGreater(checked, 100, "全书代码块少于 100 个，说明扫描本身出了问题")


class TestLinks(unittest.TestCase):
    def test_sibling_chapter_becomes_html(self):
        self.assertEqual(build_site.rewrite_href("ch01-adt.md", None), "ch01-adt.html")
        self.assertEqual(build_site.rewrite_href("勘误.md#怎么读", None), "errata.html#怎么读")

    def test_repo_file_becomes_github_link(self):
        self.assertEqual(build_site.rewrite_href("../code/ch03/array_stack/modern.hpp", None),
                         build_site.REPO_BLOB + "code/ch03/array_stack/modern.hpp")

    def test_asset_path_climbs_out_of_site_dir(self):
        ctx = build_site.Context()
        ctx.page = "figures.html"
        self.assertIn('src="../assets/x.jpg"', build_site.image_tag("图", "assets/x.jpg", ctx))

    def test_assets_href_can_be_moved_to_publish_root(self):
        """发布到 GitHub Pages 时页面摆在根上，图片前缀要跟着改（见 D-011）。"""
        ctx = build_site.Context()
        ctx.page = "figures.html"
        original = build_site.ASSETS_HREF
        try:
            build_site.ASSETS_HREF = "assets/"
            self.assertIn('src="assets/x.jpg"', build_site.image_tag("图", "assets/x.jpg", ctx))
        finally:
            build_site.ASSETS_HREF = original
        self.assertIn('src="../assets/x.jpg"', build_site.image_tag("图", "assets/x.jpg", ctx))

    def test_missing_anchor_is_reported(self):
        ctx = build_site.Context()
        ctx.page = "ch01-adt.html"
        render("见 [第二章](ch02-linear-list.md#不存在的小节)。", ctx)
        ctx.anchors_by_page = {"ch01-adt.html": set(), "ch02-linear-list.html": {"21-概念"}}
        problems = build_site.find_problems(ctx)
        self.assertEqual(len(problems), 1)
        self.assertIn("锚点", problems[0])

    def test_live_anchor_passes(self):
        ctx = build_site.Context()
        ctx.page = "ch01-adt.html"
        render("见 [第二章](ch02-linear-list.md#21-概念)。", ctx)
        ctx.anchors_by_page = {"ch01-adt.html": set(), "ch02-linear-list.html": {"21-概念"}}
        self.assertEqual(build_site.find_problems(ctx), [])

    def test_missing_asset_is_reported(self):
        ctx = build_site.Context()
        ctx.page = "figures.html"
        render("![图](assets/根本没有这张图.jpg)", ctx)
        ctx.anchors_by_page = {"figures.html": set()}
        self.assertEqual(len(build_site.find_problems(ctx)), 1)


class TestNestedInline(unittest.TestCase):
    """占位符会嵌套——链接标记里裹着行内代码，还原顺序错了就整段文字消失。

    这不是假想：首页那两条「递归深度风险见 `collab/UNVERIFIED-RISKS.md`」的链接
    曾经在网页上渲染成一个**空链接**，页面里留下的是四个 NUL 字节；NUL 在浏览器里
    不占地方，所以肉眼只看到「见 。」，而构建、check_doc、--check 全绿。
    """

    def nuls(self, markdown, ctx=None):
        ctx = ctx or build_site.Context()
        ctx.page = "x.html"
        return build_site.render_inline(markdown, ctx)

    def test_code_inside_link_label_survives(self):
        out = self.nuls("风险见 [`collab/UNVERIFIED-RISKS.md`](../collab/UNVERIFIED-RISKS.md)。")
        self.assertNotIn("\x00", out)
        self.assertIn("<code>collab/UNVERIFIED-RISKS.md</code></a>", out)

    def test_math_inside_link_label_survives(self):
        out = self.nuls("见 [第 $O(n)$ 节](ch01-adt.md#x)。")
        self.assertNotIn("\x00", out)
        self.assertIn('<span class="math">', out)

    def test_ascending_restore_would_go_red(self):
        """变异自检：把还原顺序改回从前往后，上面两条必须红。"""
        original = build_site.decorate

        def ascending(text, stash):
            text = build_site.html.escape(text)
            for index, markup in enumerate(stash):
                text = text.replace(f"\x00{index}\x00", markup)
            return text

        try:
            build_site.decorate = ascending
            broken = self.nuls("风险见 [`collab/UNVERIFIED-RISKS.md`](../collab/UNVERIFIED-RISKS.md)。")
        finally:
            build_site.decorate = original
        self.assertIn("\x00", broken)

    def test_inline_image_alt_is_plain_text(self):
        """alt 里只能是书稿原文：塞进 <span class="math"> 会把属性引号顶断。"""
        out = self.nuls("行内图 ![(a) 由 $T_1$ 构成](assets/x.jpg) 在句中。")
        self.assertNotIn("\x00", out)
        self.assertIn('alt="(a) 由 $T_1$ 构成"', out)
        self.assertNotIn('alt="(a) 由 <span', out)

    def test_no_page_carries_a_nul_byte(self):
        """仓库锚点：整本书稿加课件，渲染完不许留下一个 NUL。"""
        pages = sorted((ROOT / "book").glob("*.md")) + sorted((ROOT / "book" / "slides").glob("*.md"))
        self.assertGreater(len(pages), 20)
        for path in pages:
            ctx = build_site.Context()
            ctx.page = path.name
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                self.assertNotIn("\x00", build_site.render_inline(line, ctx),
                                 f"{path.name}:{number} 渲染后留下了占位符")


class TestMath(unittest.TestCase):
    def test_ocr_spacing_does_not_break_subscripts(self):
        """`a _ { 1 6 }` 与 `a_{16}` 同义 —— 不跳空格的话下标会退化成一个空格。"""
        spaced = build_site.render_math("a _ { 1 6 }")
        tight = build_site.render_math("a_{16}")
        self.assertEqual(strip_tags(spaced), strip_tags(tight))
        self.assertEqual(strip_tags(spaced), "a16")
        # 下标里必须真的是 16：丢了跳空格那一步，下标会变成一个空的 <sub>，
        # 正文看着还是 a16，只有这条断言看得出来
        self.assertEqual(strip_tags(re.search(r"<sub>(.*)</sub>", spaced).group(1)), "16")

    def test_fraction_and_ceiling(self):
        self.assertIn('class="frac"', build_site.render_math(r"\frac{n}{2}"))
        self.assertEqual(strip_tags(build_site.render_math(r"\lceil\log_2 n\rceil")),
                         "⌈log2n⌉")

    def test_unknown_command_is_kept_visible_and_recorded(self):
        unknown = set()
        markup = build_site.render_math(r"\wobble x", unknown)
        self.assertIn(r"\wobble", strip_tags(markup))   # 没有被静默吃掉
        self.assertIn("tex-raw", markup)                 # 而且在页面上看得出来
        self.assertEqual(unknown, {r"\wobble"})


class TestBlocks(unittest.TestCase):
    def test_table_alignment(self):
        markup = render("| 名 | 值 |\n| --- | ---: |\n| a | 1 |\n")
        self.assertIn('<td style="text-align:right">', markup)

    def test_list_continuation_line_joins_item(self):
        markup = render("- **测试是正文的一部分。** 每个实现目录的 `test.cpp`\n"
                        "  都覆盖其声明的清单。\n")
        self.assertEqual(markup.count("<li>"), 1)
        self.assertIn("都覆盖其声明的清单。", markup)

    def test_figure_caption_is_not_printed_twice(self):
        """图册的体例是图后跟一行与 alt 相同的题注；网页上应只出现一次。"""
        markup = render("![图 2.4 单链表示例](assets/x.jpg)\n\n图 2.4 单链表示例\n")
        self.assertEqual(markup.count("图 2.4 单链表示例"), 2)  # alt 属性 + figcaption
        self.assertNotIn("<p>", markup)

    def test_heading_anchor_is_github_style(self):
        ctx = build_site.Context()
        _, headings = render_with_headings("## 3.1 栈\n", ctx)
        self.assertEqual(headings, [(2, "3.1 栈", "31-栈")])

    def test_code_fence_records_its_source_file(self):
        markup = render("```cpp file=code/ch03/array_stack/modern.hpp#push\nint x;\n```\n")
        self.assertIn(build_site.REPO_BLOB + "code/ch03/array_stack/modern.hpp", markup)

    def test_inline_dollar_inside_code_is_not_math(self):
        markup = render("运行 `echo $PATH` 看看。\n")
        self.assertNotIn("math", markup)


class TestDownloadCard(unittest.TestCase):
    """首页的 PDF 下载卡片：数字来自文件本身，不手写。"""

    def test_card_reads_size_and_pages_from_disk(self):
        card = build_site.download_card()
        self.assertIn("下载完整教程", card)
        self.assertRegex(card, r"\d+ 页")           # 页数来自 build-info.json
        self.assertRegex(card, r"\d+\.\d+ MB")      # 体积来自 PDF 文件本身
        self.assertIn("12 章正文", card)              # 正文章数也来自 build-info.json

    def test_card_href_follows_publish_layout(self):
        original = build_site.PDF_HREF
        try:
            build_site.PDF_HREF = "数据结构与算法.pdf"
            card = build_site.download_card()
        finally:
            build_site.PDF_HREF = original
        # 中文文件名要转义，否则某些服务器上点了就是 404。
        # 不写死某个字的编码——更名一次就要改一次测试，那种测试守的是名字不是行为。
        self.assertIn(urllib.parse.quote("数据结构与算法.pdf"), card)
        self.assertNotIn('href="../pdf/', card)

    def test_no_pdf_means_no_card(self):
        """PDF 没排版时，宁可没有卡片，也不要一个点了 404 的链接。"""
        original = build_site.PDF_FILE
        try:
            build_site.PDF_FILE = build_site.BOOK / "pdf" / "根本没有这本.pdf"
            self.assertEqual(build_site.download_card(), "")
        finally:
            build_site.PDF_FILE = original

    def test_broken_sidecar_does_not_break_the_build(self):
        original = build_site.PDF_INFO
        try:
            build_site.PDF_INFO = Path(__file__)      # 不是 JSON
            card = build_site.download_card()
        finally:
            build_site.PDF_INFO = original
        self.assertIn("下载完整教程", card)
        self.assertNotRegex(card, r"\d+ 页")

    def test_card_only_on_the_cover_page(self):
        cover = (build_site.SITE / "index.html").read_text(encoding="utf-8")
        chapter = (build_site.SITE / "ch03-stack.html").read_text(encoding="utf-8")
        self.assertIn('class="download"', cover)
        self.assertNotIn('class="download"', chapter)


class TestGate(unittest.TestCase):
    """--check 必须在书稿改了、站点没重建时变红。"""

    def run_check(self):
        err = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            code = build_site.build(check_only=True)
        return code, err.getvalue()

    def test_repo_site_is_up_to_date(self):
        code, err = self.run_check()
        self.assertEqual(code, 0, f"book/site/ 与书稿不一致，请跑 tools/build_site.py：{err}")

    def test_stale_page_goes_red(self):
        page = build_site.SITE / "ch01-adt.html"
        original = page.read_text(encoding="utf-8")
        try:
            page.write_text(original.replace("</main>", "<p>手改的一行</p></main>"),
                            encoding="utf-8")
            code, err = self.run_check()
            self.assertEqual(code, 1)
            self.assertIn("ch01-adt.html", err)
        finally:
            page.write_text(original, encoding="utf-8")

class TestPythonCodeFidelity(unittest.TestCase):
    """D-025：Python 块的高亮同样只加标签，不改一个字节。"""

    SAMPLE = ('def demo(values: list[int]) -> None:\n'
              '    """文档串里有 <尖括号> 与 \'引号\'"""\n'
              '    # 注释里有 & 和 <\n'
              '    total = 0x1F\n'
              '    for value in values:\n'
              '        total += len(str(value))\n')

    def test_highlight_preserves_every_byte(self):
        self.assertEqual(strip_tags(build_site.highlight_python(self.SAMPLE)), self.SAMPLE)

    def test_highlight_actually_marks_keywords(self):
        rendered = build_site.highlight_python(self.SAMPLE)
        self.assertIn('<span class="k">def</span>', rendered)
        self.assertIn('<span class="k">for</span>', rendered)
        self.assertIn('<span class="t">len</span>', rendered)

    def test_triple_quoted_string_is_one_token(self):
        """三引号串若被拆开，后面整段代码都会被当成字符串——版面会当场崩掉。"""
        rendered = build_site.highlight_python(self.SAMPLE)
        self.assertEqual(rendered.count('<span class="s">'), 1, rendered)

    def test_mangled_code_would_go_red(self):
        broken = build_site.highlight_python(self.SAMPLE).replace("total", "", 1)
        self.assertNotEqual(strip_tags(broken), self.SAMPLE)

    def test_python_fence_is_dispatched_to_python_highlighter(self):
        html_out = render("```python\ndef demo():\n    return None\n```\n")
        self.assertIn('data-lang="python"', html_out)
        self.assertIn('<span class="k">def</span>', html_out)


if __name__ == "__main__":
    unittest.main()
