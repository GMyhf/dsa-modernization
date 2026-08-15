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


if __name__ == "__main__":
    unittest.main()
