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

    def test_exemption_does_not_excuse_verbatim_copies(self):
        """**豁免只赦免「像函数」，绝不赦免「逐字来自 code/」。**

        2026-08-17 实测：T-026 第一版里，给一段逐字抄自 `code/` 的 `push()`
        加一句 `original-listing="就想这么写"`，R8 就从报红变成放行——
        等于把 D-010 堵的那个口子换了个名字重新打开。
        original-listing 的语义是「这是原书引文」，而逐字来自 `code/` 的字节
        按定义就不是引文，是本书自己的代码。
        """
        block = ('```text original-listing="就想这么写"\n' + self.FUNC + "```\n")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertTrue(any("R8" in p and "逐字抄自" in p for p in problems), problems)

    def test_operator_definition_counts_as_function_like(self):
        """`operator=` 也要认。

        本书讲的正是三法则/五法则，最可能被抄成 text 块的就是拷贝赋值运算符；
        而第一版的名字正则只认 `名字(`，`operator=(` 的名字后面是 `=`，整段漏网。
        """
        op = (
            "Probe& operator=(const Probe& other) {\n"
            "    if (this != &other) {\n"
            "        delete[] data_;\n"
            "        data_ = new int[other.size_];\n"
            "    }\n"
            "    return *this;\n"
            "}\n"
        )
        self.assertTrue(check_doc.function_like_text(op), "operator= 应当算函数定义形态")
        block = "```text\n" + op + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources={})
        self.assertTrue(any("R8" in p for p in problems), problems)

    def test_exempted_original_quote_passes(self):
        """原书那种编不过的清单，写明理由后放行——否则这条规则没法用。"""
        block = ('```text original-listing="原书主程序按印刷编不过，只能原样引用"\n'
                 "void main( ) {\n"
                 "long x;\n"
                 "cin >> x;\n"
                 "cout << factorial(4) << endl;\n"
                 "```\n")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_committed_tree_passes_r8(self):
        """入库的书稿 + 全部 legacy.md 当锚：以后谁手抄源码进 text 块，这里就红。"""
        sources = check_doc.source_texts()
        problems = []
        for path in sorted(check_doc.BOOK.rglob("*.md")):
            if "pdf" in path.relative_to(check_doc.BOOK).parts:
                continue
            problems += check_doc.check_r8(path, sources)
        for legacy in sorted((check_doc.ROOT / "code").rglob("legacy.md")):
            problems += check_doc.check_r8(legacy, sources)
        self.assertEqual(problems, [])

    def test_legacy_files_are_in_scope(self):
        """`legacy.md` 也要查——那里正是原书引文最密集的地方。

        这条走 subprocess，因为「扫哪些文件」这件事只在 `main()` 里定；
        单测直接调 `check_r8` 是证明不了扫描范围的（把 legacy 那圈循环删掉，
        直接调用的测试依然全绿）。
        """
        import subprocess

        unit = check_doc.ROOT / "code" / "_probe_r8_unit"
        unit.mkdir(parents=True, exist_ok=True)
        legacy = unit / "legacy.md"
        try:
            legacy.write_text("# 探针\n\n```text\n" + self.FUNC + "```\n", encoding="utf-8")
            done = subprocess.run(
                ["python3", "tools/check_doc.py"],
                cwd=check_doc.ROOT, capture_output=True, text=True,
            )
        finally:
            legacy.unlink(missing_ok=True)
            unit.rmdir()
        self.assertEqual(done.returncode, 1, done.stdout)
        self.assertIn("R8", done.stdout)
        self.assertIn("_probe_r8_unit", done.stdout)

    def test_original_book_quote_needs_visible_exemption(self):
        """不再因“不像当前源码”静默放行；原书引文也要留下可审查的理由。"""
        original = (
            "```text\n"
            "void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }\n"
            "```\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(original, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertTrue(any("R8" in p for p in problems), problems)

    def test_original_book_quote_with_reason_is_allowed(self):
        original = (
            '```text original-listing="原书清单含 void main，按印刷无法通过 C++ 编译"\n'
            "void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }\n"
            "```\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(original, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources=self.sources())
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_blank_original_listing_reason_is_rejected(self):
        block = '```text original-listing=""\n' + self.FUNC + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources={})
        self.assertTrue(any("必须写明" in p for p in problems), problems)

    def test_drifted_function_is_flagged_without_source_match(self):
        block = "```text\n" + self.FUNC.replace("delete node;", "node = nullptr;") + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(block, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources={})
        self.assertTrue(any("形似 C++ 函数定义" in p for p in problems), problems)

    def test_long_trace_with_control_blocks_is_allowed(self):
        trace = "```text\n" + ("while (queue not empty) { take next item }\n" * 4) + "```\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "chapter.md"
            path.write_text(trace, encoding="utf-8")
            problems = check_doc.check_file(path, {"算法3.3"}, sources={})
        self.assertFalse(any("R8" in p for p in problems), problems)

    def test_legacy_evidence_file_is_subject_to_r8(self):
        """legacy.md 不能成为书稿之外的第二个静默逃生口。"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "legacy.md"
            path.write_text("```text\n" + self.FUNC + "```\n", encoding="utf-8")
            problems = check_doc.check_r8(path, sources={})
        self.assertTrue(any("形似 C++ 函数定义" in p for p in problems), problems)

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


class TestR13DoubleDash(unittest.TestCase):
    """R13：`0--127` 印出来就是 `0--127`。

    LaTeX 里 `--` 是短破折号，Markdown 不认。三次撞见同一个习惯
    （ch04 的 `0--127`、ch09 表里的 `5--8`、习题答案里的「第 7--12 章」），
    所以把它变成判据，而不是每次靠人眼。
    """

    def check(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.md"
            path.write_text(text, encoding="utf-8")
            return check_doc.check_double_dash(path)

    def test_range_with_double_dash_is_reported(self):
        problems = self.check("ASCII 为 0--127 的单字节编码。\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R13", problems[0])

    def test_range_inside_chinese_prose_is_reported(self):
        """真实撞见的第三处：习题答案里的「第 7--12 章」。"""
        self.assertTrue(self.check("见第 7--12 章。\n"))

    def test_chinese_compound_with_double_dash_is_reported(self):
        """第四处，2026-08-18：习题答案 ch12 第 3 题写的「标记--清扫」。

        判据一开始只收字母数字两侧，理由是没有真实用例撑着汉字那一半；
        代码里留了一句「哪天真出现，连同用例一起加回来」。这就是那一天。
        """
        problems = self.check("环还需要标记--清扫等方案。\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R13", problems[0])

    def test_real_en_dash_passes(self):
        self.assertEqual(self.check("ASCII 为 0–127 的单字节编码。\n"), [])

    def test_command_line_flags_are_not_dashes(self):
        """`--check` 这类开关合法：`--` 前面是空格，不在判据里。"""
        self.assertEqual(self.check("跑 `python3 tools/handoff.py --verify` 就行。\n"), [])

    def test_inline_code_and_links_are_skipped(self):
        """行内代码与链接目标里的 `--` 不算——仓库名和 URL 本来就可能带它。"""
        self.assertEqual(self.check("见 [`a--b`](https://example.com/a--b) 那份。\n"), [])

    def test_code_fences_are_skipped(self):
        self.assertEqual(self.check("```text\n5--8 输出耗尽\n```\n"), [])

    def test_committed_book_is_clean(self):
        """入库书稿当锚：以后谁再写 LaTeX 式破折号，这里就红。"""
        found = []
        for path in sorted(check_doc.BOOK.rglob("*.md")):
            if "pdf" in path.relative_to(check_doc.BOOK).parts:
                continue
            found += check_doc.check_double_dash(path)
        self.assertEqual(found, [])


class TestR16SlideCoverage(unittest.TestCase):
    """R16：正文讲了的，课件讲没讲——要有一张说得清的表。

    **缘由**：新鲜度清单把「课件内容覆盖」列为黄色，并留了一条实测警告：
    这一项只能做成**显式登记**。按节号自动匹配得 53 个「未覆盖小节」、
    按标题关键词得 45 个，抽查后全是假阳性（Prim 在 ch07 课件出现 6 次却匹配不上）。
    所以闸门验的是登记表，不是匹配器。
    """

    def entries(self, *rows):
        out = {}
        for row in rows:
            row.setdefault("by", "Claude")
            row.setdefault("date", "2026-08-18")
            out[row["section"]] = row
        return out

    PAGES = {"ch06": {"6.1.1 树与森林", "6.2 链式存储：四种表示法"}}

    def test_registered_page_passes(self):
        entries = self.entries({"section": "6.1.1", "kind": "covered",
                                "slides": ["ch06:6.1.1 树与森林"]})
        self.assertEqual(
            check_doc.check_slide_coverage({"6.1.1"}, entries, self.PAGES), [])

    def test_unregistered_section_is_reported(self):
        """正文新增一节而没登记——这正是此前没有任何机制能提醒的情况。"""
        problems = check_doc.check_slide_coverage({"6.9"}, {}, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R16", problems[0])
        self.assertIn("6.9", problems[0])

    def test_renamed_slide_page_is_reported(self):
        """课件页标题改了、登记表没跟着改。"""
        entries = self.entries({"section": "6.1.1", "kind": "covered",
                                "slides": ["ch06:树与森林（旧标题）"]})
        problems = check_doc.check_slide_coverage({"6.1.1"}, entries, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("不存在", problems[0])

    def test_stale_registry_entry_is_reported(self):
        """书稿删掉了这一节，登记表还留着。"""
        entries = self.entries({"section": "6.9", "kind": "pending", "reason": "欠着"})
        problems = check_doc.check_slide_coverage(set(), entries, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("已经不存在", problems[0])

    def test_by_children_needs_real_children(self):
        """引子下面得真有内容，不能拿 by-children 当万能豁免。"""
        entries = self.entries({"section": "6.1", "kind": "by-children", "reason": "引子"})
        self.assertTrue(check_doc.check_slide_coverage({"6.1"}, entries, self.PAGES))
        entries = self.entries(
            {"section": "6.1", "kind": "by-children", "reason": "引子"},
            {"section": "6.1.1", "kind": "covered", "slides": ["ch06:6.1.1 树与森林"]})
        self.assertEqual(
            check_doc.check_slide_coverage({"6.1", "6.1.1"}, entries, self.PAGES), [])

    def test_declined_and_pending_need_a_reason(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "slide_coverage.json"
            path.write_text(json.dumps({"coverage": [
                {"section": "3.1.3a", "kind": "declined", "by": "Claude", "date": "2026-08-18"},
            ]}, ensure_ascii=False), encoding="utf-8")
            _, problems = check_doc.load_slide_coverage(path)
            self.assertTrue(any("reason" in p for p in problems), problems)

    def test_covered_with_no_pages_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "slide_coverage.json"
            path.write_text(json.dumps({"coverage": [
                {"section": "6.1.1", "kind": "covered", "slides": [],
                 "by": "Claude", "date": "2026-08-18"},
            ]}, ensure_ascii=False), encoding="utf-8")
            _, problems = check_doc.load_slide_coverage(path)
            self.assertTrue(any("没列任何课件页" in p for p in problems), problems)

    def test_slide_page_titles_are_unique_within_a_chapter(self):
        """页标题当页标识的前提：一章之内不重名。重名了这条判据就该换 id 方案。"""
        root = check_doc.BOOK / "slides"
        for path in sorted(root.glob("ch*.md")):
            body = check_doc.FRONT_MATTER_RE.sub("", path.read_text(encoding="utf-8"), count=1)
            titles = [
                hit.group(1).strip()
                for page in body.split("\n---\n")
                for hit in [check_doc.SLIDE_PAGE_TITLE_RE.search(page)] if hit
            ]
            self.assertEqual(len(titles), len(set(titles)), f"{path.name} 页标题重名")

    def test_committed_registry_covers_every_section(self):
        """入库登记表当锚：正文加一节而不登记，这里就红。"""
        entries, problems = check_doc.load_slide_coverage()
        self.assertEqual(problems, [])
        problems = check_doc.check_slide_coverage(
            check_doc.book_section_numbers(), entries, check_doc.slide_pages())
        self.assertEqual(problems, [])


class TestR17SlidePageOrigin(unittest.TestCase):
    """R17：反过来问一遍——课件这一页，正文有它的家吗？

    **缘由**（2026-08-19）：R16 只管「正文 → 课件」。人从课件网页版点出
    `ch05` 第 9 页「表达式树：周游的一个用途」在正文找不到对应内容时，R16 全绿——
    5.2.2 确实登记了这一页，而 5.2.2 的正文当时只有一句话。同一条记录反过来读才是问题。

    这条规则守的是**孤儿页**：课件讲了、没有任何一节认领的页。它**不**承诺看得出
    「这一节的正文配不配得上它登记的那几页」——那是人工复核项（D-030 里有实测数据）。
    """

    PAGES = {"ch06": {"6.1.1 树与森林", "本章小结", "树状数组"}}

    def coverage(self, *rows):
        out = {}
        for row in rows:
            row.setdefault("by", "Claude")
            row.setdefault("date", "2026-08-19")
            out[row["section"]] = row
        return out

    def registry(self, *rows):
        out = {}
        for row in rows:
            row.setdefault("kind", "frame")
            row.setdefault("reason", "章末小结页")
            row.setdefault("by", "Claude")
            row.setdefault("date", "2026-08-19")
            out[row["page"]] = row
        return out

    def test_claimed_and_registered_pages_pass(self):
        coverage = self.coverage({"section": "6.1.1", "kind": "covered",
                                  "slides": ["ch06:6.1.1 树与森林"]})
        registry = self.registry(
            {"page": "ch06:本章小结"},
            {"page": "ch06:树状数组", "kind": "extra", "reason": "正文没有树状数组，见 T-045"})
        self.assertEqual(check_doc.check_slide_pages(coverage, registry, self.PAGES), [])

    def test_orphan_page_is_reported(self):
        """课件多讲了一页，正文没有它的家，也没人登记——就是这条规则的靶子。"""
        coverage = self.coverage({"section": "6.1.1", "kind": "covered",
                                  "slides": ["ch06:6.1.1 树与森林"]})
        registry = self.registry({"page": "ch06:本章小结"})
        problems = check_doc.check_slide_pages(coverage, registry, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R17", problems[0])
        self.assertIn("树状数组", problems[0])

    def test_page_cannot_have_two_origins(self):
        """既被某一节认领又单独登记：两张表会各说各话。"""
        coverage = self.coverage({"section": "6.1.1", "kind": "covered",
                                  "slides": ["ch06:6.1.1 树与森林", "ch06:本章小结",
                                             "ch06:树状数组"]})
        registry = self.registry({"page": "ch06:本章小结"})
        problems = check_doc.check_slide_pages(coverage, registry, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("一页只能有一个出处", problems[0])

    def test_renamed_page_in_registry_is_reported(self):
        coverage = self.coverage({"section": "6.1.1", "kind": "covered",
                                  "slides": ["ch06:6.1.1 树与森林", "ch06:树状数组"]})
        registry = self.registry({"page": "ch06:小结（旧标题）"})
        problems = check_doc.check_slide_pages(coverage, registry, self.PAGES)
        self.assertTrue(any("不存在" in p for p in problems), problems)
        self.assertTrue(any("本章小结" in p for p in problems), problems)

    def test_only_covered_entries_claim_pages(self):
        """pending/declined 的 slides 字段不算认领——它没说这一页归它。"""
        coverage = self.coverage({"section": "6.1.1", "kind": "pending", "reason": "欠着",
                                  "slides": ["ch06:6.1.1 树与森林"]})
        registry = self.registry({"page": "ch06:本章小结"}, {"page": "ch06:树状数组"})
        problems = check_doc.check_slide_pages(coverage, registry, self.PAGES)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("6.1.1 树与森林", problems[0])

    def test_registry_fields_are_validated(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "slide_coverage.json"
            path.write_text(json.dumps({"coverage": [], "pages": [
                {"page": "ch06:本章小结", "kind": "frame", "by": "Claude", "date": "2026-08-19"},
                {"page": "ch06:树状数组", "kind": "orphan", "reason": "x",
                 "by": "Claude", "date": "2026-08-19"},
                {"page": "ch06:另一页", "kind": "extra", "reason": "x", "date": "2026-08-19"},
            ]}, ensure_ascii=False), encoding="utf-8")
            entries, problems = check_doc.load_slide_page_registry(path)
            self.assertTrue(any("缺 reason" in p for p in problems), problems)
            self.assertTrue(any("frame/extra" in p for p in problems), problems)
            self.assertTrue(any("缺 by" in p for p in problems), problems)
            self.assertNotIn("ch06:树状数组", entries, "kind 不合法的还进了表")

    def test_duplicate_registration_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "slide_coverage.json"
            path.write_text(json.dumps({"coverage": [], "pages": [
                {"page": "ch06:本章小结", "kind": "frame", "reason": "x",
                 "by": "Claude", "date": "2026-08-19"},
                {"page": "ch06:本章小结", "kind": "frame", "reason": "x",
                 "by": "Claude", "date": "2026-08-19"},
            ]}, ensure_ascii=False), encoding="utf-8")
            _, problems = check_doc.load_slide_page_registry(path)
            self.assertTrue(any("登记了两次" in p for p in problems), problems)

    def test_committed_registry_accounts_for_every_page(self):
        """入库登记表当锚：课件加一页而不登记，这里就红。"""
        coverage, problems = check_doc.load_slide_coverage()
        self.assertEqual(problems, [])
        registry, problems = check_doc.load_slide_page_registry()
        self.assertEqual(problems, [])
        self.assertEqual(
            check_doc.check_slide_pages(coverage, registry, check_doc.slide_pages()), [])


class TestR15QualifiedNames(unittest.TestCase):
    """R15：书稿点名本书接口时，`类名::成员` 必须真的存在。

    **缘由**：T-037 复查在一份 11 步全绿的书稿里手工抓出 4 个不存在的接口名
    （`MinHeap::pop_min`、`HuffmanTree::code`、`DisjointSet::connected`、
    `WinnerTree::winner_value`）。R3 只管 ```cpp 块，散文里点名的接口此前不受任何约束。
    """

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        code = Path(self.tmp.name) / "code" / "ch05" / "heap"
        code.mkdir(parents=True)
        (code / "modern.hpp").write_text(
            "namespace dsa::sorting {\n"
            "class MinHeap {\n"
            "public:\n"
            "    void insert(int value);\n"
            "    int remove_min();\n"
            "};\n"
            "inline void heap_sort(std::vector<int>& v);\n"
            "}\n",
            encoding="utf-8",
        )
        self.index = check_doc.code_symbol_index(Path(self.tmp.name) / "code")

    def check(self, text):
        path = Path(self.tmp.name) / "x.md"
        path.write_text(text, encoding="utf-8")
        return check_doc.check_qualified_names(path, self.index)

    def test_real_member_passes(self):
        self.assertEqual(self.check("调用 `MinHeap::remove_min()` 取最小值。\n"), [])

    def test_missing_member_is_reported(self):
        """真实缺陷：附录写的是 `pop_min`，实现里叫 `remove_min`。"""
        problems = self.check("反复 `MinHeap::pop_min` 可得到升序。\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("R15", problems[0])
        self.assertIn("pop_min", problems[0])

    def test_unknown_type_is_reported(self):
        problems = self.check("用 `WinnerTree::winner_index()` 取冠军。\n")
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("既不是类也不是命名空间", problems[0])

    def test_namespace_member_is_checked_against_that_namespace(self):
        self.assertEqual(self.check("调用 `sorting::heap_sort`。\n"), [])
        self.assertTrue(self.check("调用 `sorting::bogo_sort`。\n"))

    def test_std_is_skipped(self):
        """标准库不归我们管。"""
        self.assertEqual(self.check("返回 `std::nullopt` 就是有环。\n"), [])

    def test_file_scoped_test_names_are_not_qualified_ids(self):
        """`test.cpp::某用例` 是本仓库的「文件::用例」写法，不是 C++ 限定名。"""
        self.assertEqual(self.check("对应用例 `test.cpp::test_copy_is_deep`。\n"), [])

    def test_code_fences_are_skipped(self):
        """围栏里的代码归 R3 管，逐字来自 code/，不重复判。"""
        self.assertEqual(self.check("```cpp\nMinHeap::pop_min();\n```\n"), [])

    def test_prose_outside_backticks_is_not_checked(self):
        """只看行内代码：散文里提到原书的 arrStack<T>::push 不该被当成本书接口。"""
        self.assertEqual(self.check("原书写的是 arrStack<T>::push。\n"), [])

    def test_committed_book_and_legacy_are_clean(self):
        """入库书稿当锚：以后谁再点名一个不存在的接口，这里就红。"""
        index = check_doc.code_symbol_index()
        found = []
        for path in sorted(check_doc.BOOK.rglob("*.md")):
            if "pdf" in path.relative_to(check_doc.BOOK).parts:
                continue
            found += check_doc.check_qualified_names(path, index)
        for legacy in sorted((check_doc.ROOT / "code").rglob("legacy.md")):
            found += check_doc.check_qualified_names(legacy, index)
        self.assertEqual(found, [])


class TestR14ExerciseAnswers(unittest.TestCase):
    """R14：出了题，就得说清楚答案在哪。

    **缘由**：书稿 12 章出了 96 道习题 + 40 道上机题，而附录里 47 条「习题答案」
    答的是课程作业与 `ref_DSA` 的题——ch01 正文第 1 题问「从大到小输出三个整数」，
    附录第 1 条答的却是「数据结构的四个层次」。**编号撞在一起，学生翻到附录只会更糊涂。**
    """

    BOOK_CH = ("# 第9章\n\n## 习题\n\n### 补充题（参考课程第 9 章）\n\n"
               "1. 补充第一题\n2. 补充第二题\n\n"
               "1. 正文第一题\n2. 正文第二题\n3. 正文第三题\n\n"
               "## 上机题\n\n1. 上机第一题\n")

    def chapter(self, tmp, text=None):
        path = check_doc.BOOK / "ch09-probe.md"
        path.write_text(text or self.BOOK_CH, encoding="utf-8")
        return path

    def test_supplementary_group_is_not_counted(self):
        """补充题与「补充题参考答案」本来就配套，不该被这条规则算成缺口。"""
        with tempfile.TemporaryDirectory() as tmp:
            path = self.chapter(tmp)
            try:
                counts = check_doc.chapter_exercises(path)
            finally:
                path.unlink()
        self.assertEqual(counts, {"习题": 3, "上机题": 1, "原书习题": 0, "原书上机题": 0})

    def test_original_exercises_are_a_separate_group(self):
        """回填的原书题单独成组：正文题的答案数不该把原书题的欠账遮住。"""
        text = ("# 第9章\n\n## 习题\n\n### 补充题（参考课程第 9 章）\n\n"
                "1. 补充第一题\n2. 补充第二题\n\n"
                "1. 正文第一题\n2. 正文第二题\n3. 正文第三题\n\n"
                "### 原书习题\n\n1. 原书第一题\n2. 原书第二题\n\n"
                "## 上机题\n\n1. 上机第一题\n\n"
                "### 原书上机题\n\n1. 原书上机第一题\n")
        path = self.chapter(None, text)
        try:
            counts = check_doc.chapter_exercises(path)
            problems = check_doc.check_exercise_answers(
                [path], set(), covered={(9, "习题"): 3, (9, "上机题"): 1})
        finally:
            path.unlink()
        self.assertEqual(counts["习题"], 3, "正文题仍按老规矩数：非原书小节里的最后一组")
        self.assertEqual(counts["原书习题"], 2)
        self.assertEqual(counts["原书上机题"], 1)
        # 正文题都答过了，红的只剩 3 道原书题
        self.assertEqual(len(problems), 3, problems)
        self.assertTrue(all("原书" in p for p in problems), problems)

    def test_unanswered_exercise_is_reported(self):
        path = self.chapter(None)
        try:
            problems = check_doc.check_exercise_answers([path], set(), covered={})
        finally:
            path.unlink()
        self.assertEqual(len(problems), 4, problems)
        self.assertTrue(any("习题第 1 题" in p for p in problems), problems)
        self.assertTrue(any("上机题第 1 题" in p for p in problems), problems)

    def test_same_numbered_answer_covers_it(self):
        path = self.chapter(None)
        try:
            problems = check_doc.check_exercise_answers(
                [path], set(), covered={(9, "习题"): 3, (9, "上机题"): 1})
        finally:
            path.unlink()
        self.assertEqual(problems, [])

    def test_partial_answers_report_only_the_tail(self):
        """答到第 2 题就只欠第 3 题——同号是逐题的，不是「这一章有答案」。"""
        path = self.chapter(None)
        try:
            problems = check_doc.check_exercise_answers(
                [path], set(), covered={(9, "习题"): 2, (9, "上机题"): 1})
        finally:
            path.unlink()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("习题第 3 题", problems[0])

    def test_registered_gap_passes(self):
        path = self.chapter(None)
        gaps = {(9, "习题", 1), (9, "习题", 2), (9, "习题", 3), (9, "上机题", 1)}
        try:
            problems = check_doc.check_exercise_answers([path], gaps, covered={})
        finally:
            path.unlink()
        self.assertEqual(problems, [])

    def test_gap_without_reason_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "answer_gaps.json"
            path.write_text(json.dumps({"gaps": [
                {"chapter": 9, "group": "习题", "number": 1, "kind": "pending",
                 "by": "x", "date": "2026-08-17"}]}), encoding="utf-8")
            _, problems = check_doc.load_answer_gaps(path)
        self.assertTrue(any("reason" in p for p in problems), problems)

    def test_unknown_kind_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "answer_gaps.json"
            path.write_text(json.dumps({"gaps": [
                {"chapter": 9, "group": "习题", "number": 1, "kind": "whatever",
                 "reason": "r", "by": "x", "date": "2026-08-17"}]}), encoding="utf-8")
            _, problems = check_doc.load_answer_gaps(path)
        self.assertTrue(any("kind" in p for p in problems), problems)

    def test_committed_book_has_every_exercise_accounted_for(self):
        """入库当锚：正文题要么有同号答案，要么登记在案。

        2026-08-19：136 → 137。第 5 章补回原书上机题第 2 题「表达式二叉树」（新书第 5 题），
        附录同步加了同号答案。数字变了必须来这里改一次——这就是这条锚的用处。

        2026-09-04：137 → 256。按扫描件回填原书章末题目（先补题面、答案登记为欠着）：
        第 2 章上机题 +2，第 5～9 章各新增「原书习题」与「原书上机题」两组
        （22+5、13+4、26+2、33+4、6+2 道）。
        """
        gaps, problems = check_doc.load_answer_gaps()
        self.assertEqual(problems, [])
        found = check_doc.check_exercise_answers(
            sorted(check_doc.BOOK.glob("ch*.md")), gaps)
        self.assertEqual(found, [])
        answered = sum(check_doc.answered_exercises().values())
        self.assertEqual(answered + len(gaps), 256, "正文题总数变了就要重新盘一遍")


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


class TestD025PythonBlocks(unittest.TestCase):
    """D-025：```python 块与 ```cpp 块受同一条 R3 契约管。

    每一条都配一个「会红」的用例——2026-08-18 立 D-025 之前，
    下面这些情形**全部是绿的**，那正是这条决策的由来。
    """

    SOURCE = (
        '"""探针模块。"""\n'
        "\n"
        "# >>> demo\n"
        "def demo(values):\n"
        "    # 注释也是切片的一部分\n"
        "    return values\n"
        "# <<< demo\n"
        "\n"
        "\n"
        "def other(values):\n"
        "    return values\n"
    )

    def _with_module(self, body_lines, info):
        """在临时仓库里放一个 .py，再用给定的围栏检查一段书稿。"""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            module = root / "code" / "probe" / "modern.py"
            module.parent.mkdir(parents=True)
            module.write_text(self.SOURCE, encoding="utf-8")
            chapter = root / "chapter.md"
            fence = "```" + info if info else "```"
            chapter.write_text(
                "# 探针\n\n" + fence + "\n" + "\n".join(body_lines) + "\n```\n",
                encoding="utf-8",
            )
            old = check_doc.ROOT
            check_doc.ROOT = root
            try:
                return check_doc.check_file(chapter, set())
            finally:
                check_doc.ROOT = old

    def test_python_anchor_uses_hash_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            module = Path(tmp) / "modern.py"
            module.write_text(self.SOURCE, encoding="utf-8")
            body, err = check_doc.read_slice(module, "demo")
            self.assertIsNone(err)
            self.assertIn("def demo(values):", body)
            self.assertNotIn(">>>", body)

    def test_cpp_anchor_marker_is_unchanged(self):
        """改锚点分发时最容易顺手把 C++ 那条也改坏。"""
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp) / "modern.hpp"
            header.write_text("// >>> push\nvoid push();\n// <<< push\n", encoding="utf-8")
            body, err = check_doc.read_slice(header, "push")
            self.assertIsNone(err)
            self.assertEqual(body, "void push();")

    def test_python_function_slice_stops_at_dedent(self):
        with tempfile.TemporaryDirectory() as tmp:
            module = Path(tmp) / "modern.py"
            module.write_text(self.SOURCE, encoding="utf-8")
            body, err = check_doc.read_slice(module, "fn:demo")
            self.assertIsNone(err)
            self.assertIn("def demo(values):", body)
            self.assertNotIn("def other", body, "切片越过了 def 的缩进边界")

    def test_multiline_signature_with_dedented_paren(self):
        """参数表跨行、闭合括号顶格：纯缩进规则会在 `):` 那行就收尾，把函数体丢掉。

        2026-08-18 写 `read_python_function` 时留下的活缺陷，当天补掉——
        `modern.py` 里当时没有这种写法，所以它不会被任何现有书稿触发。
        """
        source = (
            "def spread(\n"
            "    first,\n"
            "    second\n"
            "):\n"
            "    total = first + second\n"
            "    return total\n"
            "\n"
            "\n"
            "def after(x):\n"
            "    return x\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            module = Path(tmp) / "modern.py"
            module.write_text(source, encoding="utf-8")
            body, err = check_doc.read_slice(module, "fn:spread")
            self.assertIsNone(err)
            self.assertIn("return total", body, "函数体被丢掉了")
            self.assertNotIn("def after", body, "切片越过了下一个定义")

    def test_missing_python_function_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            module = Path(tmp) / "modern.py"
            module.write_text(self.SOURCE, encoding="utf-8")
            body, err = check_doc.read_slice(module, "fn:nope")
            self.assertIsNone(body)
            self.assertIn("nope", err)

    def test_python_block_without_file_is_rejected(self):
        problems = self._with_module(["def demo(values):", "    return values"], "python")
        self.assertTrue(any("R3" in p for p in problems), problems)

    def test_python_block_must_point_at_a_py_file(self):
        problems = self._with_module(["x"], "python file=code/probe/modern.hpp#demo")
        self.assertTrue(any("R3" in p for p in problems), problems)

    def test_matching_python_block_passes(self):
        problems = self._with_module(
            ["def demo(values):", "    # 注释也是切片的一部分", "    return values"],
            "python file=code/probe/modern.py#demo",
        )
        self.assertEqual(problems, [], problems)

    def test_drifted_python_block_is_rejected(self):
        problems = self._with_module(
            ["def demo(values):", "    # 注释也是切片的一部分", "    return values[::-1]"],
            "python file=code/probe/modern.py#demo",
        )
        self.assertTrue(any("R3" in p for p in problems), problems)

    def test_relabelling_python_as_text_is_caught_by_r8(self):
        """R8 的整个由来就是这个逃生口，Python 侧不能再开一次。"""
        long_body = [
            "def demo_with_enough_bytes_to_pass_the_threshold(values):",
            "    total = 0",
            "    for value in values:",
            "        total = total + value",
            "    return total",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            module = root / "code" / "probe" / "modern.py"
            module.parent.mkdir(parents=True)
            module.write_text("\n".join(long_body) + "\n", encoding="utf-8")
            chapter = root / "chapter.md"
            chapter.write_text("```text\n" + "\n".join(long_body) + "\n```\n", encoding="utf-8")
            problems = check_doc.check_r8(chapter, check_doc.source_texts(root / "code"))
        self.assertTrue(any("R8" in p and "modern.py" in p for p in problems), problems)

    def test_python_shaped_text_block_is_caught_even_if_not_verbatim(self):
        long_body = [
            "def a_function_whose_body_is_long_enough_to_matter(values):",
            "    accumulator = 0",
            "    for value in values:",
            "        accumulator = accumulator + value",
            "    return accumulator",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            chapter = Path(tmp) / "chapter.md"
            chapter.write_text("```text\n" + "\n".join(long_body) + "\n```\n", encoding="utf-8")
            problems = check_doc.check_r8(chapter, {})
        self.assertTrue(any("R8" in p for p in problems), problems)

    def test_original_cpp_listing_is_still_allowed_as_text(self):
        """原书是 2008 年的 C++，不会有 def；Python 这条判据不该误伤引文。"""
        body = [
            "template <class T> class arrStack : public Stack<T> {",
            "  int top;",
            "  bool top(T& item);",
            "};",
        ]
        with tempfile.TemporaryDirectory() as tmp:
            chapter = Path(tmp) / "chapter.md"
            chapter.write_text(
                '```text original-listing="原书按印刷无法编译，只能原样照抄"\n'
                + "\n".join(body) + "\n```\n",
                encoding="utf-8",
            )
            problems = check_doc.check_r8(chapter, {})
        self.assertEqual(problems, [], problems)

    def test_sync_book_writes_python_blocks(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            module = root / "code" / "probe" / "modern.py"
            module.parent.mkdir(parents=True)
            module.write_text(self.SOURCE, encoding="utf-8")
            chapter = root / "chapter.md"
            chapter.write_text("```python file=code/probe/modern.py#demo\n```\n", encoding="utf-8")
            old_doc, old_sync = check_doc.ROOT, sync_book.ROOT
            check_doc.ROOT = sync_book.ROOT = root
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    ok, count = sync_book.sync_file(chapter, write=True)
            finally:
                check_doc.ROOT, sync_book.ROOT = old_doc, old_sync
            self.assertTrue(ok)
            self.assertEqual(count, 1)
            self.assertIn("def demo(values):", chapter.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()


class TestFunctionSliceScope(unittest.TestCase):
    """`#fn:类名::函数名` —— 同一文件里两个类各有一个同名私有辅助函数时的消歧。

    活缺陷记录：课件「三种周游的代码只差一行」那一页曾经用 `#fn:inorder_impl`，
    而 `code/ch05/binary_tree/teaching.hpp` 里 `BinaryTree` 与 `BinarySearchTree`
    各有一个 `inorder_impl`，于是那一页把同一个函数印了两遍（版本不同、看着像抄错）。
    R3 逐字比对照样通过——它只保证「印的和源码一致」，不保证「印的是想要的那一个」。
    """

    SOURCE = (
        "template <typename T>\n"
        "class TreeNode {\n"
        "    template <typename Visitor>\n"
        "    static void walk(const Node* node, Visitor& visit) {\n"
        "        visit(node->tag);\n"
        "    }\n"
        "};\n"
        "\n"
        "template <typename T>\n"
        "class Tree {\n"
        "    // 带注释的那一个\n"
        "    template <typename Visitor>\n"
        "    static void walk(const Node* node, Visitor& visit) {\n"
        "        visit(node->value);   // 树版\n"
        "    }\n"
        "};\n"
        "\n"
        "template <typename T>\n"
        "class SearchTree {\n"
        "    template <typename Visitor>\n"
        "    static void walk(const Node* node, Visitor& visit) {\n"
        "        visit(node->key);\n"
        "    }\n"
        "};\n"
    )

    @contextlib.contextmanager
    def _header(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "teaching.hpp"
            path.write_text(self.SOURCE, encoding="utf-8")
            yield path

    def test_unscoped_name_takes_all(self):
        """不加类名时的老行为不变：同名的全取出来。"""
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:walk")
            self.assertIsNone(err)
            self.assertIn("node->tag", body)
            self.assertIn("node->value", body)
            self.assertIn("node->key", body)

    def test_scoped_name_takes_only_that_class(self):
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:Tree::walk")
            self.assertIsNone(err)
            self.assertIn("node->value", body)
            self.assertNotIn("node->key", body, "切片越过了类体边界")

    def test_scoped_name_matches_class_exactly(self):
        """类名要整词匹配：`Tree` 不能命中先出现的 `TreeNode`，也不能命中 `SearchTree`。"""
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:Tree::walk")
            self.assertIsNone(err)
            self.assertNotIn("node->tag", body, "`Tree` 命中了 `TreeNode` 的类体")
            body, err = check_doc.read_slice(path, "fn:SearchTree::walk")
            self.assertIsNone(err)
            self.assertIn("node->key", body)
            self.assertNotIn("node->value", body)

    def test_unknown_class_is_reported(self):
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:Nope::walk")
            self.assertIsNone(body)
            self.assertIn("Nope", err)

    def test_unknown_function_in_known_class_is_reported(self):
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:Tree::nope")
            self.assertIsNone(body)
            self.assertIn("Tree", err)
            self.assertIn("nope", err)

    def test_template_header_travels_with_the_function(self):
        """成员函数模板的 `template <...>` 丢了，印在书上的那段代码自己就不成立。"""
        with self._header() as path:
            body, err = check_doc.read_slice(path, "fn:Tree::walk")
            self.assertIsNone(err)
            self.assertIn("template <typename Visitor>", body)
            self.assertIn("// 带注释的那一个", body, "模板头上方的注释被丢掉了")
            self.assertNotIn("class Tree", body, "把类头也一起吞了")
