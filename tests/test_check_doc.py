"""书稿体检的单元测试。

重点是**每条规则都要有一个「会红」的用例**：只测通过路径的检查器等于没有检查器。
"""
import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import check_doc  # noqa: E402
import sync_book  # noqa: E402


def check(text, listings=None):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "chapter.md"
        path.write_text(text, encoding="utf-8")
        return check_doc.check_file(path, listings or {"算法3.3", "代码3.1", "代码3.2"})


class TestFenceParsing(unittest.TestCase):
    def test_parses_info_string(self):
        self.assertEqual(check_doc.parse_info("cpp file=a/b.hpp#push"), ("cpp", "a/b.hpp", "push"))
        self.assertEqual(check_doc.parse_info("cpp file=a/b.hpp"), ("cpp", "a/b.hpp", None))
        self.assertEqual(check_doc.parse_info("text"), ("text", None, None))
        self.assertEqual(check_doc.parse_info(""), ("", None, None))

    def test_block_boundaries(self):
        blocks = check_doc.iter_blocks("a\n```cpp\nx\ny\n```\nb\n")
        self.assertEqual(len(blocks), 1)
        self.assertEqual(blocks[0]["body"], ["x", "y"])
        self.assertEqual(blocks[0]["start"], 2)


class TestR1Language(unittest.TestCase):
    def test_bogus_ocr_language_is_rejected(self):
        problems = check("```hcl\nbool pop();\n```\n")
        self.assertTrue(any("R1" in p for p in problems))

    def test_allowed_language_passes(self):
        self.assertEqual(check("```text\n随便什么\n```\n"), [])


class TestR2OcrSmells(unittest.TestCase):
    def test_split_operators_are_caught(self):
        cases = {
            "for (i = 0; i < n; i + + )": "++",
            "if (top = = -1)": "==",
            "cout < < endl;": "<<",
            "arrStack < T > : : push();": "::",
            "#include < iostream >": "include",
            "item = st[top]；": "全角",
            "top = − 1;": "Unicode 减号",
        }
        for line, label in cases.items():
            with self.subTest(label=label):
                problems = check(f"```cpp file=x\n{line}\n```\n")
                self.assertTrue(any("R2" in p for p in problems), f"{line} 没被抓到")

    def test_lone_one_instead_of_closing_brace(self):
        problems = check("```cpp file=x\nvoid f() {\n1\n```\n")
        self.assertTrue(any("R2" in p and "1" in p for p in problems))

    def test_chinese_in_comments_and_strings_is_fine(self):
        """现代化后的代码里中文注释是常态，不能因此判红。"""
        body = (
            "int f() {\n"
            "    // 自赋值：先拷贝再交换，天然安全（这里有全角标点）\n"
            "    int x = 1;  // 行尾注释，也有中文，。；\n"
            '    const char* msg = "栈满溢出";\n'
            "    /* 块注释里的中文：也不该报 */\n"
            "    return x;\n"
            "}"
        )
        problems = [p for p in check(f"```cpp file=x\n{body}\n```\n") if "R2" in p]
        self.assertEqual(problems, [])

    def test_smell_inside_real_code_still_caught_when_line_has_comment(self):
        problems = check('```cpp file=x\nint i = 0; i + + ;  // 中文注释\n```\n')
        self.assertTrue(any("R2" in p for p in problems))


class TestR3IncludeContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.src = Path(self.tmp.name) / "modern.hpp"
        self.src.write_text(
            "int a = 1;\n// >>> push\nvoid push(int x) {\n    xs_[n_++] = x;\n}\n// <<< push\n",
            encoding="utf-8",
        )
        self.rel = str(self.src)
        self.addCleanup(self.tmp.cleanup)

    def read(self, anchor):
        return check_doc.read_anchor(self.src, anchor)

    def test_anchor_slice(self):
        content, err = self.read("push")
        self.assertIsNone(err)
        self.assertEqual(content, "void push(int x) {\n    xs_[n_++] = x;\n}")

    def test_missing_anchor_reports(self):
        content, err = self.read("nope")
        self.assertIsNone(content)
        self.assertIn("没有锚点", err)

    def test_unclosed_anchor_reports(self):
        self.src.write_text("// >>> push\nvoid push();\n", encoding="utf-8")
        _, err = self.read("push")
        self.assertIn("只有开头", err)

    def test_cpp_block_without_file_reference_is_rejected(self):
        problems = check("```cpp\nvoid push();\n```\n")
        self.assertTrue(any("R3" in p for p in problems))

    def test_drifted_block_is_rejected(self):
        """书稿抄了一份「差不多」的代码——这正是 R3 要抓的。"""
        problems = check(f"```cpp file={self.rel}#push\nvoid push(int x) {{\n    xs_[n_] = x;\n}}\n```\n")
        self.assertTrue(any("R3" in p and "不一致" in p for p in problems))

    def test_matching_block_passes(self):
        problems = check(
            f"```cpp file={self.rel}#push\nvoid push(int x) {{\n    xs_[n_++] = x;\n}}\n```\n"
        )
        self.assertEqual([p for p in problems if "R3" in p], [])


class TestR4Figures(unittest.TestCase):
    def test_remote_hotlink_rejected(self):
        problems = check("![栈的存储结构](https://raw.githubusercontent.com/x/y.jpg)\n")
        self.assertTrue(any("R4" in p and "热链" in p for p in problems))

    def test_missing_alt_rejected(self):
        problems = check("![](assets/x.jpg)\n")
        self.assertTrue(any("R4" in p for p in problems))

    def test_placeholder_alt_rejected(self):
        problems = check("![TODO 补图注](assets/x.jpg)\n")
        self.assertTrue(any("R4" in p and "占位" in p for p in problems))


class TestR5R6R7References(unittest.TestCase):
    def test_unclosed_listing_marker(self):
        problems = check("【算法3.3】改进的进栈操作。\n正文\n")
        self.assertTrue(any("R5" in p for p in problems))

    def test_inline_reference_is_not_a_listing_opener(self):
        """正文里「原书【代码3.1】用一个空基类」是引用，不该要求配对。"""
        problems = check("原书【代码3.1】用一个空基类来表达抽象。\n")
        self.assertEqual([p for p in problems if "R5" in p], [])

    def test_reference_to_nonexistent_listing(self):
        problems = check("详见算法9.99。\n")
        self.assertTrue(any("R6" in p for p in problems))

    def test_reference_to_nonexistent_chapter(self):
        problems = check("详见第13章。\n")
        self.assertTrue(any("R7" in p for p in problems))

    def test_markers_inside_code_blocks_are_ignored(self):
        problems = check("```text\n【算法3.3】\n第99章\n```\n")
        self.assertEqual(problems, [])


class TestSyncBook(unittest.TestCase):
    def test_sync_fills_block_and_then_check_passes(self):
        """sync（写）与 check（验）必须是一对：同步完就该体检通过。"""
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "modern.hpp"
            src.write_text("// >>> push\nvoid push(int x);\n// <<< push\n", encoding="utf-8")
            doc = Path(tmp) / "ch.md"
            doc.write_text(f"```cpp file={src}#push\n```\n", encoding="utf-8")

            with contextlib.redirect_stdout(io.StringIO()):
                ok, count = sync_book.sync_file(doc, write=True)
            self.assertTrue(ok)
            self.assertEqual(count, 1)
            self.assertIn("void push(int x);", doc.read_text(encoding="utf-8"))
            self.assertEqual(check_doc.check_file(doc, set()), [])

            with contextlib.redirect_stdout(io.StringIO()):
                ok, count = sync_book.sync_file(doc, write=True)
            self.assertEqual(count, 0, "同步应当是幂等的")


class TestRealBook(unittest.TestCase):
    def test_repo_book_passes(self):
        listings = check_doc.known_listings()
        problems = []
        for path in sorted((ROOT / "book").rglob("*.md")):
            problems += check_doc.check_file(path, listings)
        self.assertEqual(problems, [])


class TestR8CopiedTextBlocks(unittest.TestCase):
    """R8：本书自己的代码不许以 text 块手抄进书稿。

    缘由是一次真实事故：重构把两个 `cpp file=…#anchor` 块改成了 ```text，
    R3 从此看不到它们，源码改了、书上那份没改，两边当场漂开。
    """

    FUNC = (
        "T erase_node(Node* node) {\n"
        "    if (node == nullptr) throw std::out_of_range(\"empty\");\n"
        "    T value = std::move(node->value);\n"
        "    delete node;\n"
        "    return value;\n"
        "}\n"
    )

    def sources(self):
        return {"code/ch02/probe/modern.hpp": check_doc._normalize_code(
            "#pragma once\nclass Probe {\n" + self.FUNC + "};\n")}

    def test_copied_function_is_flagged(self):
        block = "```text\n" + self.FUNC + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertTrue(any("R8" in p for p in problems), problems)
        self.assertTrue(any("modern.hpp" in p for p in problems), problems)

    def test_same_code_as_cpp_file_block_is_not_flagged(self):
        """写成 cpp file= 就归 R3 管了，R8 不该再插一脚。"""
        block = "```cpp file=code/ch02/probe/modern.hpp#erase\n" + self.FUNC + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_short_teaching_excerpt_is_allowed(self):
        """正文大量「摘一行出来讲」的写法必须放行，否则这条规则没法用。"""
        excerpt = "```text\nstatic constexpr int infinity = 1;\n```\n"
        srcs = {"code/x/modern.hpp": check_doc._normalize_code(
            "class X { static constexpr int infinity = 1; };")}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(excerpt, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=srcs)
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_quoting_the_original_book_is_allowed(self):
        """引用原书那些编不过的清单，本来就只能用 text——它们不在 code/ 里，不会命中。"""
        original = (
            "```text\n"
            "void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }\n"
            "```\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(original, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_indentation_does_not_matter(self):
        """书稿里顶格、源码里在类内缩进四格——同一段代码，仍要判为抄的。"""
        dedented = "```text\n" + "".join(
            line[4:] if line.startswith("    ") else line for line in self.FUNC.splitlines(True)
        ) + "```\n"
        srcs = {"code/x/modern.hpp": check_doc._normalize_code(
            "class X {\n" + "".join("    " + l for l in self.FUNC.splitlines(True)) + "};")}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(dedented, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=srcs)
        self.assertTrue(any("R8" in p for p in problems), problems)

    def test_rule_list_mentions_r8(self):
        self.assertTrue(any(r.startswith("R8") for r in check_doc.RULES))


if __name__ == "__main__":
    unittest.main()


class TestR10Sections(unittest.TestCase):
    """R10：原书有的节，新书要么有，要么登记。

    **缘由**：第 8 章的 8.3.1 直接选择排序、8.4.1 冒泡排序、8.6.1 桶式排序、8.6.3 索引排序
    整节没写，而 `code/ch08/sorting` 里三种实现都在、有测试、还认领着算法8.3/8.5/8.10。
    台账说「已覆盖」，书上却没讲，R5–R7 一条都碰不到它——它们只管交叉引用能不能解析。
    """

    ORIGINAL = {
        "8.3": {"title": "选择排序", "chapter": 8, "line": 10},
        "8.3.1": {"title": "直接选择排序", "chapter": 8, "line": 20},
        "8.3.2": {"title": "堆排序", "chapter": 8, "line": 30},
    }

    def check(self, text, gaps=None, name="ch08-sorting.md"):
        path = check_doc.BOOK / name
        original = path.read_text(encoding="utf-8") if path.exists() else None
        try:
            path.write_text(text, encoding="utf-8")
            return check_doc.check_sections(path, gaps or {}, self.ORIGINAL)
        finally:
            if original is None:
                path.unlink()
            else:
                path.write_text(original, encoding="utf-8")

    def test_missing_section_is_reported(self):
        problems = self.check("## 8.3 选择排序\n\n### 8.3.2 堆排序\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("8.3.1", problems[0])
        self.assertIn("直接选择排序", problems[0])

    def test_present_section_passes(self):
        text = "## 8.3 选择排序\n\n### 8.3.1 直接选择排序\n\n### 8.3.2 堆排序\n"
        self.assertEqual(self.check(text), [])

    def test_registered_gap_passes(self):
        gaps = {"8.3.1": {"kind": "merged", "into": "8.3"}}
        problems = self.check("## 8.3 选择排序\n\n### 8.3.2 堆排序\n", gaps)
        self.assertEqual(problems, [])

    def test_same_number_with_different_topic_is_reported(self):
        """R11 拦住用「先跑一遍」占掉原书 8.3.1 的编号。"""
        text = "## 8.3 选择排序\n\n### 8.3.1 先跑一遍\n\n### 8.3.2 堆排序\n"
        problems = self.check(text)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R11 8.3.1 同号不同题", problems[0])
        self.assertIn("直接选择排序", problems[0])

    def test_title_typography_does_not_trigger_r11(self):
        original = {"8.3": {"title": "K叉树(定义)", "chapter": 8, "line": 10}}
        path = check_doc.BOOK / "ch08-sorting.md"
        saved = path.read_text(encoding="utf-8")
        try:
            path.write_text("## 8.3 K 叉树（定义）\n", encoding="utf-8")
            self.assertEqual(check_doc.check_sections(path, {}, original), [])
        finally:
            path.write_text(saved, encoding="utf-8")

    def test_slides_are_not_subject_to_r10(self):
        """课件按讲课节奏组织，逐节对应原书目录只会逼人塞凑数的标题。"""
        slides = check_doc.BOOK / "slides"
        path = slides / "ch08-sorting.md"
        saved = path.read_text(encoding="utf-8")
        try:
            path.write_text("# 第8章\n\n## 8.3 选择排序\n", encoding="utf-8")
            self.assertEqual(check_doc.check_sections(path, {}, self.ORIGINAL), [])
        finally:
            path.write_text(saved, encoding="utf-8")

    def test_committed_book_has_no_unregistered_gap(self):
        """入库书稿当锚：以后谁删掉一节又忘了登记，这里就红。"""
        gaps, problems = check_doc.load_section_gaps()
        self.assertEqual(problems, [])
        original = check_doc.ledger.parse_sections()
        found = []
        for path in sorted(check_doc.BOOK.glob("ch*.md")):
            found += check_doc.check_sections(path, gaps, original)
        self.assertEqual(found, [])


class TestSectionGapRegistry(unittest.TestCase):
    """登记表本身也要有门槛——否则 R10 就退化成『写一行就放行』。"""

    def load(self, payload):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "section_gaps.json"
            path.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
            return check_doc.load_section_gaps(path)

    def test_merged_without_into_is_rejected(self):
        _, problems = self.load({"gaps": [
            {"section": "2.1.1", "kind": "merged", "reason": "并进去了", "by": "x", "date": "2026-08-17"}]})
        self.assertTrue(any("into" in p for p in problems), problems)

    def test_missing_reason_is_rejected(self):
        _, problems = self.load({"gaps": [
            {"section": "2.1.1", "kind": "pending", "by": "x", "date": "2026-08-17"}]})
        self.assertTrue(any("reason" in p for p in problems), problems)

    def test_unknown_kind_is_rejected(self):
        _, problems = self.load({"gaps": [
            {"section": "2.1.1", "kind": "whatever", "reason": "r", "by": "x", "date": "2026-08-17"}]})
        self.assertTrue(any("kind" in p for p in problems), problems)

    def test_duplicate_section_is_rejected(self):
        entry = {"section": "2.1.1", "kind": "declined", "reason": "r", "by": "x", "date": "2026-08-17"}
        _, problems = self.load({"gaps": [entry, dict(entry)]})
        self.assertTrue(any("重复" in p for p in problems), problems)

    def test_committed_registry_is_clean(self):
        gaps, problems = check_doc.load_section_gaps()
        self.assertEqual(problems, [])
        self.assertTrue(gaps)


class TestR12SectionRefs(unittest.TestCase):
    """R12：写着「见第 X.Y 节」，那一节就得真的存在。

    **缘由**：T-028 把被占用的编号还给原书之后，正文里 9 处引用当场悬空——
    「后面 2.2.1–2.2.4 各节」「判据见第 2.3.2a 节」「见 4.2.5」指向的小节，
    要么改成了不带编号的 `####`，要么换了号。**改编号是对的，漏改引用是自动的**：
    R6 管【算法X.Y】、R7 管「第 N 章」，中间这一层一直空着。
    """

    BOOK = {"2.2.1", "2.2.2", "2.2a", "4.2a"}
    ORIGINAL = {"2.2.1", "2.2.2", "2.2.3", "4.2.5"}

    def check(self, text, name="probe.md"):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / name
            path.write_text(text, encoding="utf-8")
            return check_doc.check_section_refs(path, self.BOOK, self.ORIGINAL)

    def test_dangling_reference_is_reported(self):
        problems = self.check("判据见第 2.3.2a 节。\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("2.3.2a", problems[0])

    def test_live_reference_passes(self):
        self.assertEqual(self.check("完整对照见第 2.2.2 节。\n"), [])

    def test_letter_suffix_section_resolves(self):
        self.assertEqual(self.check("工程版还多两个移动操作，见 4.2a。\n"), [])

    def test_range_reference_checks_both_ends(self):
        problems = self.check("后面 2.2.1–2.2.4 各节就是把它拆开逐段讲。\n")
        self.assertTrue(any("2.2.4" in p for p in problems), problems)

    def test_original_book_reference_uses_the_manuscript(self):
        """「原书 2.2.3 节」说的是 2008 年那本书，不该按新书的目录判。"""
        self.assertEqual(self.check("原书 2.2.3 节自己写下这句话。\n"), [])
        problems = self.check("本书 2.2.3 节写下这句话。\n")
        self.assertTrue(problems, "没有原书标记时应按新书解析")

    def test_errata_file_is_original_scope(self):
        """勘误表整篇都在说原书，按底稿解析。"""
        self.assertEqual(self.check("| 第 2.2.3 节 | 排印错误 |\n", name="勘误.md"), [])

    def test_plain_decimals_are_not_section_refs(self):
        """8.7.2 的实测表里全是 213.5 这样的数字，不能当成节号。"""
        self.assertEqual(self.check("  50000    213.5    482.1   5821.9\n"), [])

    def test_committed_book_has_no_dangling_reference(self):
        """入库书稿当锚：以后再改编号忘了改引用，这里就红。"""
        book = check_doc.book_section_numbers()
        original = set(check_doc.ledger.parse_sections())
        found = []
        for path in sorted(check_doc.BOOK.rglob("*.md")):
            if "pdf" in path.relative_to(check_doc.BOOK).parts:
                continue
            found += check_doc.check_section_refs(path, book, original)
        self.assertEqual(found, [])


class TestR9Formulas(unittest.TestCase):
    """R9：公式得真能渲染出来。

    **缘由**：`book/ch12-advanced.md` 里那个多维数组偏移公式从写下来那天起就是坏的——
    它跨了三行，而渲染器只认「同一行 $$ 开头、同一行 $$ 结尾」，于是整段以原始 LaTeX
    印在页面上，一直没人发现。2026-08-17 Codex 复查公式严谨性时才顺带暴露。

    判据只覆盖**能机器判定**的部分：渲染得出来吗、命令认识吗。
    **数学本身对不对机器判不了**，仍然要人复核。
    """

    def check(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.md"
            path.write_text(text, encoding="utf-8")
            return check_doc.check_file(path, set())

    def test_multiline_display_math_is_rejected(self):
        problems = self.check("# 标题\n\n$$\na + b\n$$\n")
        self.assertTrue(any("R9" in p and "跨行" in p for p in problems), problems)

    def test_single_line_display_math_passes(self):
        self.assertEqual(self.check("# 标题\n\n$$a + b$$\n"), [])

    def test_unknown_latex_command_is_rejected(self):
        problems = self.check("# 标题\n\n$$a \\nosuchcmd b$$\n")
        self.assertTrue(any("R9" in p and "nosuchcmd" in p for p in problems), problems)

    def test_known_commands_pass(self):
        """常用命令必须都认识，否则会以原始文本印在页面上。"""
        text = ("# 标题\n\n"
                r"$$\sum_{i=0}^{h} 2^{i}\cdot\lfloor\log_2 n\rfloor "
                r"\le \Bigl( \frac{j}{2^{j}} \Bigr)$$" + "\n")
        self.assertEqual(self.check(text), [])

    def test_dollars_inside_code_blocks_are_not_formulas(self):
        """```text 里的 `$$` 是命令行提示或伪代码，不是公式。"""
        self.assertEqual(self.check("# 标题\n\n```text\n$$\nnot math\n$$\n```\n"), [])



if __name__ == "__main__":
    unittest.main()
