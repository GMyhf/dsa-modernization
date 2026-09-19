"""作者代码包登记表（tools/authorsrc.py）的单元测试。

代码包是**证据**：它回答「这处错误是作者的代码本来就错、印出来的与作者代码不一致，还是 OCR 造成的」。
证据可信要靠三件事，下面的用例一件一件钉住：

* 105 条清单每条都有去处——指到包里的符号，或写明包里没有的理由；
* 登记的符号真能在文件里找到**定义**（声明、调用、注释里的同名词都不算）；
* 包的字节没被人动过、与 2021 版副本的差异都登记过。
"""
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import authorsrc  # noqa: E402
import ledger  # noqa: E402


class TestFindDefinition(unittest.TestCase):
    SRC = """#include <iostream>
template <class T> class Stack;            // 前置声明，不是定义
template <class T>
class Stack {
public:
    bool push(const T item);               // 声明，不是定义
    bool pop(T* item) {                    // 类内定义
        if (n == 0) { cout << "}"; return false; }    // 字符串里的花括号不算
        return true;
    }
    int n;
};
// bool push(const T item) { 注释里的不算 }
template <class T>
bool Stack<T>::push(const T item) {
    return helper(item);                   // 调用，不是定义
}
"""

    def test_class_skips_forward_declaration(self):
        line, body = authorsrc.find_definition(self.SRC, "class Stack")
        self.assertEqual(line, 3, "要带上前一行的 template <class T>")
        self.assertTrue(body.startswith("template <class T>\nclass Stack {"))
        self.assertTrue(body.rstrip().endswith("};"))

    def test_function_skips_declaration_and_comment(self):
        line, body = authorsrc.find_definition(self.SRC, "push")
        self.assertEqual(line, 14)
        self.assertIn("Stack<T>::push", body)
        self.assertTrue(body.rstrip().endswith("}"))

    def test_braces_inside_strings_do_not_confuse_matching(self):
        _, body = authorsrc.find_definition(self.SRC, "pop")
        self.assertIn("return true;", body, "字符串里的 } 若被计数，函数体会被截短")

    def test_call_is_not_a_definition(self):
        self.assertIsNone(authorsrc.find_definition(self.SRC, "helper"))

    def test_nth_definition(self):
        src = "int f() { return 1; }\nint f(int x) { return x; }\n"
        self.assertEqual(authorsrc.find_definition(src, "f", 2)[0], 2)
        self.assertIsNone(authorsrc.find_definition(src, "f", 3))

    def test_commented_out_versions_need_the_flag(self):
        src = "/*\nlong fact(long n) { return n; }\n*/\nlong fact(long n) { return 1; }\n"
        self.assertEqual(authorsrc.find_definition(src, "fact")[0], 4)
        self.assertEqual(authorsrc.find_definition(authorsrc.uncomment_blocks(src), "fact")[0], 2)


class TestDecode(unittest.TestCase):
    def roundtrip(self, data: bytes) -> str:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "x.h"
            path.write_bytes(data)
            return authorsrc.decode(path)

    def test_gbk_crlf(self):
        self.assertEqual(self.roundtrip("// 栈已满\r\n".encode("gbk")), "// 栈已满\n")

    def test_utf8_bom_is_not_read_as_gbk(self):
        # BOM 当 GBK 读会变成「锘」，编译器报 does not name a type——那是我们读错了
        text = self.roundtrip(b"\xef\xbb\xbf#include <x>\r\n")
        self.assertEqual(text, "#include <x>\n")

    def test_plain_utf8(self):
        self.assertEqual(self.roundtrip("// 伸展树\n".encode("utf-8")), "// 伸展树\n")


class TestRealManifest(unittest.TestCase):
    """仓库里那份登记表本身。编译核对慢（54 个程序），这里跳过，闸门的 --check 会跑。"""

    def test_manifest_is_consistent(self):
        self.assertEqual(authorsrc.check(authorsrc.load_manifest(), compiler=None), [])

    def test_covers_exactly_the_inventory(self):
        inventory = {item["id"] for item in ledger.parse_inventory()}
        self.assertEqual(set(authorsrc.load_manifest()["listings"]), inventory)

    def test_listing_3_2_is_the_getTop_evidence(self):
        # site_visit/README.md 与 array_stack/legacy.md 都援引这一条：包里叫 getTop，不与 int top 重名
        entry = authorsrc.load_manifest()["listings"]["代码3.2"]
        (_, _, _, body), = authorsrc.slices_for(entry)
        self.assertIn("int\t\ttop;", body)
        self.assertIn("bool getTop(T* item)", body)
        self.assertNotRegex(body, r"bool\s+top\s*\(")


class TestCheckGoesRed(unittest.TestCase):
    """在临时副本上改坏一处，--check 必须报出来。"""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.pack = base / "pack"
        shutil.copytree(authorsrc.PACK, self.pack, ignore=shutil.ignore_patterns("a.out"))
        self.saved = authorsrc.PACK, authorsrc.TWIN
        authorsrc.PACK = self.pack
        authorsrc.TWIN = self.saved[1]
        self.data = json.loads(json.dumps(authorsrc.load_manifest()))

    def tearDown(self):
        authorsrc.PACK, authorsrc.TWIN = self.saved
        self.tmp.cleanup()

    def problems(self):
        return authorsrc.check(self.data, compiler=None)

    def test_baseline_copy_is_green(self):
        self.assertEqual(self.problems(), [])

    def test_missing_listing_is_red(self):
        del self.data["listings"]["算法3.3"]
        self.assertTrue(any("算法3.3" in p and "没有登记" in p for p in self.problems()))

    def test_absent_needs_a_reason(self):
        self.data["listings"]["算法3.3"] = {"absent": " "}
        self.assertTrue(any("absent 必须写理由" in p for p in self.problems()))

    def test_unknown_symbol_is_red(self):
        self.data["listings"]["代码3.2"]["symbols"] = ["class arrStackX"]
        self.assertTrue(any("arrStackX" in p for p in self.problems()))

    def test_edited_source_is_red(self):
        target = self.pack / "ch03_StackQueue" / "alg3.5" / "arrStack.h"
        target.write_bytes(target.read_bytes().replace(b"getTop", b"top"))
        found = self.problems()
        self.assertTrue(any("被改动" in p and "arrStack.h" in p for p in found), found)

    def test_unregistered_twin_delta_is_red(self):
        del self.data["twin_deltas"]["ch07_Graph/Graph_Dijkstra/Graph_matrix.h"]
        self.assertTrue(any("Graph_matrix.h" in p and "twin_deltas" in p for p in self.problems()))

    def test_stale_twin_delta_is_red(self):
        self.data["twin_deltas"]["ch03_StackQueue/alg3.5/arrStack.h"] = "并不存在的差异"
        self.assertTrue(any("实际相同" in p for p in self.problems()))


class TestFindings(unittest.TestCase):
    """「包里有没有」的结论必须是 grep 结果，而不是一句需要人相信的话。"""

    def setUp(self):
        self.data = json.loads(json.dumps(authorsrc.load_manifest()))

    def by_id(self, fid):
        return next(item for item in self.data["findings"] if item["id"] == fid)

    def test_real_findings_hold(self):
        self.assertEqual(authorsrc.check_findings(self.data), [])

    def test_pattern_that_no_longer_matches_is_red(self):
        self.by_id("E05")["checks"][0]["match"] = ["bool top\\(T& item\\)"]
        self.assertTrue(any("E05" in p and "找不到" in p for p in authorsrc.check_findings(self.data)))

    def test_forbidden_pattern_is_red(self):
        self.by_id("E05")["checks"][0]["no_match"] = ["getTop"]
        self.assertTrue(any("E05" in p and "不该出现" in p for p in authorsrc.check_findings(self.data)))

    def test_every_runtime_erratum_needs_a_verdict(self):
        self.data["findings"] = [f for f in self.data["findings"] if f["id"] != "R04"]
        self.assertTrue(any("R04" in p and "没有「作者代码包里有没有」" in p for p in authorsrc.check_findings(self.data)))

    def test_prose_errata_do_not_need_a_verdict(self):
        # R09 在 2026-09-18 按扫描件改记为 prose（`};` 是空语句），不再要求结论
        kinds = {e["id"]: e["kind"] for e in authorsrc.load_errata()}
        self.assertEqual(kinds["R09"], "prose")
        covered = {eid for f in self.data["findings"] for eid in authorsrc.as_list(f.get("errata"))}
        self.assertNotIn("R09", covered)

    def test_unknown_verdict_is_red(self):
        self.by_id("E01")["verdict"] = "maybe"
        self.assertTrue(any("verdict" in p for p in authorsrc.check_findings(self.data)))


class TestBookSections(unittest.TestCase):
    """附录「考场代码包」三节与勘误里的一节由登记表生成；登记表改了书稿没重写，闸门要红。"""

    def test_book_is_fresh(self):
        self.assertEqual(authorsrc.check_book(authorsrc.load_manifest()), [])

    def test_changed_finding_makes_book_stale(self):
        data = json.loads(json.dumps(authorsrc.load_manifest()))
        data["findings"][0]["summary"] += "（改过）"
        self.assertTrue(any("已过期" in p and "勘误.md" in p for p in authorsrc.check_book(data)))

    def test_code_outside_backticks_is_rejected(self):
        with self.assertRaises(ValueError):
            authorsrc._cell("包里是 Link<T>* top")  # <T> 会被当成 HTML 标签吞掉
        self.assertEqual(authorsrc._cell("包里是 `Link<T>* top`"), "包里是 `Link<T>* top`")


class TestToolchains(unittest.TestCase):
    """包的编译结果随工具链而变（2026-09-18 Codex 在 macOS clang + libc++ 上撞出闸门红）。"""

    def test_programs_follow_local_includes(self):
        # 第 8 章的排序 .cpp 自己不写 main，main 在 #include "SortMain.h" 里
        progs = authorsrc.programs()
        self.assertIn("ch08_Sort/ShellSort/ShSort2.cpp", progs)
        self.assertIn("ch08_Sort/QuickSort/QuickSort.cpp", progs)
        self.assertNotIn("ch08_Sort/sort.h", progs)

    def test_manifest_programs_match_detection(self):
        self.assertEqual(sorted(authorsrc.load_manifest()["programs"]), sorted(authorsrc.programs()))

    def test_baselines_are_per_toolchain(self):
        data = authorsrc.load_manifest()
        self.assertIn("gcc/libstdc++", authorsrc.recorded_toolchains(data))
        self.assertEqual(authorsrc.recorded_toolchains(data)[0], "gcc/libstdc++", "g++ 列排第一：OpenJudge 用它")
        bag = data["programs"]["ch10_Search/BitSet/bag.cpp"]
        self.assertTrue(bag["gcc/libstdc++"]["compiles"])
        if "clang/libc++" in bag:
            self.assertFalse(bag["clang/libc++"]["compiles"], "libc++ 下全局 count 与 std::count 撞名")

    def test_unknown_toolchain_warns_instead_of_failing(self):
        data = json.loads(json.dumps(authorsrc.load_manifest()))
        saved = authorsrc.toolchain_key
        authorsrc.toolchain_key = lambda compiler: "exotic/libfoo"
        warnings = []
        try:
            problems = authorsrc.check_programs(data, ["whatever"], out=warnings.append)
        finally:
            authorsrc.toolchain_key = saved
        self.assertEqual(problems, [])
        self.assertTrue(any("没有登记基线" in w for w in warnings))

    def test_recorded_mismatch_is_red(self):
        data = json.loads(json.dumps(authorsrc.load_manifest()))
        saved_key, saved_sweep = authorsrc.toolchain_key, authorsrc.compile_sweep
        authorsrc.toolchain_key = lambda compiler: "gcc/libstdc++"
        flipped = {rel: {"compiles": not e["gcc/libstdc++"]["compiles"]} for rel, e in data["programs"].items()}
        authorsrc.compile_sweep = lambda compiler: flipped
        try:
            problems = authorsrc.check_programs(data, ["g++"], out=lambda *_: None)
        finally:
            authorsrc.toolchain_key, authorsrc.compile_sweep = saved_key, saved_sweep
        self.assertEqual(len(problems), len(flipped))


class TestHarnessRegistry(unittest.TestCase):
    def test_every_author_diff_is_registered(self):
        registered = set(authorsrc.load_manifest()["harnesses"])
        self.assertEqual(set(authorsrc.harness_files()), registered)
        self.assertGreaterEqual(len(registered), 4)

    def test_unregistered_harness_is_red(self):
        data = json.loads(json.dumps(authorsrc.load_manifest()))
        dropped = sorted(data["harnesses"])[0]
        del data["harnesses"][dropped]
        self.assertTrue(any(dropped in p and "未在 harnesses 里登记" in p
                            for p in authorsrc.harness_registration_problems(data)))

    def test_registered_but_missing_harness_is_red(self):
        data = json.loads(json.dumps(authorsrc.load_manifest()))
        data["harnesses"]["code/ch99/nowhere/author_diff.cpp"] = {"include": ["x"]}
        self.assertTrue(any("ch99" in p and "文件不存在" in p
                            for p in authorsrc.harness_registration_problems(data)))

if __name__ == "__main__":
    unittest.main()
