"""PDF 截断自检的单元测试。

`xelatex` 退出码为 0 并不等于书排完了——2026-08-14 发布的 189 页 PDF 少了两整章
和 291 张插图，闸门却一路绿灯。所以每条自检都必须有一个「会红」的用例。
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

import build_book_pdf  # noqa: E402

FULL_TEX = r"""
\chapter{第1章 概论}
\includegraphics[width=0.62\linewidth]{/repo/book/assets/aaa.jpg}
\chapter{原书插图}
\includegraphics[width=0.62\linewidth]{/repo/book/assets/bbb.jpg}
\chapter{原书勘误}
"""

FULL_TOC = r"""
\contentsline {chapter}{第1章 概论}{1}{}
\contentsline {chapter}{原书插图}{9}{}
\contentsline {chapter}{原书勘误}{12}{}
"""

FULL_LOG = """
File: /repo/book/assets/aaa.jpg Graphic file (type bmp)
File: /repo/book/assets/bbb.jpg Graphic file (type bmp)
Output written on book.pdf (14 pages)
"""

MISSING = object()


def verify(tex=FULL_TEX, toc=FULL_TOC, log=FULL_LOG):
    """跑一次自检，返回 (页数或 None, stderr)。None 表示自检判定书是残的。"""
    with tempfile.TemporaryDirectory() as tmp:
        tex_path = Path(tmp) / "book.tex"
        toc_path = Path(tmp) / "book.toc"
        tex_path.write_text(tex, encoding="utf-8")
        if toc is not MISSING:
            toc_path.write_text(toc, encoding="utf-8")
        err = io.StringIO()
        pages = None
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            try:
                pages = build_book_pdf.verify_not_truncated(tex_path, toc_path, log)
            except SystemExit:
                pages = None
        return pages, err.getvalue()


class TestVerifyNotTruncated(unittest.TestCase):
    def test_complete_build_passes(self):
        pages, err = verify()
        self.assertEqual(pages, 14)
        self.assertEqual(err, "")

    def test_missing_trailing_chapters_is_caught(self):
        """观测到的那次事故：目录停在第 1 章，插图和勘误两章没排出来。"""
        pages, err = verify(toc="\\contentsline {chapter}{第1章 概论}{1}{}\n")
        self.assertIsNone(pages)
        self.assertIn("书被截断了", err)
        self.assertIn("只排到 1 章", err)

    def test_missing_toc_file_is_caught(self):
        pages, err = verify(toc=MISSING)
        self.assertIsNone(pages)
        self.assertIn("只排到 0 章", err)

    def test_dropped_figures_are_caught(self):
        """另一半事故：章都在，291 张图一张没嵌进去。"""
        pages, err = verify(log="Output written on book.pdf (14 pages)\n")
        self.assertIsNone(pages)
        self.assertIn("2/2 张图没进 PDF", err)

    def test_xelatex_without_output_line_is_caught(self):
        pages, err = verify(log=FULL_LOG.replace("Output written on book.pdf (14 pages)", ""))
        self.assertIsNone(pages)
        self.assertIn("没正常收尾", err)

    def test_expectations_are_derived_from_the_tex(self):
        """再加一张图，期望值自己跟着变——这里不写死任何页数或图数。"""
        pages, err = verify(tex=FULL_TEX + "\\includegraphics{/repo/book/assets/ccc.jpg}\n")
        self.assertIsNone(pages)
        self.assertIn("1/3 张图没进 PDF", err)


class TestBuildInfoSidecar(unittest.TestCase):
    """页数只有 xelatex 的日志知道；网页版的下载卡片靠这份 sidecar 读到它。"""

    def test_counts_come_from_the_tex_and_the_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            tex_path = Path(tmp) / "book.tex"
            tex_path.write_text(FULL_TEX, encoding="utf-8")
            original = build_book_pdf.PDF_DIR
            try:
                build_book_pdf.PDF_DIR = Path(tmp)
                with contextlib.redirect_stdout(io.StringIO()):
                    written = build_book_pdf.write_build_info(tex_path, FULL_LOG, 364)
                info = json.loads(written.read_text(encoding="utf-8"))
            finally:
                build_book_pdf.PDF_DIR = original
        self.assertEqual(info["chapters"], 3)
        self.assertEqual(info["main_chapters"], 12)
        self.assertEqual(info["figures"], 2)
        self.assertEqual(info["pages"], 364)
        self.assertRegex(info["source_sha256"], r"^[0-9a-f]{64}$")

    def test_rebuilding_the_same_book_gives_the_same_bytes(self):
        """不写时间戳：同一份书稿重排两次，sidecar 不该在 git 里制造 diff。"""
        with tempfile.TemporaryDirectory() as tmp:
            tex_path = Path(tmp) / "book.tex"
            tex_path.write_text(FULL_TEX, encoding="utf-8")
            original = build_book_pdf.PDF_DIR
            try:
                build_book_pdf.PDF_DIR = Path(tmp)
                with contextlib.redirect_stdout(io.StringIO()):
                    first = build_book_pdf.write_build_info(tex_path, FULL_LOG, 364).read_bytes()
                    second = build_book_pdf.write_build_info(tex_path, FULL_LOG, 364).read_bytes()
            finally:
                build_book_pdf.PDF_DIR = original
        self.assertEqual(first, second)


class TestPdfFreshness(unittest.TestCase):
    def run_check(self, root: Path, source: Path):
        original = (
            build_book_pdf.CHAPTERS,
            build_book_pdf.PREAMBLE,
            build_book_pdf.PDF_DIR,
            build_book_pdf.OUTPUT,
        )
        pdf_dir = root / "pdf"
        pdf_dir.mkdir()
        preamble = root / "preamble.tex"
        preamble.write_text("% test\n", encoding="utf-8")
        output = pdf_dir / "book.pdf"
        output.write_bytes(b"%PDF-test")
        try:
            build_book_pdf.CHAPTERS = [source]
            build_book_pdf.PREAMBLE = preamble
            build_book_pdf.PDF_DIR = pdf_dir
            build_book_pdf.OUTPUT = output
            info = {"chapters": 1, "main_chapters": 1, "figures": 1, "pages": 2,
                    "source_sha256": build_book_pdf.source_sha256()}
            (pdf_dir / "build-info.json").write_text(json.dumps(info), encoding="utf-8")
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                result = build_book_pdf.check_current()
            return result, out.getvalue()
        finally:
            (build_book_pdf.CHAPTERS, build_book_pdf.PREAMBLE,
             build_book_pdf.PDF_DIR, build_book_pdf.OUTPUT) = original

    def test_matching_source_hash_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "chapter.md"
            source.write_text("第一版\n", encoding="utf-8")
            result, out = self.run_check(Path(tmp), source)
        self.assertEqual(result, 0)
        self.assertIn("PDF 与源文件一致", out)

    def test_one_character_source_change_is_caught(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "chapter.md"
            source.write_text("第一版\n", encoding="utf-8")

            original = (build_book_pdf.CHAPTERS, build_book_pdf.PREAMBLE,
                        build_book_pdf.PDF_DIR, build_book_pdf.OUTPUT)
            pdf_dir = root / "pdf"
            pdf_dir.mkdir()
            preamble = root / "preamble.tex"
            preamble.write_text("% test\n", encoding="utf-8")
            output = pdf_dir / "book.pdf"
            output.write_bytes(b"%PDF-test")
            try:
                build_book_pdf.CHAPTERS = [source]
                build_book_pdf.PREAMBLE = preamble
                build_book_pdf.PDF_DIR = pdf_dir
                build_book_pdf.OUTPUT = output
                info = {"chapters": 1, "main_chapters": 1, "figures": 1, "pages": 2,
                        "source_sha256": build_book_pdf.source_sha256()}
                (pdf_dir / "build-info.json").write_text(json.dumps(info), encoding="utf-8")
                source.write_text("第二版\n", encoding="utf-8")
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    result = build_book_pdf.check_current()
            finally:
                (build_book_pdf.CHAPTERS, build_book_pdf.PREAMBLE,
                 build_book_pdf.PDF_DIR, build_book_pdf.OUTPUT) = original
        self.assertEqual(result, 1)
        self.assertIn("PDF 已过期", out.getvalue())


class TestRegexes(unittest.TestCase):
    def test_graphic_regex_handles_optional_args(self):
        found = build_book_pdf.GRAPHIC_RE.findall(
            r"\includegraphics{a.jpg}\includegraphics[width=2cm]{b/c.jpg}"
        )
        self.assertEqual(found, ["a.jpg", "b/c.jpg"])

    def test_pages_regex_reads_the_page_count(self):
        self.assertEqual(
            build_book_pdf.PAGES_RE.search("Output written on book.pdf (326 pages).").group(1),
            "326",
        )
        self.assertEqual(
            build_book_pdf.PAGES_RE.search("Output written on book.pdf (1 page).").group(1),
            "1",
        )


if __name__ == "__main__":
    unittest.main()
