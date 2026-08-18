"""代码闸门的单元测试。

这里测的不是「现代化代码对不对」——那是 `code/**/test.cpp` 的事。
这里测的是**闸门本身有没有牙**：一个会挂的单元，check_code 必须判它挂。
一个永远返回绿的闸门比没有闸门更糟，因为它让人以为验证过了。
"""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import check_code  # noqa: E402


SANITIZER_AVAILABLE, _SANITIZER_DIAGNOSTIC = check_code.sanitizer_preflight()


def make_unit(root: Path, test_src, standard="c++20"):
    d = root / "probe"
    d.mkdir(parents=True)
    (d / "unit.json").write_text(
        json.dumps({"id": "probe", "title": "闸门探针", "listings": [
            {"id": "算法3.3", "anchor": "#pragma once", "test": "int main"}
        ], "standard": standard}),
        encoding="utf-8",
    )
    # 探针也必须长得像个真单元：D-007 的实质性检查对所有单元一视同仁，
    # 不给测试夹具开例外——否则「干净单元能通过」这条断言就名不副实了。
    (d / "legacy.md").write_text(
        "# 闸门探针的 legacy.md\n\n"
        "这是 tests/ 造的合成单元，用来验证闸门本身的行为，不对应原书任何清单。\n"
        "内容按 D-007 的形状写足，是因为实质性检查不接受例外——\n"
        "一个能通过闸门的『干净单元』，必须真的能通过全部判据。\n\n"
        "## 证据（形状示例）\n\n"
        "```console\n"
        "$ g++ -std=c++17 -c probe.cpp\n"
        "probe.cpp:1:1: error: 这是探针用的示例输出，不是真实缺陷\n"
        "```\n\n"
        "## 为什么要写这么长\n\n"
        "D-007 要求 legacy.md 至少 20 行实质内容且含可复现证据。\n"
        "红线的原话是「每条缺陷都要有证据」——两行说明不构成证据。\n"
        "探针若能靠一行蒙混过关，那条判据对真单元也就形同虚设。\n\n"
        "## 与真单元的差别\n\n"
        "真单元的这份文件要逐条摘录原书清单、逐条附编译器或 sanitizer 的真实输出。\n"
        "参见 code/ch03/array_stack/legacy.md 或 code/ch04/pattern_matching/legacy.md。\n"
        "本文件只是形状占位，不含任何关于原书的断言。\n\n"
        "## 这个夹具为什么不走豁免\n\n"
        "check_substance() 没有 waiver 字段，这是有意的：\n"
        "一旦开了逃生口，最先用它的就是最该被拦下的那类提交。\n"
        "夹具自己达标，比给夹具开后门更能说明判据是真的。\n",
        encoding="utf-8",
    )
    (d / "modern.hpp").write_text("#pragma once\n", encoding="utf-8")
    (d / "test.cpp").write_text(test_src, encoding="utf-8")
    return d


def run_gate(unit_dir: Path, allow_degraded=None):
    """跑闸门。sanitizer 环境不可用时自动加 --allow-degraded。

    否则 check_code 会以退出码 2（环境问题）返回，那些**与 sanitizer 无关**的
    门牙判据（断言失败要报、-Werror 要生效）就无从验证——而那正是 macOS 上
    唯一还能验的两条。降级只丢掉 sanitizer 档，不丢掉这两条。
    """
    degraded = (not SANITIZER_AVAILABLE) if allow_degraded is None else allow_degraded
    proc = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "check_code.py"), str(unit_dir)]
        + (["--allow-degraded"] if degraded else []),
        capture_output=True,
        text=True,
        timeout=180,
    )
    return proc.returncode, proc.stdout + proc.stderr


@unittest.skipIf(check_code.compiler() is None, "机器上没有 C++ 编译器")
class TestGateHasTeeth(unittest.TestCase):
    """注意 skip 的粒度：只有真正依赖 sanitizer 的那几条才 skip。

    前两条（会挂的断言要被报出来、-Werror 要真的生效）与 sanitizer 无关，
    在任何机器上都必须跑——把整个类一起 skip 掉，等于在没有 sanitizer 的机器上
    对「闸门有没有牙」变成零覆盖。
    """

    def test_failing_assertion_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), 'int main() { return 1; }  // 测试失败\n')
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertIn("测试失败", out)

    def test_warning_is_an_error(self):
        """-Werror 必须真的生效：教材代码最爱的有符号/无符号比较不能放过。"""
        src = "int main() { int i = -1; unsigned u = 1; return i < u ? 0 : 1; }\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertIn("编译失败", out)

    @unittest.skipUnless(SANITIZER_AVAILABLE, "本条依赖 sanitizer；环境诊断由 TestSanitizerPreflight 覆盖")
    def test_sanitizer_catches_heap_overflow(self):
        """越界必须被拦住——而且要注意：release-O2 那档它是静默通过的。

        实测这条先被 UBSan 拦下（`-fno-sanitize-recover=all` 让它当场 abort，
        ASan 还来不及报），所以判据认「两个 sanitizer 任一开口」，不写死 ASan。
        """
        src = "int main() { int* p = new int[3]; p[3] = 1; int v = p[3]; delete[] p; return v - 1; }\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertTrue(
            "AddressSanitizer" in out or "runtime error" in out,
            f"越界没有被任何 sanitizer 抓到：\n{out}",
        )
        self.assertIn("✅ [release-O2]", out, "这正是只跑 -O2 会漏掉 UB 的证据")

    @unittest.skipUnless(SANITIZER_AVAILABLE, "本条依赖 sanitizer；环境诊断由 TestSanitizerPreflight 覆盖")
    def test_asan_catches_use_after_free(self):
        """这条 UBSan 管不着，只有 ASan 能抓——用来证明 ASan 确实挂上了。"""
        src = "int main() { int* p = new int[3]; delete[] p; return p[0]; }\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertIn("AddressSanitizer", out)

    @unittest.skipUnless(SANITIZER_AVAILABLE, "本条依赖 sanitizer；环境诊断由 TestSanitizerPreflight 覆盖")
    def test_clean_unit_passes_both_profiles(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(
                Path(tmp),
                '#include <cstdio>\nint main() { std::printf("Probe: 5 项断言，0 失败\\n"); return 0; }\n',
            )
            code, out = run_gate(unit)
        self.assertEqual(code, 0, out)
        self.assertIn("debug+asan+ubsan", out)
        self.assertIn("release-O2", out)


class TestD001StaticCheck(unittest.TestCase):
    """D-001 是人拍板的公约。公约要能被机器守住，否则第 10 个单元就开始漂。"""

    def unit_with(self, tmp, header_src, meta_extra=None):
        d = make_unit(Path(tmp), "int main() { return 0; }\n")
        (d / "modern.hpp").write_text(header_src, encoding="utf-8")
        if meta_extra:
            meta = json.loads((d / "unit.json").read_text(encoding="utf-8"))
            meta.update(meta_extra)
            (d / "unit.json").write_text(json.dumps(meta, ensure_ascii=False), encoding="utf-8")
        return d, json.loads((d / "unit.json").read_text(encoding="utf-8"))

    def test_iostream_in_implementation_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, "#include <iostream>\nvoid f() { std::cout << 1; }\n")
            problems = check_code.check_d001(d, meta)
        self.assertTrue(any("D-001§3" in p for p in problems))
        self.assertEqual(len([p for p in problems if "D-001§3" in p]), 2, "头文件与调用各报一条")

    def test_stl_container_in_implementation_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, "#include <vector>\n")
            problems = check_code.check_d001(d, meta)
        self.assertTrue(any("D-001§2" in p for p in problems))

    def test_allowed_headers_pass(self):
        src = "#include <optional>\n#include <type_traits>\n#include <cstddef>\n"
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, src)
            self.assertEqual(check_code.check_d001(d, meta), [])

    def test_comment_mentioning_cout_is_not_a_violation(self):
        """legacy 对照说明里提到 std::cout 是正当的，别把注释判红。"""
        src = "// 原书这里写 std::cout << \"栈满溢出\";\nint x = 0;\n"
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, src)
            self.assertEqual(check_code.check_d001(d, meta), [])

    def test_block_comment_and_string_literals_are_not_violations(self):
        src = '/* std::cout << 1; */\nconst char* text = "#include <vector>";\n'
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, src)
            self.assertEqual(check_code.check_d001(d, meta), [])

    def test_whitespace_obfuscated_forbidden_tokens_are_rejected(self):
        src = "#  include   <vector>\nvoid f() { std :: cout << 1; }\n"
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(tmp, src)
            problems = check_code.check_d001(d, meta)
        self.assertEqual(len(problems), 2, problems)

    def test_blank_exception_reason_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(
                tmp, "#include <vector>\n", {"d001_exceptions": {"#include<vector>": " "}}
            )
            self.assertTrue(check_code.check_d001(d, meta))

    def test_documented_exception_is_honoured(self):
        with tempfile.TemporaryDirectory() as tmp:
            d, meta = self.unit_with(
                tmp,
                "#include <vector>\n",
                {"d001_exceptions": {"#include <vector>": "本节讲的是图的邻接表，vector 只做外层容器"}},
            )
            self.assertEqual(check_code.check_d001(d, meta), [])

    def test_real_seed_unit_complies(self):
        unit = ROOT / "code" / "ch03" / "array_stack"
        meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
        self.assertEqual(check_code.check_d001(unit, meta), [])


@unittest.skipIf(check_code.compiler() is None, "机器上没有 C++ 编译器")
class TestSanitizerPreflight(unittest.TestCase):
    """红队那轮在 macOS 上撞到「连空探针都触发 ASan 初始化错误」。

    闸门必须能把「环境跑不起来」和「代码坏了」分开说——否则读日志的人
    会一个个单元去排除一个根本不在单元里的问题。
    """

    def test_preflight_passes_on_a_working_toolchain(self):
        ok, out = check_code.sanitizer_preflight()
        if not ok:
            self.skipTest(f"本机 sanitizer 环境不可用：{out}")
        self.assertTrue(ok)

    def test_preflight_reports_a_broken_environment(self):
        ok, out = check_code.sanitizer_preflight(flags=["-fsanitize=definitely-not-a-sanitizer"])
        self.assertFalse(ok)
        self.assertIn("空探针", out, "诊断必须点明是探针失败，不是某个单元失败")

    def test_environment_failure_uses_a_distinct_exit_code(self):
        """环境问题退出码 2，代码问题退出码 1——交接方一眼能分清。"""
        import inspect

        src = inspect.getsource(check_code.main)
        self.assertIn("sys.exit(2)", src)
        self.assertIn("--allow-degraded", check_code.main.__doc__ or src)

    def test_thin_test_warns_in_degraded_mode(self):
        """降级档下断言太少要亮黄灯——sanitizer 没跑，Release-O2 覆盖不了内存与 UB。

        这条规则是给 macOS 那种 sanitizer 起不来的环境准备的：那边看到「0 failures」
        最容易收工，而三五条断言撑不住一个单元的说法。

        断言数取 7：在 D-007 的密度下限（5）之上、新下限（10）之下，
        这样亮的一定是这盏灯，而不是「断言密度不足」那条。
        直接调 build_and_run 而不走子进程——`--allow-degraded` 只是*允许*降级，
        本机 sanitizer 跑得起来时根本不会降级，那条路径在 Linux 上测不到。
        """
        src = ('#include <cstdio>\n'
               'int main(){ std::printf("Probe: 7 项断言，0 失败\\n"); return 0; }\n')
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            work = Path(tmp) / "work"
            work.mkdir(parents=True, exist_ok=True)
            ok, logs = check_code.build_and_run(
                unit, work, profiles=[check_code.PROFILES[-1]], degraded=True
            )
        text = "\n".join(logs)
        self.assertIn("降级档测试偏薄", text)
        self.assertIn("只有 7 项断言", text)
        self.assertTrue(ok, text)   # 黄灯不是红灯：单元本身仍然通过

    def test_thin_test_is_silent_when_sanitizer_runs(self):
        """sanitizer 正常跑得起来时不该亮这盏灯——那时 Release-O2 不是唯一证据。"""
        src = ('#include <cstdio>\n'
               'int main(){ std::printf("Probe: 7 项断言，0 失败\\n"); return 0; }\n')
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            work = Path(tmp) / "work"
            work.mkdir(parents=True, exist_ok=True)
            ok, logs = check_code.build_and_run(
                unit, work, profiles=[check_code.PROFILES[-1]], degraded=False
            )
        text = "\n".join(logs)
        self.assertNotIn("降级档测试偏薄", text)
        self.assertTrue(ok, text)

    def test_thick_test_does_not_warn_even_when_degraded(self):
        """断言够多就不该被打扰——黄灯只针对『薄』，不是针对『降级』本身。"""
        src = ('#include <cstdio>\n'
               'int main(){ std::printf("Probe: 40 项断言，0 失败\\n"); return 0; }\n')
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), src)
            work = Path(tmp) / "work"
            work.mkdir(parents=True, exist_ok=True)
            ok, logs = check_code.build_and_run(
                unit, work, profiles=[check_code.PROFILES[-1]], degraded=True
            )
        text = "\n".join(logs)
        self.assertNotIn("降级档测试偏薄", text)
        self.assertTrue(ok, text)

    def test_degraded_mode_is_loud(self):
        """降级不能悄悄变绿：结论旁边必须再喊一次。"""
        import inspect

        src = inspect.getsource(check_code.main)
        self.assertIn("降级", src)
        self.assertIn("不代表内存与 UB 干净", src)


class TestTeachingAndDemoAreVerified(unittest.TestCase):
    """D-012 的教学版分层，闸门这一半有没有牙。

    这层的风险很具体：**教学版是书稿正文整块印出来、读者最可能照抄的那一份**。
    它一旦成了「书里印着、闸门不管」的代码，就退回了本项目开工时要修的那个问题。
    `demo.cpp` 在 D-012 之前正是这种状态——R3 保证它和文件逐字一致，
    但从来没有任何一步编译过它。
    """

    def test_teaching_header_without_test_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching.hpp").write_text("#pragma once\n", encoding="utf-8")
            _, problems = check_code.unit_programs(unit, [str(unit / "test.cpp")])
        self.assertTrue(any("teaching_test.cpp" in p for p in problems), problems)

    def test_teaching_test_without_header_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching_test.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            _, problems = check_code.unit_programs(unit, [str(unit / "test.cpp")])
        self.assertTrue(any("teaching.hpp" in p for p in problems), problems)

    def test_teaching_and_demo_become_their_own_programs(self):
        """三份源码各有自己的 main，必须编成三个可执行文件而不是链在一起。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching.hpp").write_text("#pragma once\n", encoding="utf-8")
            (unit / "teaching_test.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            (unit / "demo.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            programs, problems = check_code.unit_programs(unit, [str(unit / "test.cpp")])
        self.assertEqual(problems, [])
        self.assertEqual([kind for kind, _ in programs], ["test", "teaching", "demo"])
        for _, srcs in programs:
            self.assertEqual(len(srcs), 1, "每个产物只带自己那一个 main")

    def test_thin_teaching_test_is_rejected(self):
        """教学版少考虑几件事是 D-012 的取舍；少验几条不是。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching.hpp").write_text("#pragma once\n", encoding="utf-8")
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            problems = check_code.check_substance(unit, meta, assertions=30, teaching_assertions=3)
        self.assertTrue(any("教学版只有 3 项断言" in p for p in problems), problems)

    def test_adequate_teaching_test_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching.hpp").write_text("#pragma once\n", encoding="utf-8")
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            problems = check_code.check_substance(unit, meta, assertions=30, teaching_assertions=30)
        self.assertEqual(problems, [])

    def test_d001_covers_the_teaching_header(self):
        """教学版可以少写 noexcept，但容器里照样一个 cout 都不许有。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(Path(tmp), "int main() { return 0; }\n")
            (unit / "teaching.hpp").write_text(
                "#include <iostream>\nvoid f() { std::cout << 1; }\n", encoding="utf-8"
            )
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            problems = check_code.check_d001(unit, meta)
        self.assertTrue(any("teaching.hpp" in p for p in problems), problems)

    @unittest.skipIf(check_code.compiler() is None, "机器上没有 C++ 编译器")
    def test_broken_demo_turns_the_gate_red(self):
        """这一条守的就是 D-012 之前那个洞：demo 编不过必须红。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(
                Path(tmp),
                '#include <cstdio>\nint main() { std::printf("Probe: 5 项断言，0 失败\\n"); return 0; }\n',
            )
            (unit / "demo.cpp").write_text("int main() { return nonexistent(); }\n", encoding="utf-8")
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertIn("/demo]", out)
        self.assertIn("编译失败", out)

    @unittest.skipIf(check_code.compiler() is None, "机器上没有 C++ 编译器")
    def test_demo_that_exits_nonzero_turns_the_gate_red(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = make_unit(
                Path(tmp),
                '#include <cstdio>\nint main() { std::printf("Probe: 5 项断言，0 失败\\n"); return 0; }\n',
            )
            (unit / "demo.cpp").write_text("int main() { return 3; }\n", encoding="utf-8")
            code, out = run_gate(unit)
        self.assertEqual(code, 1)
        self.assertIn("demo 运行失败", out)


class TestListingBindings(unittest.TestCase):
    def test_rejects_bare_string_comment_and_duplicates(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp)
            (unit / "modern.hpp").write_text("// comment-only\nvoid real_anchor() {}\n", encoding="utf-8")
            (unit / "test.cpp").write_text("test_one test_two\n", encoding="utf-8")
            meta = {"listings": [
                "算法1.1",
                {"id": "算法1.2", "anchor": "comment-only", "test": "test_one"},
                {"id": "算法1.3", "anchor": "void real_anchor()", "test": "test_two"},
                {"id": "算法1.4", "anchor": "void real_anchor()", "test": "test_two"},
            ]}
            problems = check_code.check_listing_bindings(unit, meta)
        joined = "\n".join(problems)
        self.assertIn("清单绑定格式错误", joined)
        self.assertIn("只存在于注释行", joined)
        self.assertIn("实现锚点与同单元其他清单重复", joined)
        self.assertIn("测试锚点与同单元其他清单重复", joined)

    def test_removing_algorithm_6_10_implementation_names_missing_anchor(self):
        """重放 2026-08-16 的冒领：删完整实现，必须先由 T-025 具名报红。"""
        source_dir = ROOT / "code" / "ch06" / "general_tree"
        meta = json.loads((source_dir / "unit.json").read_text(encoding="utf-8"))
        anchor = next(e["anchor"] for e in meta["listings"] if e["id"] == "算法6.10")
        source = (source_dir / "modern.hpp").read_text(encoding="utf-8")
        start = source.index(anchor)
        start = source.rfind("\n", 0, start) + 1
        brace = source.index("{", start)
        depth, end = 0, brace
        while end < len(source):
            if source[end] == "{": depth += 1
            elif source[end] == "}":
                depth -= 1
                if depth == 0:
                    end += 1
                    break
            end += 1
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp)
            (unit / "modern.hpp").write_text(source[:start] + source[end:], encoding="utf-8")
            (unit / "test.cpp").write_text(
                (source_dir / "test.cpp").read_text(encoding="utf-8"), encoding="utf-8")
            problems = check_code.check_listing_bindings(unit, meta)
        self.assertIn(
            f"  ❌ 算法6.10 的实现锚点不存在：{anchor}", problems,
            "必须由绑定闸门具名定位，不能等编译失败",
        )


class TestOutputTruncation(unittest.TestCase):
    def test_keeps_head_and_tail(self):
        """噪声多的失败输出里，最后那句 FAIL 不能被顶掉。"""
        text = "\n".join([f"noise {i}" for i in range(500)] + ["FAIL: 关键的一行"])
        out = check_code.indent(text)
        self.assertIn("FAIL: 关键的一行", out)
        self.assertIn("中间省略", out)
        self.assertIn("noise 0", out)

    def test_short_output_untouched(self):
        self.assertNotIn("省略", check_code.indent("a\nb\nc"))


class TestD025PythonArm(unittest.TestCase):
    """D-025 的 Python 臂：和 C++ 侧一样，闸门要有牙。

    这里不测「排序写得对不对」——那是 code/ch08/sorting/test.py 的事。
    这里测的是：一段把整章委托给标准库的 Python，闸门必须判红；
    一条没给 Python 锚点又没写 py_skip 的清单，闸门必须判红。
    """

    def _unit(self, root: Path, modern_py, listings=None, exceptions=None):
        d = root / "pyprobe"
        d.mkdir(parents=True)
        meta = {
            "id": "pyprobe",
            "title": "Python 臂探针",
            "listings": listings if listings is not None else [
                {"id": "算法8.1", "anchor": "#pragma once", "test": "int main",
                 "py_code_line": "def demo(", "py_test": "demo works"}
            ],
        }
        if exceptions:
            meta["d025_exceptions"] = exceptions
        (d / "unit.json").write_text(json.dumps(meta, ensure_ascii=False), encoding="utf-8")
        (d / "modern.py").write_text(modern_py, encoding="utf-8")
        (d / "test.py").write_text("# demo works\nprint('1 项断言，0 失败')\n", encoding="utf-8")
        return d

    def test_stdlib_shortcut_is_rejected(self):
        """`sorted()` / `heapq` 一行就是一整章，实现文件里出现即判红。"""
        for source, token in (
            ("def demo(values):\n    return sorted(values)\n", "sorted"),
            ("def demo(values):\n    values.sort()\n", "sort"),
            ("import heapq\n\n\ndef demo(values):\n    return values\n", "heapq"),
            ("import bisect\n\n\ndef demo(values):\n    return values\n", "bisect"),
        ):
            with tempfile.TemporaryDirectory() as tmp, self.subTest(token=token):
                unit = self._unit(Path(tmp), source)
                meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
                problems = check_code.check_d025(unit, meta)
                self.assertTrue(
                    any(f"`{token}`" in p for p in problems),
                    f"实现里的 {token} 应当被 D-025 拦下，实得：{problems}",
                )

    def test_ban_is_per_name_not_per_module(self):
        """D-025 §2b：该封的是绕过某一节课的名字，不是整个 collections。

        封整个模块的代价是第 7 章图算法一上来就得为 defaultdict 写豁免，
        而豁免写多了，名单就从判据退化成手续。
        """
        allowed = (
            "from collections import defaultdict",          # 不删任何一节，正当管道
            "from collections import OrderedDict",          # 本书没有对应的课；dict 本身就保序
            "from collections import namedtuple",
            "import collections",                           # 光 import 不用，不算违规
        )
        for source in allowed:
            with tempfile.TemporaryDirectory() as tmp, self.subTest(source=source):
                unit = self._unit(Path(tmp), source + "\n\n\ndef demo(v):\n    return v\n")
                meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
                self.assertEqual(check_code.check_d025(unit, meta), [],
                                 f"{source} 不该被拦——它不删本书任何一节课")

    def test_names_that_do_bypass_a_lesson_are_banned_in_every_spelling(self):
        """deque 删 3.2、Counter 删 8.6.1 与 5.6；换个写法不能绕过去。"""
        banned = (
            ("from collections import deque", "deque"),
            ("from collections import deque as dq", "deque"),
            ("import collections\n\n\ndef demo(v):\n    return collections.deque()", "deque"),
            ("from collections import Counter", "Counter"),
        )
        for source, token in banned:
            with tempfile.TemporaryDirectory() as tmp, self.subTest(source=source):
                unit = self._unit(Path(tmp), source + "\n\n\ndef demo(v):\n    return v\n")
                meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
                problems = check_code.check_d025(unit, meta)
                self.assertTrue(any(f"`{token}`" in p for p in problems),
                                f"{source} 应当被拦，实得：{problems}")

    def test_sort_is_position_sensitive(self):
        """`sort` 只在被调用时算：形参或局部变量叫 sort 完全正当。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), "def demo(sort, values):\n    return sort(values)\n")
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertEqual(check_code.check_d025(unit, meta), [])

    def test_unit_can_add_its_own_bans(self):
        """dict/set 全局封不现实，但第 10 章讲闭散列表时必须封（D-025 §2b）。"""
        source = "def demo(values):\n    table = dict()\n    return table\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), source)
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertEqual(check_code.check_d025(unit, meta), [], "dict 默认不该被封")
            meta["d025_forbidden"] = {"dict": "算法10.9–10.12 闭散列表就是本节的课"}
            problems = check_code.check_d025(unit, meta)
            self.assertTrue(any("`dict`" in p for p in problems), problems)

    def test_unit_ban_must_carry_a_reason(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), "def demo(v):\n    return v\n")
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            meta["d025_forbidden"] = {"dict": "  "}
            self.assertTrue(check_code.check_d025(unit, meta))

    def test_comment_mentioning_sorted_is_not_a_violation(self):
        """判据走 AST 而不是正则：注释和字符串里出现 `sorted` 不该报红。"""
        source = (
            "# 这里刻意不调用 sorted()，理由见 D-025\n"
            'DOC = "不要用 sorted"\n\n\n'
            "def demo(values):\n    return values\n"
        )
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), source)
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertEqual(check_code.check_d025(unit, meta), [])

    def test_written_exception_lifts_the_ban(self):
        """和 d001_exceptions 一样：可以豁免，但必须写理由。"""
        source = "def demo(values):\n    return sorted(values)\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), source,
                              exceptions={"sorted": "本单元讲的是稳定性判定，不是排序本身"})
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertEqual(check_code.check_d025(unit, meta), [])

    def test_empty_exception_reason_does_not_lift_the_ban(self):
        source = "def demo(values):\n    return sorted(values)\n"
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), source, exceptions={"sorted": "   "})
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertTrue(check_code.check_d025(unit, meta))

    def test_every_listing_needs_a_python_verdict(self):
        """有 modern.py 就不许沉默：每条清单要么给锚点，要么写 py_skip 说明理由。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(
                Path(tmp), "def demo(values):\n    return values\n",
                listings=[
                    {"id": "算法8.1", "anchor": "#pragma once", "test": "int main",
                     "py_code_line": "def demo(", "py_test": "demo works"},
                    {"id": "算法8.2", "anchor": "#pragma once", "test": "int main"},
                ],
            )
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            problems = check_code.check_python_bindings(unit, meta)
            self.assertTrue(any("算法8.2" in p for p in problems), problems)

    def test_py_skip_must_carry_a_reason(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(
                Path(tmp), "def demo(values):\n    return values\n",
                listings=[{"id": "算法8.1", "anchor": "#pragma once", "test": "int main",
                           "py_skip": ""}],
            )
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            self.assertTrue(check_code.check_python_bindings(unit, meta))

    def test_anchor_must_exist_outside_comments(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), "# def demo(values)\ndef other(v):\n    return v\n")
            meta = json.loads((unit / "unit.json").read_text(encoding="utf-8"))
            problems = check_code.check_python_bindings(unit, meta)
            self.assertTrue(any("只存在于注释行" in p or "不存在" in p for p in problems), problems)

    def test_modern_py_without_test_py_is_rejected(self):
        """Python 实现不能是唯一没人验的代码。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), "def demo(values):\n    return values\n")
            (unit / "test.py").unlink()
            ok, logs, _ = check_code.run_python(unit)
            self.assertFalse(ok)
            self.assertTrue(any("没有 test.py" in line for line in logs), logs)

    def test_optimized_out_asserts_profile_is_not_used(self):
        """`-O` 会把 assert 剥掉，那一档下测试恒绿——它必须不在名单里。"""
        for _, flags in check_code.PY_PROFILES:
            self.assertNotIn("-O", flags)
            self.assertNotIn("-OO", flags)

    def test_failing_python_test_turns_the_unit_red(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), "def demo(values):\n    return values\n")
            (unit / "test.py").write_text(
                "# demo works\nimport sys\nprint('1 项断言，1 失败')\nsys.exit(1)\n",
                encoding="utf-8",
            )
            ok, logs, _ = check_code.run_python(unit)
            self.assertFalse(ok, logs)

class TestD026PythonStyle(unittest.TestCase):
    """D-026：书上印的 Python 不能比课件还难读，三条机器守住。

    每条都配一个「会红」的用例和一个「不该红」的用例——只测通过路径的
    检查器等于没有检查器（tests/test_check_doc.py 开头那句话，这里同样适用）。
    """

    def _check(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "probe.py"
            path.write_text(source, encoding="utf-8")
            return check_code.check_pystyle(path)

    def test_semicolon_is_rejected(self):
        problems = self._check("a = 1; b = 2\n")
        self.assertTrue(any("一行一句" in p for p in problems), problems)

    def test_semicolon_inside_string_or_comment_is_fine(self):
        """判据走 tokenize，不是字符串查找。"""
        self.assertEqual(self._check('TEXT = "a; b"  # 这里也有 ; 号\n'), [])

    def test_single_line_compound_is_rejected(self):
        for source in ("if True: pass\n",
                       "for i in range(3): print(i)\n",
                       "def f(): return 1\n",
                       "class A: pass\n"):
            with self.subTest(source=source):
                problems = self._check(source)
                self.assertTrue(any("另起一行" in p for p in problems), problems)

    def test_multiline_compound_is_fine(self):
        self.assertEqual(self._check("def f():\n    return 1\n"), [])

    def test_assignment_spacing_is_rejected(self):
        for source in ("a=1\n", "a =1\n", "a+= 1\n"):
            with self.subTest(source=source):
                problems = self._check(source)
                self.assertTrue(any("空格" in p for p in problems), problems)

    def test_annotated_assignment_is_not_a_false_positive(self):
        """`result: list[int] = []` 完全合规。

        第一版把左边界取成**目标**而不是**注解**，12 处全仓假阳性都出在这里
        （2026-08-18 实测）。这条断言就是那次的凭据。
        """
        self.assertEqual(self._check("result: list[int] = []\n"), [])

    def test_keyword_argument_and_default_are_not_assignments(self):
        self.assertEqual(self._check("def f(x=1):\n    return f(x=2)\n"), [])

    def test_dict_literal_is_not_a_compound_statement(self):
        self.assertEqual(self._check('d = {"k": 1}\n'), [])

    def test_repo_python_is_clean(self):
        """全仓实跑：这条一旦红，就是有人又把书上要印的代码压成了单行。"""
        offenders = {}
        for path in sorted(list((ROOT / "code").rglob("modern.py"))
                           + list((ROOT / "code").rglob("test.py"))):
            found = check_code.check_pystyle(path)
            if found:
                offenders[str(path.relative_to(ROOT))] = len(found)
        self.assertEqual(offenders, {}, f"D-026 风格问题：{offenders}")


class TestAssertionCountMustBeComputed(unittest.TestCase):
    """`N 项断言` 里的 N 是闸门读的那个数，它必须是算出来的。

    缘由（2026-08-18）：`code/ch12/trie/test.py` 印的是 `print("12 项断言")`——
    一个字面量。删掉一半断言它照报 12，密度闸门被自己读的那行字架空。
    """

    def _unit(self, root, test_source):
        d = root / "probe"
        d.mkdir(parents=True)
        (d / "test.py").write_text(test_source, encoding="utf-8")
        return d

    def test_literal_count_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), 'print("12 项断言")\n')
            problems = check_code.check_reported_count_is_computed(unit)
            self.assertTrue(any("写死" in p for p in problems), problems)

    def test_computed_count_is_accepted(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit = self._unit(Path(tmp), 'checks = 3\nprint(f"{checks} 项断言")\n')
            self.assertEqual(check_code.check_reported_count_is_computed(unit), [])

    def test_repo_tests_all_compute_their_count(self):
        offenders = [str(p.parent.relative_to(ROOT))
                     for p in sorted((ROOT / "code").rglob("test.py"))
                     if check_code.check_reported_count_is_computed(p.parent)]
        self.assertEqual(offenders, [], f"断言数写死的单元：{offenders}")


class TestUnitLevelPySkip(unittest.TestCase):
    """整个单元有意不给 Python（D-026 §2）。

    清单级 py_skip 挂在 listings[] 上，而「原书无对应清单」的单元没有 listings
    可挂——ch11/bplus_tree 正是这种形状，决定必须另有落脚处。
    """

    def _unit(self, root, meta_extra, with_python):
        d = root / "probe"
        d.mkdir(parents=True)
        meta = {"id": "probe", "title": "探针", "listings": []}
        meta.update(meta_extra)
        (d / "unit.json").write_text(json.dumps(meta, ensure_ascii=False), encoding="utf-8")
        if with_python:
            (d / "modern.py").write_text("def demo():\n    return 1\n", encoding="utf-8")
            (d / "test.py").write_text('checks = 1\nprint(f"{checks} 项断言")\n', encoding="utf-8")
        return d, meta

    def test_declared_skip_without_python_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit, meta = self._unit(Path(tmp), {"py_skip": "本节讲页式存储管理"}, False)
            ok, logs, _ = check_code.run_python(unit, meta=meta)
            self.assertTrue(ok, logs)
            self.assertTrue(any("不提供 Python" in line for line in logs), logs)

    def test_declared_skip_with_python_is_rejected(self):
        """说了不给，却还留着 modern.py——两份事实互相打架，必须判红。"""
        with tempfile.TemporaryDirectory() as tmp:
            unit, meta = self._unit(Path(tmp), {"py_skip": "本节讲页式存储管理"}, True)
            ok, logs, _ = check_code.run_python(unit, meta=meta)
            self.assertFalse(ok, logs)

    def test_empty_skip_reason_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            unit, meta = self._unit(Path(tmp), {"py_skip": "   "}, False)
            ok, _, _ = check_code.run_python(unit, meta=meta)
            self.assertFalse(ok)


if __name__ == "__main__":
    unittest.main()
