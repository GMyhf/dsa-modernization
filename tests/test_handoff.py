"""交接包生成的单元测试。

为什么值得测：**第一轮交接时仓库里所有东西都是未跟踪的新文件**，
而 `git diff HEAD` 看不见未跟踪文件——如果不补，审查方拿到的是一份空 diff，
却还附着一句「闸门全绿」。这正是首轮最容易翻车的地方。
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import handoff  # noqa: E402


class TestGitHelper(unittest.TestCase):
    def test_soft_swallows_failure(self):
        self.assertEqual(handoff.git(["rev-parse", "--verify", "no-such-ref"], soft=True), "")

    def test_keep_output_on_error_preserves_stdout(self):
        """`git diff --no-index` 有差异时退出码是 1，stdout 恰恰是我们要的东西。"""
        with tempfile.TemporaryDirectory() as tmp:
            f = Path(tmp) / "a.txt"
            f.write_text("hello\n", encoding="utf-8")
            out = handoff.git(
                ["diff", "--no-index", "--", "/dev/null", str(f)], keep_output_on_error=True
            )
        self.assertIn("+hello", out)

    def test_hard_failure_still_raises(self):
        with self.assertRaises(subprocess.CalledProcessError):
            handoff.git(["cat-file", "-p", "definitely-not-an-object"])


class TestUntrackedFilesReachTheReviewer(unittest.TestCase):
    def test_new_file_appears_in_diff(self):
        rel = "collab/__probe_untracked__.md"
        probe = ROOT / rel
        self.assertFalse(probe.exists(), "探针文件已存在，说明上一次测试没清干净")
        probe.write_text("探针：这一行必须出现在 review 包的 diff 里\n", encoding="utf-8")
        self.addCleanup(probe.unlink)
        try:
            chunks = handoff.untracked_diffs([rel])
        finally:
            pass
        joined = "\n".join(chunks)
        self.assertIn("+探针", joined)
        self.assertIn(rel, joined)

    def test_binary_file_does_not_explode_the_packet(self):
        """插图是二进制，git 会缩成一行；别把 12KB 的 jpg 原样灌进 markdown。"""
        figures = sorted((ROOT / "book" / "assets").glob("*.jpg"))
        if not figures:
            self.skipTest("book/assets 下还没有插图")
        rel = str(figures[0].relative_to(ROOT))
        out = "\n".join(handoff.untracked_diffs([rel]))
        self.assertLess(len(out), 2000, "二进制文件不该被展开成大段内容")


class TestChecklist(unittest.TestCase):
    def test_checklist_covers_the_project_red_lines(self):
        """检查清单是交接的最后一道人工闸门，红线掉了一条就等于没人看那条。"""
        for keyword in (
            "dsa_raw.md", "file=", "变异自检", "STL", "exclusions.json", "--verify",
            "D-001", "d001_exceptions",
        ):
            with self.subTest(keyword=keyword):
                self.assertIn(keyword, handoff.CHECKLIST)

    def test_checklist_requires_pdf_freshness(self):
        self.assertIn("build_book_pdf.py --check", handoff.CHECKLIST)


if __name__ == "__main__":
    unittest.main()
