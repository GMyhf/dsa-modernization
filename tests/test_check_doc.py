"""书稿体检的单元测试。

重点是**每条规则都要有一个「会红」的用例**：只测通过路径的检查器等于没有检查器。
"""
import contextlib
import io
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
