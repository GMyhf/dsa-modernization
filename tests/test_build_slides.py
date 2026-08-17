"""课件渲染器与 `#fn:` 切片的单元测试。

课件和书稿共享同一条硬承诺：**屏幕上投的代码 = 学生 clone 下来能跑的代码**。
这条承诺由 `check_doc.py` 的 R3 守着，而 R3 能不能在课件上生效，取决于两件事：

1. 分页别切错——`---` 出现在代码围栏里是完全可能的，切错会把一页代码劈成两半，
   R3 随后比对不上，人只会以为是自己抄错了；
2. `#fn:` 别把**调用点**当成定义切出来——那样切出来的东西也能通过 R3
   （它确实逐字来自源码），但讲的是另一段代码。第二条是真踩过的坑。
"""
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import build_slides  # noqa: E402
import check_doc  # noqa: E402


class TestSplitting(unittest.TestCase):
    def test_front_matter_is_taken_off(self):
        meta, body = build_slides.split_front_matter(
            "---\ntitle: 第3章 栈与队列\nsubtitle: 教程\n---\n\n# 第一页\n"
        )
        self.assertEqual(meta["title"], "第3章 栈与队列")
        self.assertEqual(meta["subtitle"], "教程")
        self.assertNotIn("title:", body)

    def test_no_front_matter_is_fine(self):
        meta, body = build_slides.split_front_matter("# 直接开讲\n")
        self.assertEqual(meta, {})
        self.assertIn("直接开讲", body)

    def test_slides_split_on_bare_triple_dash(self):
        slides = build_slides.split_slides("# 一\n\n---\n\n# 二\n\n---\n\n# 三\n")
        self.assertEqual(len(slides), 3)

    def test_separator_inside_a_fence_does_not_split(self):
        """**本文件最重要的一条。**

        `---` 在 C++ 注释、ASCII 图、命令行输出里都可能出现。切错的后果不是报错，
        是把一页代码劈成两半——而两半各自都还是「来自 code/ 的文本」，
        R3 未必拦得住，人只会以为自己抄漏了。
        """
        text = "# 一\n\n```text\n上面\n---\n下面\n```\n\n---\n\n# 二\n"
        slides = build_slides.split_slides(text)
        self.assertEqual(len(slides), 2)
        first = "\n".join(slides[0])
        self.assertIn("上面", first)
        self.assertIn("下面", first)

    def test_empty_slides_are_dropped(self):
        slides = build_slides.split_slides("\n---\n\n# 只有这一页\n\n---\n\n")
        self.assertEqual(len(slides), 1)

    def test_notes_are_taken_out_of_the_body(self):
        lines, notes = build_slides.take_notes(
            ["# 标题", "", "<!-- 备注", "讲课时说的话", "-->", "", "- 要点"]
        )
        body = "\n".join(lines)
        self.assertNotIn("讲课时说的话", body)
        self.assertIn("- 要点", body)
        self.assertEqual(notes, ["讲课时说的话"])

    def test_plain_html_comments_are_left_alone(self):
        """只有带「备注」二字的注释才是讲稿，别的注释原样留着。"""
        _, notes = build_slides.take_notes(["<!-- 这只是个普通注释 -->"])
        self.assertEqual(notes, [])


class TestFunctionSlice(unittest.TestCase):
    """`#fn:名字` 的切片判据。"""

    def slice(self, source, ref):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.hpp"
            path.write_text(source, encoding="utf-8")
            return check_doc.read_slice(path, ref)

    def test_takes_the_definition_with_its_comment(self):
        src = "class A {\n    // 说明这个函数在干什么\n    void push(int v) {\n        n_ = v;\n    }\n};\n"
        text, err = self.slice(src, "fn:push")
        self.assertIsNone(err)
        self.assertIn("// 说明这个函数在干什么", text)
        self.assertIn("void push(int v) {", text)
        self.assertIn("}", text)

    def test_call_site_is_not_a_definition(self):
        """**真踩过的坑。**

        找 `full` 时，`if (full()) {` 那一行也「名字后面跟括号、后面还有花括号」。
        判据必须落在「参数表的右括号后面是不是 `{` 或 `:`」上——
        `if (full())` 的右括号后面是另一个 `)`。
        """
        src = (
            "class Q {\n"
            "    bool full() const { return n_ == cap_; }\n"
            "    bool enqueue(int v) {\n"
            "        if (full()) {\n"
            "            return false;\n"
            "        }\n"
            "        return true;\n"
            "    }\n"
            "};\n"
        )
        text, err = self.slice(src, "fn:full")
        self.assertIsNone(err)
        self.assertIn("bool full() const", text)
        self.assertNotIn("enqueue", text)
        self.assertNotIn("return false;", text)

    def test_declaration_is_not_a_definition(self):
        src = "class A {\n    void clear();\n};\nvoid A::clear() {\n    n_ = 0;\n}\n"
        text, err = self.slice(src, "fn:clear")
        self.assertIsNone(err)
        self.assertIn("void A::clear() {", text)
        self.assertNotIn("void clear();", text)

    def test_constructor_with_init_list(self):
        """构造函数的 `)` 后面往往什么都没有，`:` 在下一行。"""
        src = (
            "class A {\n"
            "    explicit A(int n)\n"
            "        : n_(n), m_(0) {}\n"
            "};\n"
        )
        text, err = self.slice(src, "fn:A")
        self.assertIsNone(err)
        self.assertIn("explicit A(int n)", text)
        self.assertIn(": n_(n), m_(0) {}", text)

    def test_destructor_is_findable(self):
        """`~A(` 前面是空格，两个都是非单词字符——`\\b` 在这里不成立。"""
        src = "class A {\n    A() {}\n    ~A() { delete p_; }\n};\n"
        text, err = self.slice(src, "fn:~A")
        self.assertIsNone(err)
        self.assertIn("~A() { delete p_; }", text)
        self.assertNotIn("A() {}\n", text.replace("~A() {", ""))

    def test_plain_name_does_not_match_the_destructor(self):
        src = "class A {\n    A() { n_ = 0; }\n    ~A() { delete p_; }\n};\n"
        text, err = self.slice(src, "fn:A")
        self.assertIsNone(err)
        self.assertIn("A() { n_ = 0; }", text)
        self.assertNotIn("delete p_", text)

    def test_overloads_come_out_together(self):
        """同名重载一并取出——讲课时本来就该一起看（比如两种 enqueue）。"""
        src = (
            "class A {\n"
            "    void push(const int& v) { a_ = v; }\n"
            "    void push(double v) { b_ = v; }\n"
            "};\n"
        )
        text, err = self.slice(src, "fn:push")
        self.assertIsNone(err)
        self.assertIn("const int& v", text)
        self.assertIn("double v", text)

    def test_missing_name_is_an_error_not_silence(self):
        text, err = self.slice("class A {};\n", "fn:nope")
        self.assertIsNone(text)
        self.assertIn("nope", err)

    def test_braces_in_strings_do_not_confuse_the_matcher(self):
        src = (
            'class A {\n'
            '    void go() {\n'
            '        const char* s = "}";\n'
            '        n_ = 1;\n'
            '    }\n'
            '    int after_ = 0;\n'
            '};\n'
        )
        text, err = self.slice(src, "fn:go")
        self.assertIsNone(err)
        self.assertIn("n_ = 1;", text)
        self.assertNotIn("after_", text)



class TestDensityGuard(unittest.TestCase):
    """单页太满就警告——这是上一轮留下的开放问题，这一轮补上判据。

    数字是量出来的（见 `MAX_SLIDE_LINES` 的注释），不是拍的。
    它只警告不挡构建：代码页滚一下还能接受，要点页超了基本是没拆干净。
    """

    def test_threshold_is_pinned(self):
        """阈值一改，全部课件的排版结论就变了，得有人重新过一遍。"""
        self.assertEqual(build_slides.MAX_SLIDE_LINES, 32)

    def test_crowded_slide_is_reported(self):
        import io
        import contextlib

        with tempfile.TemporaryDirectory() as tmp:
            deck = Path(tmp) / "ch99-test.md"
            body = "\n".join(f"- 第 {i} 条要点" for i in range(60))
            deck.write_text(f"---\ntitle: 探针\n---\n\n# 太满的一页\n\n{body}\n",
                            encoding="utf-8")
            saved_slides, saved_site = build_slides.SLIDES, build_slides.SITE
            build_slides.SLIDES = Path(tmp)
            build_slides.SITE = Path(tmp) / "site"
            try:
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    build_slides.build()
                out = buf.getvalue()
            finally:
                build_slides.SLIDES, build_slides.SITE = saved_slides, saved_site
        self.assertIn("太满的一页", out)
        self.assertIn("投影上放不下", out)

    def test_normal_slide_is_not_reported(self):
        import io
        import contextlib

        with tempfile.TemporaryDirectory() as tmp:
            deck = Path(tmp) / "ch99-test.md"
            deck.write_text("---\ntitle: 探针\n---\n\n# 正常一页\n\n- 一\n- 二\n",
                            encoding="utf-8")
            saved_slides, saved_site = build_slides.SLIDES, build_slides.SITE
            build_slides.SLIDES = Path(tmp)
            build_slides.SITE = Path(tmp) / "site"
            try:
                buf = io.StringIO()
                with contextlib.redirect_stdout(buf):
                    build_slides.build()
                out = buf.getvalue()
            finally:
                build_slides.SLIDES, build_slides.SITE = saved_slides, saved_site
        self.assertNotIn("投影上放不下", out)

    def test_committed_decks_are_all_within_budget(self):
        """入库的 12 份课件本身也当锚：以后加页超了，这里就红。"""
        import io
        import contextlib

        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            build_slides.build(check_only=True)
        self.assertNotIn("投影上放不下", buf.getvalue())


class TestRenderedDeck(unittest.TestCase):
    """拿入库的那份课件当锚：渲染器改坏了，这里立刻能看出来。"""

    SITE = ROOT / "book" / "slides" / "site"

    def test_index_cards_do_not_reuse_fullscreen_deck_class(self):
        """索引卡片不能继承放映容器的 `.deck { height: 100vh }`。"""
        text = build_slides.index_html([("ch01.html", "第一章", 17)])
        self.assertIn('class="deck-card"', text)
        self.assertNotIn('<a class="deck"', text)

    @unittest.skipUnless(SITE.is_dir(), "课件还没构建")
    def test_deck_is_self_contained(self):
        """零 CDN：投影的机器不一定有网。

        允许 `<a href="https://…">` —— 那是「看源码」的链接，点不点在人；
        不允许任何**加载**外部资源的 src。
        """
        text = (self.SITE / "ch03-stack.html").read_text(encoding="utf-8")
        self.assertNotIn('src="http', text)
        self.assertNotIn("@import", text)
        self.assertNotIn("<link", text)

    @unittest.skipUnless(SITE.is_dir(), "课件还没构建")
    def test_notes_are_carried_into_the_page(self):
        text = (self.SITE / "ch03-stack.html").read_text(encoding="utf-8")
        self.assertIn("data-notes=", text)
        self.assertIn("演讲者备注", text)

    @unittest.skipUnless(SITE.is_dir(), "课件还没构建")
    def test_images_resolve_from_the_deck(self):
        import re

        text = (self.SITE / "ch03-stack.html").read_text(encoding="utf-8")
        srcs = re.findall(r'<img[^>]*src="([^"]+)"', text)
        self.assertTrue(srcs, "这份课件本来是有插图的")
        for src in srcs:
            self.assertTrue((self.SITE / src).is_file(), src)

    @unittest.skipUnless(SITE.is_dir(), "课件还没构建")
    def test_check_mode_agrees_with_the_committed_output(self):
        self.assertEqual(build_slides.build(check_only=True), 0)


if __name__ == "__main__":
    unittest.main()
