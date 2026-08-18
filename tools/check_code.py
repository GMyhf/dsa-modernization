#!/usr/bin/env python3
"""check_code.py — 把 code/ 下每个单元真正编译、真正跑一遍。

原书的 C++ 是 2008 年的教材写法：裸 `new[]`、没有析构、`bool pop(T& item)` 用出参、
`int` 当下标、没有 const 正确性、更没有测试。现代化之后如果只是「看起来更现代」，
那就什么都没证明。这个脚本是本项目最硬的仲裁：

  1. 严格编译：-Wall -Wextra -Wpedantic -Werror（教材代码最爱的隐式转换、
     未使用参数、有符号/无符号比较，在这里全部是错误）
  2. Debug 构建带 ASan + UBSan，-fno-sanitize-recover=all：越界和 UB 当场崩
  3. Release 构建 -O2 再跑一遍：只在某个优化档下成立的测试不算数
  4. test.cpp 自带断言，退出码非 0 即失败
  5. D-025：单元若有 modern.py，还要跑 test.py 两档（默认档 / `-X dev -W error`），
     并按 D-025 的名单查「一行把这一章删掉」的标准库调用

用法:
  python3 tools/check_code.py                    # 全部单元
  python3 tools/check_code.py code/ch03/array_stack
  python3 tools/check_code.py --keep             # 保留 .build/ 便于手工复现
"""
import argparse
import ast
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import tokenize
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具

CODE = ROOT / "code"

BASE_FLAGS = ["-Wall", "-Wextra", "-Wpedantic", "-Werror"]
PROFILES = [
    (
        "debug+asan+ubsan",
        ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
    ),
    ("release-O2", ["-O2"]),
]
TIMEOUT_SEC = 120


def compiler():
    return shutil.which("g++") or shutil.which("clang++")


# D-001（collab/DECISION_LOG.md，人已拍板）里能被机器守住的两条：
#   第 3 条：数据结构类内部严禁 I/O；
#   第 2 条：不得用 STL 容器直接替代该章节要讲的手写实现。
# 只查 modern.*（实现），不查 test.cpp——测试里用 vector/iostream 是正当的。
D001_FORBIDDEN = [
    (re.compile(r"#\s*include\s*<(iostream|cstdio)>"), "D-001§3 实现文件不得引入 I/O 头文件"),
    (re.compile(r"\bstd::(cout|cerr|clog|printf)\b"), "D-001§3 数据结构类内部严禁 I/O"),
    (
        re.compile(r"#\s*include\s*<(vector|stack|queue|deque|list|forward_list|map|set|"
                   r"unordered_map|unordered_set)>"),
        "D-001§2 不得用 STL 容器替代本章要讲的手写实现",
    ),
]


# ── D-025：Python 臂 ────────────────────────────────────────────────────────
#
# C++ 侧有两档构建（sanitizer 与 -O2），Python 没有 sanitizer 可跑，但**有**一档
# 真正会多抓东西的模式：`-X dev` 打开开发模式（调试用内存分配器、更严的默认警告
# 过滤器），`-W error` 把 DeprecationWarning 这类「现在能跑、下一版就不能跑」
# 的东西变成失败。
#
# 反过来，`-O` 档**故意不跑**：`-O` 会把 assert 语句整个剥掉，那一档下测试恒绿。
# 一个永远不会红的档不是第二重保险，是伪证。
PY_PROFILES = [
    ("py-default", []),
    ("py-dev", ["-X", "dev", "-W", "error"]),
]
SHARED_CASE_RE = re.compile(r"共享用例\s*[:：]\s*(\d+)")
SHARED_ERROR_KINDS = {"", "invalid_argument", "out_of_range"}


def shared_case_total(unit_dir: Path, meta):
    """返回 cases.tsv 的有效用例数；双实现单元缺表即报错。"""
    has_both = (unit_dir / "modern.py").is_file() and not meta.get("py_skip")
    path = unit_dir / "cases.tsv"
    if not path.is_file():
        return (None, "  ❌ 双实现单元缺 cases.tsv（T-047）") if has_both else (None, None)
    lines = [line for line in path.read_text(encoding="utf-8").splitlines()
             if line.strip() and not line.lstrip().startswith("#")]
    if not lines or lines[0].split("\t") != ["name", "operation", "input", "expected", "expected_error"]:
        return None, "  ❌ cases.tsv 首行必须是 T-047 的五列表头"
    total = len(lines) - 1
    if total < 1:
        return None, "  ❌ cases.tsv 没有任何共享用例"
    for number, line in enumerate(lines[1:], 2):
        fields = line.split("\t")
        if len(fields) != 5:
            return None, f"  ❌ cases.tsv:{number} 不是五列 TSV"
        if fields[4] not in SHARED_ERROR_KINDS:
            return None, f"  ❌ cases.tsv:{number} 的异常类别未知：{fields[4]}"
    return total, None


def check_shared_loader(unit_dir: Path):
    """有 cases.tsv 的单元必须走 `code/support/shared_cases.*` 读表，不许自己解析。

    缘由（2026-08-18）：`ch08/sorting` 交来时两种语言各手抄了一份解析。它能跑，
    但两份抄本对同一张表的读法**并不相同**——C++ 那份对列数只补不校
    （`while (fields.size() < 5) emplace_back()`，六列的行照收），
    Python 那份按五元组解包、当场抛。表一旦加列，两边就静默分家，
    而「共享用例」这四个字正是靠两边读同一张表才成立的。
    """
    if not (unit_dir / "cases.tsv").is_file():
        return []
    problems = []
    for name, needle in (("test.cpp", "shared_cases::load"), ("test.py", "shared_cases.load")):
        path = unit_dir / name
        if not path.is_file():
            continue
        if needle not in path.read_text(encoding="utf-8"):
            problems.append(
                f"  ❌ {name} 有 cases.tsv 却没有调用 `{needle}`——"
                "共享用例表只能由 code/support/shared_cases.* 读，自己解析会与另一侧静默分家（T-047）"
            )
    return problems


def mutate_shared_table(unit_dir: Path, binary, shared_total):
    """把 `cases.tsv` 改坏，两侧都必须变红——否则它们只是**数了行数**。

    缘由（2026-08-18 复核）：闸门原本只核对「两侧都报了 N，且 N 等于表长」。
    条数是最容易凑对的量——`ch07/graph` 的 Python 侧当时遍历了整张表、
    条数报得完全正确，而异常那一行的输入是写死的，改表毫无反应。
    两边都在数行，没在跑同一件事。

    **这条自检不用重新编译**：`cases.tsv` 是运行期按工作目录读的，
    所以把已经建好的二进制换个工作目录跑一遍就够了，代价是两次进程启动。
    改动落在临时目录里，仓库里的表一个字节都不碰。
    """
    if shared_total is None:
        return []
    lines = (unit_dir / "cases.tsv").read_text(encoding="utf-8").splitlines()
    target = None
    for index, line in enumerate(lines[1:], 1):
        fields = line.split("\t")
        if len(fields) == 5 and fields[3].strip():
            target = index
            break
    if target is None:
        return ["  ❌ cases.tsv 里没有一条带 expected 的行——"
                "「把表改坏，两侧都要红」这条自检就无从下手（T-047）"]
    fields = lines[target].split("\t")
    # 往期望值尾巴上加一个字符：对任何真的在比对 expected 的实现，这一定不成立。
    fields[3] = fields[3] + ("9" if fields[3][-1].isdigit() else "X")
    lines[target] = "\t".join(fields)
    corrupted = "\n".join(lines) + "\n"

    problems = []
    with tempfile.TemporaryDirectory(prefix="dsa-shared-") as tmp:
        root = Path(tmp)
        # 镜像成 <临时根>/<章>/<单元>/，因为 test.py 靠 parents[2] 找 support/
        mirror = root / unit_dir.parent.name / unit_dir.name
        mirror.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(unit_dir, mirror)
        if (CODE / "support").is_dir():
            shutil.copytree(CODE / "support", root / "support")
        (mirror / "cases.tsv").write_text(corrupted, encoding="utf-8")
        name = lines[target].split("\t")[0]
        if binary is not None:
            run = subprocess.run([str(binary)], capture_output=True, text=True,
                                 cwd=mirror, timeout=TIMEOUT_SEC)
            if run.returncode == 0:
                problems.append(
                    f"  ❌ 把 cases.tsv 的 `{name}` 行期望值改坏后 **C++ 侧仍然通过**——"
                    "它报了条数，但没有真的在比对表里的期望值（T-047）")
        if (unit_dir / "modern.py").is_file() and (unit_dir / "test.py").is_file():
            run = subprocess.run([python_exe(), "test.py"], capture_output=True, text=True,
                                 cwd=mirror, timeout=TIMEOUT_SEC)
            if run.returncode == 0:
                problems.append(
                    f"  ❌ 把 cases.tsv 的 `{name}` 行期望值改坏后 **Python 侧仍然通过**——"
                    "同上（T-047）")
    return problems


def check_shared_report(output, expected, language):
    if expected is None:
        return None
    hit = SHARED_CASE_RE.search(output)
    if not hit:
        return f"  ❌ [{language}] 没有报告 `共享用例: N`（T-047）"
    actual = int(hit.group(1))
    if actual != expected:
        return f"  ❌ [{language}] 共享用例报告 {actual}，但 cases.tsv 有 {expected} 条"
    return None

# D-001 §2 在 C++ 侧禁的是「用 STL 容器替代本章要讲的手写实现」。
# Python 的标准库把同一件事做得更彻底——`sorted()` 一个调用就是第 8 章全章，
# `heapq` 一行就是 5.5 节。判据不变，只是换了一份名单。
# 和 `d001_exceptions` 一样，`unit.json.d025_exceptions` 可以豁免，但**必须写理由**。
#
# **名单按「删掉哪一节课」逐条列，不按模块整个封**（D-025 §2b，2026-08-18 人拍板）。
# 最初这里写的是整个 `collections`，那条太粗：`defaultdict` 与 `namedtuple`
# 不删任何一节课，是正当管道；真正会删课的只有 `deque` 与 `Counter`。
# 封整个模块的代价是第 7 章图算法的 Python 版一上来就得为 `defaultdict` 写豁免，
# 而豁免写多了，名单就从判据退化成手续。
D025_FORBIDDEN_IMPORTS = {
    "heapq": "D-025 堆与优先队列是 5.5 的课",
    "bisect": "D-025 二分检索是 10.2 的课",
    "re": "D-025 模式匹配是 4.3 的课（KMP）",
}
# 这些名字**出现即算**，不限于调用位置：`from collections import deque`、
# `collections.deque`、`d = deque` 都要拦下，否则换个写法就绕过去了。
D025_FORBIDDEN_NAMES = {
    "deque": "D-025 队列、循环队列与双端队列是 3.2 的课",
    "Counter": "D-025 计数是 8.6.1 桶式排序与 5.6 Huffman 频次统计的课",
}
# 与 D025_FORBIDDEN_NAMES 的差别是**位置敏感**：这些词只在被调用时算违规。
D025_FORBIDDEN_CALLS = {
    "sorted": "D-025 排序是第 8 章的课",
    "sort": "D-025 排序是第 8 章的课（`list.sort()`）",
    "print": "D-001§3 数据结构与算法实现内部严禁 I/O",
    "input": "D-001§3 数据结构与算法实现内部严禁 I/O",
}
# 再窄一档：这些只在**方法调用**位置算。`sort` 是内建方法名，不是内建函数——
# `values.sort()` 违规，而把一个排序函数当参数传进来再调用（`sort(values)`，
# test.py 里就这么写的）完全正当。不分这一档，闸门会把正当写法判红。
D025_METHOD_ONLY = {"sort"}

# **没有被封的，也记下来，免得下次有人「顺手补全」**：
#   OrderedDict —— 本书没有「保持插入序的散列表」这一节可删；而且 Python 3.7 起
#                   普通 dict 本身就保序，封 OrderedDict 却放行 dict 是做样子。
#   defaultdict / namedtuple —— 不对应任何一节课，是正当管道。
#
# 反过来，`dict` 与 `set` 确实会删掉第 10 章的课（算法10.9–10.12 闭散列表、
# 代码10.4 与算法10.5–10.7 集合），但它们是内建名、在别处又是不可替代的管道，
# 全局封不现实。所以给单元一个自己加码的口子：`unit.json.d025_forbidden`
# 列出「**本单元**因为讲这一节而额外禁用的名字」。第 10 章做 Python 版时用它。
D025_UNIT_BAN_KEY = "d025_forbidden"


# ── D-026：Python 代码风格，机器守住的三条 ──────────────────────────────────
#
# D-001 §4 给 C++ 定了命名与形态；D-025 引入 Python 时只定了证据链，**没定风格**。
# 后果当天就出现了：ch10–ch12 的实现写成 `self.kind=kind; self.rpp=records_per_page`，
# 而这些字节是**要印进书里**的（R3 逐字契约）。教材印出来的代码比课件还难读，
# 是本项目最不该出的错。
#
# 只守机器能判准的三条，不做半吊子的 PEP8：
#   1. 一行一句——`;` 不作语句分隔符
#   2. 复合语句的体另起一行——不写 `if x: return y`
#   3. 赋值号两侧各一个空格——`a = 1` 而不是 `a=1`
# 类型注解没有进这三条：它值得要求，但「哪些算公开接口」没有机器判据，
# 写成规则就会变成一条谁都能绕的软规定。宁可不写，也不写守不住的。
PYSTYLE_COMPOUND = (ast.If, ast.For, ast.While, ast.With, ast.Try,
                    ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
PYSTYLE_ASSIGN_OP = re.compile(r"^ (?:[-+*/%@&|^]|//|\*\*|>>|<<)?= $")


def check_pystyle(path: Path):
    """返回该文件的风格问题列表。判据见 D-026。"""
    source = path.read_text(encoding="utf-8")
    lines = source.splitlines()
    problems = []
    name = path.name

    # 1. 分号。走 tokenize 而不是字符串查找——注释和字面量里的 `;` 不算。
    try:
        for token in tokenize.generate_tokens(io.StringIO(source).readline):
            if token.type == tokenize.OP and token.string == ";":
                problems.append(f"  ❌ {name}:{token.start[0]} D-026 一行一句：`;` 不作语句分隔符")
    except (tokenize.TokenError, IndentationError) as exc:
        return [f"  ❌ {name} 无法分词：{exc}"]

    try:
        tree = ast.parse(source, filename=str(path))
    except SyntaxError as exc:
        return problems + [f"  ❌ {name}:{exc.lineno} 语法错误：{exc.msg}"]

    for node in ast.walk(tree):
        # 2. 复合语句的体不能贴在头一行
        if isinstance(node, PYSTYLE_COMPOUND) and node.body:
            if getattr(node.body[0], "lineno", None) == node.lineno:
                problems.append(
                    f"  ❌ {name}:{node.lineno} D-026 复合语句的体要另起一行"
                    f"（`{lines[node.lineno - 1].strip()[:56]}`）"
                )
        # 3. 赋值号两侧各一个空格
        if isinstance(node, (ast.Assign, ast.AugAssign, ast.AnnAssign)):
            value = getattr(node, "value", None)
            # 左边界要取「`=` 之前的最后一个节点」。带注解的赋值里那是**注解**
            # 而不是目标——取错就会把 `result: list[int] = []` 这种完全合规的写法判红
            # （2026-08-18 第一版就这么错了，12 处假阳性全在这里）。
            if isinstance(node, ast.AnnAssign):
                left = node.annotation
            elif isinstance(node, ast.Assign):
                left = node.targets[-1]
            else:
                left = node.target
            if value is None or value.lineno != left.end_lineno:
                continue  # 跨行赋值不判，判据不可靠
            segment = lines[left.end_lineno - 1][left.end_col_offset:value.col_offset]
            if not PYSTYLE_ASSIGN_OP.match(segment):
                problems.append(
                    f"  ❌ {name}:{node.lineno} D-026 赋值号两侧各留一个空格"
                    f"（`{lines[node.lineno - 1].strip()[:56]}`）"
                )
    return problems


def python_exe():
    return sys.executable or shutil.which("python3")


def check_d025(unit_dir: Path, meta):
    """modern.py 里有没有「一行把这一章删掉」的调用。用 AST 而不是正则——
    注释里出现 `sorted` 不该报红，`x.sort()` 该报红，正则两头都做不好。"""
    path = unit_dir / "modern.py"
    if not path.is_file():
        return []
    exceptions = {
        token.strip(): reason.strip()
        for token, reason in (meta.get("d025_exceptions") or {}).items()
        if isinstance(reason, str) and reason.strip()
    }
    source = path.read_text(encoding="utf-8")
    try:
        tree = ast.parse(source, filename=str(path))
    except SyntaxError as exc:
        return [f"  ❌ modern.py 语法错误：第 {exc.lineno} 行 {exc.msg}"]

    # 本单元自己加码的禁用名（讲这一节，就不能用现成的那个东西）。
    unit_bans = {}
    for token, reason in (meta.get(D025_UNIT_BAN_KEY) or {}).items():
        if not (isinstance(reason, str) and reason.strip()):
            return [f"  ❌ unit.json 的 {D025_UNIT_BAN_KEY}['{token}'] 没写理由——"
                    "自己加码也要说明这一节讲的是什么"]
        unit_bans[token.strip()] = f"D-025 本单元自禁：{reason.strip()}"
    names = {**D025_FORBIDDEN_NAMES, **unit_bans}

    problems = []
    seen = set()

    def flag(lineno, token, why):
        if token in exceptions or (lineno, token) in seen:
            return
        seen.add((lineno, token))
        problems.append(f"  ❌ modern.py:{lineno} {why}: `{token}`")

    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                root = alias.name.split(".")[0]
                if root in D025_FORBIDDEN_IMPORTS:
                    flag(node.lineno, root, D025_FORBIDDEN_IMPORTS[root])
        elif isinstance(node, ast.ImportFrom):
            root = (node.module or "").split(".")[0]
            if root in D025_FORBIDDEN_IMPORTS:
                flag(node.lineno, root, D025_FORBIDDEN_IMPORTS[root])
            for alias in node.names:
                if alias.name in names:
                    flag(node.lineno, alias.name, names[alias.name])
        elif isinstance(node, ast.Call):
            func = node.func
            if isinstance(func, ast.Name):
                name, method = func.id, False
            elif isinstance(func, ast.Attribute):
                name, method = func.attr, True
            else:
                name, method = None, False
            if name in D025_FORBIDDEN_CALLS and (method or name not in D025_METHOD_ONLY):
                flag(node.lineno, name, D025_FORBIDDEN_CALLS[name])
        # 位置无关的那一档：`deque(...)`、`collections.deque`、`alias = deque`
        # 三种写法都要拦，所以直接看 Name / Attribute 节点本身。
        if isinstance(node, ast.Name) and node.id in names:
            flag(node.lineno, node.id, names[node.id])
        elif isinstance(node, ast.Attribute) and node.attr in names:
            flag(node.lineno, node.attr, names[node.attr])
    return problems


def check_python_bindings(unit_dir: Path, meta):
    """有 modern.py 的单元，它认领的每一条清单都得给出 Python 锚点，或写明为什么不给。

    这条防的是「悄悄只覆盖一半」：Python 版实现了 17 条里的 5 条，
    书上却按「本章双实现」印，读者无从知道哪几条其实没有。
    """
    if not (unit_dir / "modern.py").is_file():
        return []
    problems = []
    source = (unit_dir / "modern.py").read_text(encoding="utf-8")
    source_lines = source.splitlines()
    tests = (unit_dir / "test.py").read_text(encoding="utf-8") if (unit_dir / "test.py").is_file() else ""
    anchors, tests_seen = set(), set()
    for entry in meta.get("listings", []):
        if not isinstance(entry, dict):
            continue
        listing = entry.get("id", "?")
        skip = entry.get("py_skip")
        anchor, test = entry.get("py_code_line"), entry.get("py_test")
        if isinstance(skip, str) and skip.strip():
            if anchor or test:
                problems.append(f"  ❌ {listing} 同时写了 py_skip 和 py_code_line/py_test，二选一")
            continue
        if skip is not None:
            problems.append(f"  ❌ {listing} 的 py_skip 是空的——不给 Python 可以，不写理由不行")
            continue
        if not (isinstance(anchor, str) and anchor.strip() and isinstance(test, str) and test.strip()):
            problems.append(
                f"  ❌ {listing} 缺 py_code_line/py_test：本单元有 modern.py，"
                "每条清单要么给出 modern.py 里的**一行真代码**（例如 `def kmp_search(`），"
                "要么写 py_skip 说明理由（D-025）。"
                "注意它**不是** `# >>> 名字` 那种切片标记——那是给书稿围栏 `#锚点` 用的，"
                "两者同名为「锚点」曾经绊倒过人，字段改名 py_code_line 就是为此（D-026）"
            )
            continue
        if anchor in anchors:
            problems.append(f"  ❌ {listing} 的 py_code_line 与同单元其他清单重复：{anchor}")
        if test in tests_seen:
            problems.append(f"  ❌ {listing} 的 py_test 与同单元其他清单重复：{test}")
        anchors.add(anchor)
        tests_seen.add(test)
        matching = [line for line in source_lines if anchor in line]
        if not matching:
            problems.append(f"  ❌ {listing} 的 py_code_line 在 modern.py 里不存在：{anchor}")
        elif all(line.lstrip().startswith("#") for line in matching):
            problems.append(
                f"  ❌ {listing} 的 py_code_line 只存在于注释行：{anchor}"
                "——填 `# >>> 名字` 这种切片标记是最常见的一种填法错误，"
                "这里要的是实现里的一行真代码"
            )
        if test not in tests:
            problems.append(f"  ❌ {listing} 的 py_test 在 test.py 里不存在：{test}")
    return problems


def check_reported_count_is_computed(unit_dir: Path):
    """`N 项断言` 里的 N 必须是**数出来的**，不能是写死的字面量。

    2026-08-18 实测：`code/ch12/trie/test.py` 印的是 `print("12 项断言")`——
    一个常量。密度闸门读的正是这个数，于是删掉一半断言它也照报 12。
    这不是风格问题，是**闸门被自己读的那行字架空**。
    """
    path = unit_dir / "test.py"
    if not path.is_file():
        return []
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    except SyntaxError:
        return []  # 语法错误由 run_python 报，这里不重复
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                and node.func.id == "print" and node.args):
            continue
        first = node.args[0]
        if isinstance(first, ast.Constant) and isinstance(first.value, str) \
                and "项断言" in first.value:
            return [f"  ❌ test.py:{node.lineno} 断言数是写死的字面量"
                    f"（`{first.value.strip()[:40]}`）——闸门读的就是这个数，"
                    "它必须由一个真的计数器算出来"]
    return []


def run_python(unit_dir: Path, profiles=None, meta=None, shared_total=None):
    """跑 test.py 两档。返回 (ok, 日志, 断言数)。"""
    # 单元级 py_skip：整个单元有意不给 Python（D-025 §1）。清单级的 py_skip 挂在
    # listings[] 上，但「原书无对应清单」的单元没有 listings 可挂，
    # 决定就没有落脚处——那正是 ch11/bplus_tree 需要的形状。
    unit_skip = (meta or {}).get("py_skip")
    if isinstance(unit_skip, str) and unit_skip.strip():
        if (unit_dir / "modern.py").is_file() or (unit_dir / "test.py").is_file():
            return False, ["  ❌ unit.json 写了 py_skip（本单元不给 Python），"
                           "却仍有 modern.py / test.py"], None
        return True, ["  · 本单元按 D-025 §1 不提供 Python 实现（unit.json 的 py_skip 写明了理由）"], None
    if unit_skip is not None:
        return False, ["  ❌ unit.json 的 py_skip 是空的——不给 Python 可以，不写理由不行"], None
    if not (unit_dir / "modern.py").is_file() and not (unit_dir / "test.py").is_file():
        return True, [], None
    logs, ok, assertions = [], True, None
    if not (unit_dir / "test.py").is_file():
        return False, ["  ❌ 有 modern.py 却没有 test.py：Python 实现不能是唯一没人验的代码（D-025）"], None
    if not (unit_dir / "modern.py").is_file():
        return False, ["  ❌ 有 test.py 却没有 modern.py。"], None
    for name, flags in (PY_PROFILES if profiles is None else profiles):
        cmd = [python_exe(), *flags, "test.py"]
        proc = subprocess.run(
            cmd, capture_output=True, text=True, cwd=unit_dir, timeout=TIMEOUT_SEC
        )
        if proc.returncode != 0:
            ok = False
            logs.append(
                f"  ❌ [{name}] Python 测试失败（退出码 {proc.returncode}）"
                f"\n{indent(proc.stdout + proc.stderr)}"
            )
            # 这一档已经判红，别再往下走去打那行 ✅——2026-08-18 实测，
            # 只改 Python 阈值时同一档同时印出「❌ 退出码 1」和「✅ …1 失败」。
            # 交接包里贴的就是这几行日志，自相矛盾的日志比没有日志更坏。
            continue
        shared_problem = check_shared_report(proc.stdout, shared_total, name)
        if shared_problem:
            ok = False
            logs.append(shared_problem)
            continue
        hit = re.search(r"(\d+)\s*项断言", proc.stdout)
        if hit:
            assertions = int(hit.group(1))
        tail = proc.stdout.strip().splitlines()[-1:] or ["(无输出)"]
        logs.append(f"  ✅ [{name}] {tail[0][:80]}")
    return ok, logs, assertions


def strip_comments_and_strings(source: str) -> str:
    """用空白替换注释与字符串，保留换行与预处理指令的行号。"""
    out, i, state = [], 0, "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "code" and ch == "/" and nxt == "/":
            state = "line-comment"
            out.extend("  ")
            i += 2
        elif state == "code" and ch == "/" and nxt == "*":
            state = "block-comment"
            out.extend("  ")
            i += 2
        elif state == "code" and ch == '"':
            state = "string"
            out.append(" ")
            i += 1
        elif state == "code" and ch == "'":
            state = "char"
            out.append(" ")
            i += 1
        elif state == "line-comment":
            out.append("\n" if ch == "\n" else " ")
            state = "code" if ch == "\n" else state
            i += 1
        elif state == "block-comment" and ch == "*" and nxt == "/":
            state = "code"
            out.extend("  ")
            i += 2
        elif state == "block-comment":
            out.append("\n" if ch == "\n" else " ")
            i += 1
        elif state in {"string", "char"} and ch == "\\":
            out.extend("  " if nxt else " ")
            i += 2 if nxt else 1
        elif state in {"string", "char"} and ((state == "string" and ch == '"') or (state == "char" and ch == "'")):
            state = "code"
            out.append(" ")
            i += 1
        elif state in {"string", "char"}:
            out.append("\n" if ch == "\n" else " ")
            i += 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


# 一个单元可以在结构上完全合规——unit.json 在、legacy.md 在、test.cpp 在、
# 认领的编号也都真实存在——而里面几乎什么都没有。台账会照样报「已现代化」。
# 这个空档是 2026-08-12 第一轮就写进 NOTES 的预言，同年同日被兑现：
# 一次提交用 542 行覆盖 61 条清单，第 8 章 17 条排序清单只有 11 项断言，
# 且 quick() = std::sort、heap() = std::make_heap、radix() 直接调 counting()。
#
# 「实现得对不对」没法机械判定，但「有没有东西」可以。下面两条守的是后者。
MIN_ASSERTIONS_PER_LISTING = 3
MIN_ASSERTIONS_PER_UNIT = 5
# 降级档的黄灯：Release 通过而 sanitizer 未运行时，太薄的测试必须显眼。
MIN_DEGRADED_ASSERTIONS = 10
MIN_LEGACY_LINES = 20
EVIDENCE_MARKERS = ("error:", "runtime error", "Sanitizer", "$ g++", "$ ./")
# 教学版是书稿正文整块印出来的那一份，读者最可能照抄。它比工程版少了移动语义与
# 强异常保证（D-012 的有意取舍），但「少考虑几件事」不等于「少验几条」——
# 教学版自己承诺的东西（LIFO、翻倍、深拷贝、空状态、零 I/O）必须逐条有断言守着。
MIN_TEACHING_ASSERTIONS = 10


def check_substance(unit_dir: Path, meta, assertions, teaching_assertions=None, py_assertions=None):
    """单元里到底有没有东西。返回 problems 列表。

    判据刻意保守：现有的扎实单元是每条清单 8–18 项断言，
    这里的下限只要求 3，够宽了。真有正当理由低于它，
    那是该写进 DECISION_LOG 的决定，不是又开一个逃生口。
    """
    problems = []
    listings = len(meta.get("listings", []))
    if assertions is not None and listings:
        need = max(MIN_ASSERTIONS_PER_LISTING * listings, MIN_ASSERTIONS_PER_UNIT)
        if assertions < need:
            problems.append(
                f"  ❌ 断言密度不足：{listings} 条清单只有 {assertions} 项断言"
                f"（下限 {need}）。清单被认领却几乎没有被验证。"
            )

    if (unit_dir / "teaching.hpp").is_file() and teaching_assertions is not None:
        if teaching_assertions < MIN_TEACHING_ASSERTIONS:
            problems.append(
                f"  ❌ 教学版只有 {teaching_assertions} 项断言（下限 {MIN_TEACHING_ASSERTIONS}）。"
                "正文整块印出来的那一份，不能比工程版验得松。"
            )

    # Python 侧的密度按**它自己认领了几条清单**算，而不是按单元的全部清单：
    # py_skip 掉的那些本来就不该要求 Python 断言（D-025）。
    if (unit_dir / "modern.py").is_file() and py_assertions is not None:
        claimed = [
            e for e in meta.get("listings", [])
            if isinstance(e, dict) and not (e.get("py_skip") or "").strip()
        ]
        # 「原书无对应清单」的单元（ch11/ch12 那几个）claimed 为空，此前**完全没有下限**——
        # 一个有 modern.py 的单元可以只写 4 条断言而闸门不响。至少按单元下限兜住。
        need = max(MIN_ASSERTIONS_PER_LISTING * len(claimed), MIN_ASSERTIONS_PER_UNIT)
        if py_assertions < need:
            where = f"{len(claimed)} 条清单认领了 Python 实现" if claimed else "本单元有 Python 实现"
            problems.append(
                f"  ❌ Python 断言密度不足：{where}，"
                f"只有 {py_assertions} 项断言（下限 {need}）。"
            )

    legacy = unit_dir / "legacy.md"
    if legacy.is_file():
        text = legacy.read_text(encoding="utf-8")
        lines = len([ln for ln in text.splitlines() if ln.strip()])
        if lines < MIN_LEGACY_LINES:
            problems.append(
                f"  ❌ legacy.md 只有 {lines} 行实质内容（下限 {MIN_LEGACY_LINES}）。"
                "红线要求「每条缺陷都要有证据」，两行说明不构成证据。"
            )
        elif not any(marker in text for marker in EVIDENCE_MARKERS):
            problems.append(
                "  ❌ legacy.md 里没有任何可复现的证据"
                f"（找不到 {', '.join(EVIDENCE_MARKERS)} 之一）。"
                "「原书这样写不好」不是证据，编译器与 sanitizer 的输出才是。"
            )
    return problems


def check_listing_bindings(unit_dir: Path, meta):
    """每条清单必须绑定存在于实现与测试中的稳定文本锚点。"""
    problems = []
    listings = meta.get("listings", [])
    sources = "\n".join(
        (unit_dir / name).read_text(encoding="utf-8")
        for name in ("modern.hpp", "modern.cpp", "teaching.hpp", "teaching.cpp", "demo.cpp")
        if (unit_dir / name).is_file()
    )
    tests = (unit_dir / "test.cpp").read_text(encoding="utf-8") if (unit_dir / "test.cpp").is_file() else ""
    code_lines, tests_seen = set(), set()
    source_lines = sources.splitlines()
    for entry in listings:
        if not isinstance(entry, dict):
            problems.append(
                f"  ❌ 清单绑定格式错误：{entry!r}（必须为 {{id, code_line, test}} 对象）")
            continue
        listing, code_line, test = entry.get("id"), entry.get("code_line"), entry.get("test")
        if not all(isinstance(x, str) and x.strip() for x in (listing, code_line, test)):
            problems.append(
                f"  ❌ 清单绑定字段不完整：{entry!r}"
                "（要 id / code_line / test 三项；code_line 是实现里的**一行真代码**，"
                "例如 `void push(const T& value)`，**不是** `// >>> 名字` 那种切片标记）"
            )
            continue
        if code_line in code_lines:
            problems.append(f"  ❌ {listing} 的 code_line 与同单元其他清单重复：{code_line}")
        if test in tests_seen:
            problems.append(f"  ❌ {listing} 的测试名与同单元其他清单重复：{test}")
        code_lines.add(code_line)
        tests_seen.add(test)
        matching_lines = [line for line in source_lines if code_line in line]
        if not matching_lines:
            problems.append(f"  ❌ {listing} 的 code_line 在实现里不存在：{code_line}")
        elif all(line.lstrip().startswith(("//", "/*", "*")) for line in matching_lines):
            problems.append(
                f"  ❌ {listing} 的 code_line 只存在于注释行：{code_line}"
                "——填 `// >>> 名字` 这种切片标记是最常见的一种填法错误，"
                "这里要的是实现里的一行真代码"
            )
        if test not in tests:
            problems.append(f"  ❌ {listing} 的测试名在 test.cpp 里不存在：{test}")
    return problems


def check_d001(unit_dir: Path, meta):
    """返回违反 D-001 的问题列表。unit.json 的 d001_exceptions 可豁免，但必须写理由。"""
    exceptions = {
        re.sub(r"\s+", "", token): reason.strip()
        for token, reason in meta.get("d001_exceptions", {}).items()
        if isinstance(reason, str) and reason.strip()
    }
    problems = []
    # teaching.* 同样是「数据结构实现」，D-001 §2/§3 一视同仁：教学版可以少写
    # noexcept、少写 static_assert（D-012 的豁免只到这里为止），但绝不能在容器里
    # 打 cout，也不能改用 std::vector 把这一节讲的东西删掉。
    for name in ("modern.hpp", "modern.cpp", "teaching.hpp", "teaching.cpp"):
        path = unit_dir / name
        if not path.is_file():
            continue
        source = strip_comments_and_strings(path.read_text(encoding="utf-8"))
        for lineno, code in enumerate(source.splitlines(), 1):
            # C++ 允许 `std :: cout` 和 `#  include <vector>`；静态闸门不能因为
            # 空白不同就漏掉同一条违规。去掉空白只用于本轮 D-001 token 匹配。
            normalized = re.sub(r"\s+", "", code)
            for pattern, desc in D001_FORBIDDEN:
                m = pattern.search(normalized)
                if not m:
                    continue
                token = m.group(0).strip()
                reason = exceptions.get(token) or exceptions.get(m.group(1) if m.groups() else "")
                if reason:
                    continue  # 有豁免且写了理由
                problems.append(f"  ❌ {name}:{lineno} {desc}: `{token}`")
    return problems


SANITIZER_PROFILE = "debug+asan+ubsan"


def sanitizer_preflight(std="c++17", flags=None, workdir=None):
    """先拿一个空程序试 sanitizer 能不能用。返回 (ok, 诊断输出)。

    存在的理由：红队那一轮在 macOS 上撞到 `sanitizer_malloc_mac.inc:189
    (!asan_init_is_running)`——**连空探针都挂**。那种情况下闸门会把每个单元
    都判红，读日志的人只会以为是自己的代码坏了。工具应当自己说清楚
    「是环境不可用」，而不是让人一个个单元去排除。
    """
    flags = flags or dict(PROFILES)[SANITIZER_PROFILE]
    tmp = workdir or tempfile.mkdtemp(prefix="dsa-preflight-")
    Path(tmp).mkdir(parents=True, exist_ok=True)
    src, binary = Path(tmp) / "preflight.cpp", Path(tmp) / "preflight"
    src.write_text("int main() { return 0; }\n", encoding="utf-8")
    try:
        build = subprocess.run(
            [compiler(), f"-std={std}", *flags, str(src), "-o", str(binary)],
            capture_output=True, text=True, timeout=TIMEOUT_SEC,
        )
        if build.returncode != 0:
            return False, "编译空探针即失败：\n" + (build.stdout + build.stderr).strip()
        run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=TIMEOUT_SEC)
        if run.returncode != 0:
            return False, (
                f"空探针运行失败（退出码 {run.returncode}）：\n"
                + (run.stdout + run.stderr).strip()
            )
        return True, "ok"
    except (subprocess.TimeoutExpired, OSError) as exc:
        return False, f"空探针未能完成：{exc}"
    finally:
        if workdir is None:
            shutil.rmtree(tmp, ignore_errors=True)


def discover(paths):
    if paths:
        dirs = []
        for p in paths:
            d = Path(p) if Path(p).is_absolute() else ROOT / p
            if not (d / "unit.json").is_file():
                print(f"❌ {p} 不是一个单元（缺 unit.json）")
                sys.exit(2)
            dirs.append(d)
        return dirs
    return sorted(p.parent for p in CODE.rglob("unit.json")) if CODE.is_dir() else []


# 一个单元最多有三个可执行产物，每个都在两档 profile 下真编译真运行：
#
#   test       modern.hpp（工程版）的断言测试——一直都有。
#   teaching   teaching.hpp（教学版）的断言测试——D-012 引入。
#   demo       书稿「先跑一遍」印的那段 main——**在 D-012 之前从来没被编译过**，
#              R3 只保证它和文件逐字一致，不保证那个文件能编译。教学版正文
#              最依赖「抄下来就能跑」，这个洞必须堵上。
#
# 每个产物一个独立的可执行文件：三份源码各有自己的 main，不能链进同一个二进制。
def unit_programs(unit_dir: Path, test_sources):
    """返回 ([(kind, sources), ...], problems)。"""
    programs = [("test", test_sources)]
    problems = []

    teaching = unit_dir / "teaching.hpp"
    teaching_test = unit_dir / "teaching_test.cpp"
    if teaching.is_file() and not teaching_test.is_file():
        problems.append(
            "  ❌ 有 teaching.hpp 却没有 teaching_test.cpp：教学版是书稿正文印出来的那份，"
            "不能是唯一没人验的代码（D-012）。"
        )
    elif teaching_test.is_file() and not teaching.is_file():
        problems.append("  ❌ 有 teaching_test.cpp 却没有 teaching.hpp。")
    elif teaching.is_file():
        programs.append(("teaching", [str(teaching_test)]))

    demo = unit_dir / "demo.cpp"
    if demo.is_file():
        programs.append(("demo", [str(demo)]))
    return programs, problems


def build_and_run(unit_dir: Path, workdir: Path, keep=False, profiles=None, degraded=False):
    """返回 (ok, 输出片段列表)。"""
    rel = rel_label(unit_dir)
    meta = json.loads((unit_dir / "unit.json").read_text(encoding="utf-8"))
    shared_total, shared_problem = shared_case_total(unit_dir, meta)
    logs = [shared_problem] if shared_problem else []
    loader_problems = check_shared_loader(unit_dir)
    logs.extend(loader_problems)
    std = meta.get("standard", "c++17")  # D-001
    extra = meta.get("flags", [])
    sources = [str(unit_dir / "test.cpp")]
    # modern.cpp 需要一起编译；modern.hpp 由 test.cpp #include
    if (unit_dir / "modern.cpp").is_file():
        sources.append(str(unit_dir / "modern.cpp"))
    sources += [str(unit_dir / s) for s in meta.get("extra_sources", [])]

    profiles = PROFILES if profiles is None else profiles
    ok = shared_problem is None and not loader_problems
    d001 = check_d001(unit_dir, meta)
    if d001:
        ok = False
        logs.extend(d001)
    bindings = check_listing_bindings(unit_dir, meta)
    if bindings:
        ok = False
        logs.extend(bindings)
    # D-025 Python 臂：静态判据先跑，跑不跑得起来后面见分晓
    d025 = check_d025(unit_dir, meta)
    if d025:
        ok = False
        logs.extend(d025)
    py_bindings = check_python_bindings(unit_dir, meta)
    if py_bindings:
        ok = False
        logs.extend(py_bindings)
    counted = check_reported_count_is_computed(unit_dir)
    if counted:
        ok = False
        logs.extend(counted)
    style = []
    for name in ("modern.py", "test.py"):
        if (unit_dir / name).is_file():
            style.extend(check_pystyle(unit_dir / name))
    if style:
        ok = False
        logs.extend(style[:8])
        if len(style) > 8:
            logs.append(f"  ❌ …另有 {len(style) - 8} 处 D-026 风格问题（同类，不逐条列）")

    programs, structure = unit_programs(unit_dir, sources)
    if structure:
        ok = False
        logs.extend(structure)

    assertions = None
    teaching_assertions = None
    test_binary = None
    for name, flags in profiles:
        for kind, srcs in programs:
            suffix = "" if kind == "test" else f"-{kind}"
            binary = workdir / f"{unit_dir.name}{suffix}-{name}"
            label = name if kind == "test" else f"{name}/{kind}"
            cmd = [
                compiler(),
                f"-std={std}",
                *BASE_FLAGS,
                *flags,
                *extra,
                f"-I{unit_dir}",
                f"-I{CODE}",  # 共享的测试探针：#include "support/fault_injection.hpp"
                *srcs,
                "-o",
                str(binary),
            ]
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT_SEC)
            if kind == "test" and proc.returncode == 0:
                test_binary = binary
            if proc.returncode != 0:
                ok = False
                logs.append(f"  ❌ [{label}] 编译失败\n{indent(proc.stdout + proc.stderr)}")
                continue
            run = subprocess.run(
                [str(binary)], capture_output=True, text=True, cwd=unit_dir, timeout=TIMEOUT_SEC
            )
            if run.returncode != 0:
                ok = False
                what = "demo 运行失败" if kind == "demo" else "测试失败"
                logs.append(
                    f"  ❌ [{label}] {what}（退出码 {run.returncode}）"
                    f"\n{indent(run.stdout + run.stderr)}"
                )
                continue
            if kind == "test":
                report_problem = check_shared_report(run.stdout, shared_total, f"{label}/C++")
                if report_problem:
                    ok = False
                    logs.append(report_problem)
            tail = run.stdout.strip().splitlines()[-1:] or ["(无输出)"]
            hit = re.search(r"(\d+)\s*项断言", run.stdout)
            if hit and kind == "test":
                assertions = int(hit.group(1))
            elif hit and kind == "teaching":
                teaching_assertions = int(hit.group(1))
            logs.append(f"  ✅ [{label}] {tail[0][:80]}")
    if ok and shared_total is not None:
        drift = mutate_shared_table(unit_dir, test_binary, shared_total)
        if drift:
            ok = False
            logs.extend(drift)
        else:
            logs.append(f"  ✅ [改坏用例表] 两侧都随之变红（{shared_total} 条共享用例）")

    py_ok, py_logs, py_assertions = run_python(
        unit_dir, meta=meta, shared_total=shared_total
    )
    if not py_ok:
        ok = False
    logs.extend(py_logs)

    substance = check_substance(
        unit_dir, meta, assertions, teaching_assertions, py_assertions
    )
    if substance:
        ok = False
        logs.extend(substance)
    if degraded and assertions is not None and assertions < MIN_DEGRADED_ASSERTIONS:
        logs.append(
            f"  ⚠️ 降级档测试偏薄：只有 {assertions} 项断言（建议至少 {MIN_DEGRADED_ASSERTIONS}）；"
            "sanitizer 未运行，Release-O2 不能覆盖内存与 UB。"
        )
    if keep:
        logs.append(f"  产物保留在 {workdir}")
    return ok, [f"{rel}  «{meta.get('title', '')}»", *logs]


def indent(text, prefix="     ", head=12, tail=28):
    """失败输出掐头去尾。

    掐中间而不是只留开头：被测代码如果在循环里打日志，几百行噪声会把
    最后那句 `FAIL: ...` 顶出视野——变异自检时就踩过这个坑。
    """
    lines = text.strip().splitlines()
    if len(lines) > head + tail:
        lines = lines[:head] + [f"... 中间省略 {len(lines) - head - tail} 行 ..."] + lines[-tail:]
    body = "\n".join(prefix + line for line in lines)
    return body or prefix + "(无输出)"


def main():
    parser = argparse.ArgumentParser(description="编译并运行 code/ 下的现代化单元")
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--keep", action="store_true", help="保留编译产物")
    parser.add_argument(
        "--allow-degraded",
        action="store_true",
        help="仅当 sanitizer 环境自检失败时生效：跳过该档、只跑 Release，并把降级大声记在输出里",
    )
    opts = parser.parse_args()

    if not compiler():
        print("❌ 找不到 g++ 或 clang++")
        sys.exit(2)

    units = discover(opts.paths)
    if not units:
        print("⚠️  code/ 下还没有单元，跳过（脚手架已就位，等第一个清单现代化）")
        return

    # 先自检 sanitizer 环境。挂了就直说是环境挂了，别让人误以为是单元的代码坏了。
    profiles, degraded_note = PROFILES, None
    ok_env, env_out = sanitizer_preflight()
    if not ok_env:
        if not opts.allow_degraded:
            print("❌ sanitizer 环境自检失败——这不是某个单元的问题，是这台机器上跑不起来。")
            print(indent(env_out))
            print(
                "\n本档用的是：" + " ".join(dict(PROFILES)[SANITIZER_PROFILE])
                + "\n处理办法：换一台能跑 ASan 的机器，或确认无解后用 --allow-degraded"
                + "（只跑 Release，降级会写进输出与交接包，不会悄悄变绿）。"
            )
            sys.exit(2)  # 2 = 环境问题，区别于 1 = 代码问题
        profiles = [(n, f) for n, f in PROFILES if n != SANITIZER_PROFILE]
        degraded_note = (
            "⚠️  降级运行：sanitizer 档已跳过（环境自检失败），本次结果**不覆盖内存与 UB 检查**。\n"
            + indent(env_out, head=4, tail=6)
        )
        print(degraded_note + "\n")

    workdir = Path(ROOT / ".build") if opts.keep else Path(tempfile.mkdtemp(prefix="dsa-check-"))
    workdir.mkdir(parents=True, exist_ok=True)
    failed, blocks = [], []
    for unit in units:
        try:
            ok, log = build_and_run(
                unit, workdir, opts.keep, profiles, degraded=degraded_note is not None
            )
        except subprocess.TimeoutExpired:
            ok, log = False, [rel_label(unit), f"  ❌ 超过 {TIMEOUT_SEC}s 未结束"]
        blocks.append("\n".join(log))
        if not ok:
            failed.append(rel_label(unit))
    if not opts.keep:
        shutil.rmtree(workdir, ignore_errors=True)

    print("\n".join(blocks))
    with_python = [u for u in units if (u / "modern.py").is_file()]
    print(
        f"\n{'❌' if failed else '✅'} {len(units) - len(failed)}/{len(units)} 个单元通过"
        f"（每个 {len(profiles)} 种构建：{', '.join(n for n, _ in profiles)}）"
    )
    if with_python:
        print(
            f"   其中 {len(with_python)} 个单元另有 Python 实现，"
            f"各跑 {len(PY_PROFILES)} 档：{', '.join(n for n, _ in PY_PROFILES)}（D-025）"
        )
    if degraded_note:
        # 降级必须在结论旁边再喊一次：交接包里只贴尾部几行的人不能被瞒过去。
        print("⚠️  本次为降级运行，未跑 sanitizer 档——上面的绿不代表内存与 UB 干净。")
    if failed:
        print("失败: " + ", ".join(failed))
        sys.exit(1)


if __name__ == "__main__":
    main()
