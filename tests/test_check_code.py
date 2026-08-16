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
        json.dumps({"id": "probe", "title": "闸门探针", "listings": ["算法3.3"], "standard": standard}),
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


if __name__ == "__main__":
    unittest.main()
