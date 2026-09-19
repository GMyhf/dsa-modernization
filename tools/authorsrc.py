#!/usr/bin/env python3
"""authorsrc.py — 作者代码包：原书每条清单在作者当年的工程里是哪一段。

为什么要有这个工具：

`dsa_raw.md` 是扫描件的 OCR，扫描件是「原书印了什么」的凭据。但**印出来的**清单
和**作者写的**工程并不总是同一份：代码3.2 印的是与成员变量 `int top` 同名的
`bool top(T&)`，编译不过；作者代码包里那个函数叫 `getTop(T*)`，编译得过。
没有第二个证人，我们只能说「原书这里编译不过」，说不出这是作者的代码本来就错、
印出来的版本与作者的代码不一致，还是 OCR 造成的——三者在 `legacy.md` 里的写法完全不同。
（包与原书都是 2008 年 6 月，谁先谁后不可考，所以不说「排印时引入」。原书前言
`dsa_raw.md:195` 称它为「与本书配套的代码包」。）

作者代码包（`DSCode_ZhangWangZhao2008_06.zip`）是 2025 秋期末机考「考场可用资料」之一，
解包在 `site_visit/DSCode_ZWZ200806_CPP/`（来源与收录口径见 `site_visit/README.md`）。
本工具把「清单 → 代码包里的文件与符号」登记进 `collab/authorsrc.json`，于是：

    清单号 → (文件, 符号) → 那一段源码             # --listing 3.2
    源文件 → sha256                               # 包被改动时闸门变红
    程序   → 今天的 g++ 编译得过吗                 # 包在考场上能不能直接用

`--check`（闸门用）核对六件事：
  1. 原书 105 条清单**每一条**都有登记——要么指到代码包里的文件与符号，要么写明
     「包里没有」的理由（与 exclusions.json 同一个规矩：不许悄悄漏掉）；
  2. 登记的文件存在、每个符号都能在文件里找到**定义**（不是声明）；
  3. 代码包源文件与登记的 sha256 一致——包是证据，证据不许手改；
  4. 与 `ref_数据结构与算法A 2021秋/SourceCodes/`（同一套代码的 2021 版副本）逐文件
     比对，只允许登记过的差异；
  5. 结论（findings）：「这处缺陷作者包里有没有」每条都带正则，在包的源码上逐条成立；
     `collab/errata.json` 里每条编译/运行/内存类勘误都必须有一条结论；书稿里由登记表
     生成的几节（`book/考场代码包.md` 三节、`book/勘误.md` 一节）与登记表逐字一致；
  6. 每个带 `main` 的程序在今天的 g++（`-std=c++17 -fsyntax-only`）下编译得过或
     编译不过，与登记一致；`code/**/author_diff.cpp` 对拍程序逐个编译（ASan/UBSan）
     并运行，退出码 0。编译器缺失时这一项只提示不判红。

用法:
  python3 tools/authorsrc.py                    # 概况：登记了几条、几条包里没有、编译情况
  python3 tools/authorsrc.py --listing 3.2      # 打印代码3.2 在作者包里的那一段（UTF-8）
  python3 tools/authorsrc.py --listing 代码3.2  # 同上
  python3 tools/authorsrc.py --compile          # 重跑编译扫描并打印（不写登记表）
  python3 tools/authorsrc.py --diff             # 只跑 code/**/author_diff.cpp 对拍
  python3 tools/authorsrc.py --write-book       # 重写书稿里由登记表生成的节（附录「考场代码包」与勘误的一节）
  python3 tools/authorsrc.py --check            # 闸门用：五项核对，不一致退出码 1
  python3 tools/authorsrc.py --write-hashes     # 代码包更新后重记 sha256 与编译结果
  python3 tools/authorsrc.py --export DIR       # 把代码包转成 UTF-8 + LF 写到 DIR
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from repo import ROOT, rel_label  # noqa: E402  同目录工具
import ledger  # noqa: E402  清单总表的 parser of record

PACK = ROOT / "site_visit" / "DSCode_ZWZ200806_CPP"
TWIN = ROOT / "ref_数据结构与算法A 2021秋" / "SourceCodes"
MANIFEST = ROOT / "collab" / "authorsrc.json"
BUILD = ROOT / ".build" / "authorsrc"

# 闸门不许碰网络。Ubuntu 默认设了 DEBUGINFOD_URLS=https://debuginfod.ubuntu.com，
# clang 的 ASan 报错时，llvm-symbolizer 会**逐个库上网取调试信息**：一个 use-after-free
# 的报告从 0.4 秒拖到 7–11 秒，断网时更可能卡住（2026-09-19 查 T-080 自测变慢时量出）。
# 符号化只需要本地的 -g 信息，所以对本进程及其所有子进程清掉它。
os.environ.pop("DEBUGINFOD_URLS", None)


SOURCE_SUFFIXES = (".h", ".cpp")
# 2021 版副本把 `ch03_StackQueue` 叫 `chap3_StackQueue`、`ch08_Sort` 叫 `Chap8_Sort`
PACK_CHAPTER_RE = re.compile(r"^ch(\d+)_(.+)$")
TWIN_CHAPTER_RE = re.compile(r"^[Cc]hap(\d+)_(.+)$")
MAIN_RE = re.compile(r"\bmain\s*\(")


# ---------------------------------------------------------------- 读包


def decode(path: Path) -> str:
    """返回 UTF-8 文本、LF 换行。

    作者包大多是 GBK + CRLF（2008 年 VC6 的默认），但第 10、12 章有几个文件是
    带 BOM 的 UTF-8——当 GBK 读，BOM 会变成「锘」，编译器报 `does not name a type`，
    那是我们读错了，不是作者写错了。所以先认 BOM，再试严格 UTF-8，最后才是 GBK。
    """
    data = path.read_bytes()
    if data.startswith(b"\xef\xbb\xbf"):
        text = data[3:].decode("utf-8", errors="replace")
    else:
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            text = data.decode("gbk", errors="replace")
    return text.replace("\r\n", "\n").replace("\r", "\n")


def source_files(base: Path):
    """包里的可读源文件（相对路径 → Path），按路径排序。"""
    if not base.is_dir():
        return {}
    found = {}
    for path in sorted(base.rglob("*")):
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            found[path.relative_to(base).as_posix()] = path
    return found


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def twin_path(rel: str):
    """考场版里的相对路径 → 2021 版副本里的对应路径（章目录名不同，其余相同）。"""
    head, _, rest = rel.partition("/")
    found = PACK_CHAPTER_RE.match(head)
    if not found or not TWIN.is_dir():
        return None
    number = int(found.group(1))
    for candidate in TWIN.iterdir():
        other = TWIN_CHAPTER_RE.match(candidate.name)
        if other and int(other.group(1)) == number and other.group(2) == found.group(2):
            return candidate / rest
    return None


# ---------------------------------------------------------------- 找定义


def _mask(text: str) -> str:
    """把注释与字符串/字符字面量换成空格（长度不变），好让花括号匹配不被它们骗。"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        two = text[i : i + 2]
        if two == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        elif text[i] in "\"'":
            quote, j = text[i], i + 1
            while j < n and text[j] != quote and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            for k in range(i + 1, min(j, n)):
                out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


def _match(masked: str, start: int, open_ch: str, close_ch: str) -> int:
    """masked[start] 是 open_ch；返回配对的 close_ch 之后的位置，找不到返回 -1。"""
    depth = 0
    for i in range(start, len(masked)):
        if masked[i] == open_ch:
            depth += 1
        elif masked[i] == close_ch:
            depth -= 1
            if depth == 0:
                return i + 1
    return -1


def _line_start(text: str, pos: int) -> int:
    return text.rfind("\n", 0, pos) + 1


def _with_template_prefix(text: str, start: int) -> int:
    """定义前一行若是 `template <...>`，把它一起带上。"""
    if start == 0:
        return start
    prev_start = _line_start(text, start - 1)
    if text[prev_start:start].lstrip().startswith("template"):
        return prev_start
    return start


def find_definition(text: str, symbol: str, nth: int = 1):
    """在 text 里找 symbol 的**定义**，返回 (起始行号, 片段文本)；找不到返回 None。

    symbol 两种写法：
      `class X` / `struct X` —— 类定义（`class X :` 或 `class X {`，不认前置声明）
      `f`                     —— 函数定义：`f(...)` 之后紧跟 `const`/初始化列表再是 `{`
      `operator&` 这类运算符同样按函数处理。
    """
    masked = _mask(text)
    kind, _, name = symbol.partition(" ")
    if kind in ("class", "struct") and name:
        pattern = re.compile(r"\b" + kind + r"\s+" + re.escape(name) + r"\b[^;{]*\{")
        for found in pattern.finditer(masked):
            brace = found.end() - 1
            end = _match(masked, brace, "{", "}")
            if end < 0:
                continue
            if masked[end : end + 10].lstrip().startswith(";"):
                end = masked.index(";", end) + 1
            nth -= 1
            if nth:
                continue
            begin = _with_template_prefix(text, _line_start(text, found.start()))
            return text.count("\n", 0, begin) + 1, text[begin:end]
        return None
    pattern = re.compile(r"(?<![\w~])" + re.escape(symbol) + r"\s*\(")
    for found in pattern.finditer(masked):
        paren = found.end() - 1
        after = _match(masked, paren, "(", ")")
        if after < 0:
            continue
        tail = re.match(r"\s*(?:const\s*)?(?::[^;{]*)?\{", masked[after:])
        if not tail:
            continue  # 声明或调用
        brace = after + tail.end() - 1
        end = _match(masked, brace, "{", "}")
        if end < 0:
            continue
        nth -= 1
        if nth:
            continue
        begin = _with_template_prefix(text, _line_start(text, found.start()))
        return text.count("\n", 0, begin) + 1, text[begin:end]
    return None


# ---------------------------------------------------------------- 登记表


def load_manifest(path=MANIFEST):
    if not path.is_file():
        return {"_doc": [], "listings": {}, "hashes": {}, "programs": {}, "twin_deltas": {}}
    return json.loads(path.read_text(encoding="utf-8"))


def save_manifest(data, path=MANIFEST):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def normalize_id(raw: str, inventory_ids):
    """`3.2` → `代码3.2`；`算法3.3` 原样；对不上返回 None。"""
    raw = raw.strip()
    if raw in inventory_ids:
        return raw
    for prefix in ("算法", "代码"):
        if prefix + raw in inventory_ids:
            return prefix + raw
    return None


def uncomment_blocks(text: str) -> str:
    """去掉 `/*` `*/` 两个记号（换成等长空格）。作者常把同一函数的旧版本整段注释掉留在文件里
    （fact.cpp 里递归版与迭代版阶乘就是这样），登记 `commented_out` 的清单要在这些段落里找。"""
    return text.replace("/*", "  ").replace("*/", "  ")


def slices_for(entry):
    """一条已登记清单 → [(相对路径, 符号, 起始行号|None, 片段|None)]。

    符号可带 `@n` 后缀，取第 n 个定义（同一文件里有同名的几个版本时用）。
    """
    rel = entry["file"]
    path = PACK / rel
    text = decode(path) if path.is_file() else ""
    if entry.get("commented_out"):
        text = uncomment_blocks(text)
    out = []
    for symbol in entry.get("symbols", []):
        name, _, nth = symbol.partition("@")
        found = find_definition(text, name, int(nth) if nth else 1) if text else None
        out.append((rel, symbol, found[0] if found else None, found[1] if found else None))
    return out


# ---------------------------------------------------------------- 编译扫描


LOCAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)


def translation_unit_has_main(rel, files, seen=None):
    """rel 连同它 `#include "..."` 进来的包内文件里有没有 main。

    第 8 章每个排序 .cpp 自己不写 main，末尾 `#include "SortMain.h"`——只看 .cpp 本身会把
    12 个排序程序整个漏掉（2026-09-18 Codex 复核 T-079 时才发现，原先的「40 个程序」少算了它们）。
    """
    seen = set() if seen is None else seen
    if rel in seen or rel not in files:
        return False
    seen.add(rel)
    text = _mask_keep_includes(decode(files[rel]))
    if MAIN_RE.search(_mask(decode(files[rel]))):
        return True
    base = Path(rel).parent
    for target in LOCAL_INCLUDE_RE.findall(text):
        resolved = Path(os.path.normpath(base / target)).as_posix()
        if translation_unit_has_main(resolved, files, seen):
            return True
    return False


def _mask_keep_includes(text: str) -> str:
    """去掉注释但保留字符串（#include "x" 的文件名在字符串里）。"""
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def programs(files=None):
    """包里能单独编译成程序的 .cpp（自身或其包含的包内文件里有 main）。"""
    files = source_files(PACK) if files is None else files
    return [rel for rel in files if rel.endswith(".cpp") and translation_unit_has_main(rel, files)]


def export(dest: Path):
    """把代码包转成 UTF-8 + LF 写到 dest（保留目录结构），返回写出的文件数。"""
    if dest.exists():
        shutil.rmtree(dest)
    count = 0
    for rel, path in source_files(PACK).items():
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(decode(path), encoding="utf-8")
        count += 1
    return count


def find_compiler(spec=None):
    """返回编译器命令行（list）。spec 形如 "clang++ -stdlib=libc++"；缺省依次找 g++、clang++。"""
    if spec:
        argv = spec.split()
        return argv if shutil.which(argv[0]) else None
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return [name]
    return None


def toolchain_key(compiler) -> str:
    """编译器族 / 标准库，例如 gcc/libstdc++、clang/libc++、apple-clang/libc++。

    包的编译结果**随工具链而变**，不是一个布尔值：第 8 章的排序先调用、后定义（ModInsSort、
    Partition……），clang 照两阶段查找的标准报错、g++ 放行；BitSet/bag.cpp 的全局 count
    在 libc++ 下与 std::count 撞名。macOS 上的 `g++` 其实是 Apple clang + libc++。
    所以编译结果按这个键分开登记，谁的机器就对谁的基线。
    """
    version = subprocess.run([*compiler, "--version"], capture_output=True, text=True).stdout
    family = "apple-clang" if "Apple" in version else "clang" if "clang" in version else "gcc"
    probe = subprocess.run(
        [*compiler, "-std=c++17", "-x", "c++", "-E", "-dM", "-"],
        input="#include <cstddef>\n", capture_output=True, text=True,
    ).stdout
    library = "libc++" if "_LIBCPP_VERSION" in probe else "libstdc++"
    return f"{family}/{library}"


def first_error(stderr: str) -> str:
    for line in stderr.splitlines():
        if "error:" in line:
            return line.split("error:", 1)[1].strip()
    return stderr.strip().splitlines()[0] if stderr.strip() else ""


def compile_sweep(compiler):
    """{程序相对路径: {"compiles": bool, "error": 首条 error 文本}}；在 UTF-8 副本上编译。

    compiler 是命令行 list；结果只对这一个工具链成立（见 toolchain_key）。"""
    work = BUILD / "utf8"
    export(work)
    results = {}

    def one(rel):
        src = work / rel
        proc = subprocess.run(
            [*compiler, "-std=c++17", "-w", "-fsyntax-only", "-I", str(src.parent), str(src)],
            capture_output=True,
            text=True,
        )
        entry = {"compiles": proc.returncode == 0}
        if proc.returncode != 0:
            entry["error"] = first_error(proc.stderr.replace(str(work) + "/", ""))
        return rel, entry

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        for rel, entry in pool.map(one, programs()):
            results[rel] = entry
    return dict(sorted(results.items()))


# ---------------------------------------------------------------- 对拍（code/**/author_diff.cpp）

HARNESS_NAME = "author_diff.cpp"
CODE = ROOT / "code"
SANITIZE = ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]


def harness_files():
    return sorted(p.relative_to(ROOT).as_posix() for p in CODE.rglob(HARNESS_NAME)) if CODE.is_dir() else []


def sanitizers_work(compiler) -> bool:
    """本机 ASan/UBSan 能否真编真跑（UNVERIFIED-RISKS 记过某台机器上空探针失败）。"""
    probe_dir = BUILD / "probe"
    probe_dir.mkdir(parents=True, exist_ok=True)
    src, exe = probe_dir / "probe.cpp", probe_dir / "probe"
    src.write_text("int main() { return 0; }\n", encoding="utf-8")
    built = subprocess.run([*compiler, *SANITIZE, str(src), "-o", str(exe)], capture_output=True)
    return built.returncode == 0 and subprocess.run([str(exe)], capture_output=True).returncode == 0


def run_harness(compiler, rel, includes, sanitize=True):
    """编译并运行一个对拍程序。作者代码用 UTF-8 副本；返回 (ok, 最后一行输出或错误)。

    作者代码有意不查泄漏（findNext 的 new int[m]、setPos 的游离结点都从不释放——那正是
    要记录的缺陷，不是对拍要拦的东西），所以 detect_leaks=0；越界与未定义行为照拦。
    """
    work = BUILD / "utf8"
    exe = BUILD / "harness" / "_".join(compiler).replace("/", "_") / rel.replace("/", "__").replace(".cpp", "")
    exe.parent.mkdir(parents=True, exist_ok=True)
    # -fpermissive 只有 g++ 认；clang 会拒绝未知选项，所以按编译器族给
    permissive = ["-fpermissive"] if toolchain_key(compiler).startswith("gcc") else []
    flags = ["-std=c++17", "-w", *permissive, "-O0", "-g", *(SANITIZE if sanitize else [])]
    include_flags = [arg for inc in includes for arg in ("-I", str(work / inc))]
    built = subprocess.run(
        [*compiler, *flags, *include_flags, str(ROOT / rel), "-o", str(exe)], capture_output=True, text=True
    )
    if built.returncode != 0:
        return False, "编译失败：" + first_error(built.stderr)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="print_stacktrace=1")
    ran = subprocess.run([str(exe)], capture_output=True, text=True, env=env, timeout=300)
    lines = (ran.stdout + ran.stderr).strip().splitlines()
    if ran.returncode != 0:
        detail = next((line for line in lines if "ERROR:" in line or "runtime error" in line or "✗" in line), "")
        return False, f"退出码 {ran.returncode}：{detail or (lines[-1] if lines else '')}"
    return True, lines[-1] if lines else ""


def harness_registration_problems(data):
    """code/**/author_diff.cpp 与登记表两边对得上：不许有没登记的对拍、也不许登记了却没有文件。"""
    problems, registered, found = [], data.get("harnesses", {}), harness_files()
    for rel in found:
        if rel not in registered:
            problems.append(f"{rel}：对拍程序未在 harnesses 里登记 include 目录")
    for rel in registered:
        if rel not in found:
            problems.append(f"{rel}：登记了对拍程序，文件不存在")
    return problems


def check_harnesses(data, compiler, out=print):
    problems, registered = harness_registration_problems(data), data.get("harnesses", {})
    runnable = [rel for rel in harness_files() if rel in registered]
    if not runnable:
        return problems
    sanitize = sanitizers_work(compiler)
    if not sanitize:
        out("  ⚠ 本机 ASan/UBSan 探针失败，对拍程序不带 sanitizer 编译——越界读写不会被拦下")
    export(BUILD / "utf8")
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = {rel: pool.submit(run_harness, compiler, rel, registered[rel]["include"], sanitize) for rel in runnable}
    for rel, future in futures.items():
        ok, line = future.result()
        if ok:
            out(f"  {line}  ← {rel}")
        else:
            problems.append(f"{rel}：{line}")
    return problems


# ---------------------------------------------------------------- 结论（findings）

VERDICTS = {
    "same": "包里也有",          # 作者的代码本来就这样：不是排印或 OCR 造成的
    "absent": "包里没有",        # 印出来的与作者代码不一致；包里是另一种（通常是对的）写法
    "no_code": "包里无此代码",   # 包里根本没有对应的实现，无从比较
    "pack_only": "只在包里",     # 原书没印或印对了，缺陷只在代码包里——考场上直接用包的人会踩
}
ERRATA = ROOT / "collab" / "errata.json"
ERRATA_NEEDING_VERDICT = ("compile", "runtime", "memory")


def load_errata():
    if not ERRATA.is_file():
        return []
    return json.loads(ERRATA.read_text(encoding="utf-8")).get("errata", [])


def as_list(value):
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


def check_findings(data):
    """每条结论的正则都要在代码包源码上成立；每条编译/运行/内存类勘误都要有结论。"""
    problems, seen = [], set()
    findings = data.get("findings", [])
    errata = {e["id"]: e for e in load_errata()}
    covered = set()
    for item in findings:
        fid = item.get("id", "?")
        if fid in seen:
            problems.append(f"结论 {fid}：编号重复")
        seen.add(fid)
        if item.get("verdict") not in VERDICTS:
            problems.append(f"结论 {fid}：verdict 必须是 {'/'.join(VERDICTS)} 之一")
        if not str(item.get("summary", "")).strip():
            problems.append(f"结论 {fid}：缺 summary")
        for eid in as_list(item.get("errata")):
            if eid not in errata:
                problems.append(f"结论 {fid}：勘误 {eid} 在 collab/errata.json 里不存在")
            covered.add(eid)
        harness = item.get("harness")
        if harness and not (ROOT / harness).is_file():
            problems.append(f"结论 {fid}：对拍程序 {harness} 不存在")
        if not item.get("checks"):
            problems.append(f"结论 {fid}：至少要有一条针对代码包源码的 checks")
        for check_item in item.get("checks", []):
            path = PACK / check_item.get("file", "")
            if not path.is_file():
                problems.append(f"结论 {fid}：文件不存在 {check_item.get('file')}")
                continue
            text = decode(path)
            for pattern in check_item.get("match", []):
                if not re.search(pattern, text, re.M):
                    problems.append(f"结论 {fid}：{check_item['file']} 里找不到 /{pattern}/")
            for pattern in check_item.get("no_match", []):
                if re.search(pattern, text, re.M):
                    problems.append(f"结论 {fid}：{check_item['file']} 里不该出现 /{pattern}/")
    for eid, entry in errata.items():
        if entry.get("kind") in ERRATA_NEEDING_VERDICT and eid not in covered:
            problems.append(f"勘误 {eid}（{entry.get('kind')}）：没有「作者代码包里有没有」的结论")
    return problems


# ---------------------------------------------------------------- 书稿里由本工具生成的节
#
# 书稿里有几节**整节由登记表生成**：清单在包里的位置、编译结果、包里的坑、勘误在包里有没有。
# 手抄一份迟早和登记表分叉，所以这几节归本工具所有：从 `## 标题` 那一行之后、到下一个
# `## ` 之前的正文，由 --write-book 重写，--check 逐字比对。节外的文字照常手写。
# （不用 HTML 注释当标记：build_site 的渲染器是自己写的，不认注释。）

BOOK = ROOT / "book"
APPENDIX = BOOK / "考场代码包.md"
ERRATA_PAGE = BOOK / "勘误.md"

COMPILE_HINTS = [
    # g++ 与 clang 的措辞不同，正则两边都要认
    (r"main.{0,3} must return .int", "`void main()` 改成 `int main()`"),
    (r"\bcout\b", "补 `using namespace std;`，或写 `std::cout`"),
    (r"extra qualification", "类内声明去掉 `Graphm::` 前缀"),
    (r"comparison between pointer and integer", "`assert(str != '\\0')` 改为 `assert(str != NULL)`（勘误 E11）"),
    (r"reference to .less. is ambiguous", "自定义的 `less` 与 `std::less` 撞名：改名，或去掉 `using namespace std;`"),
    (r"reference to .count. is ambiguous", "全局变量 `count` 与 `std::count` 撞名（只在 libc++ 下）：改名，或去掉 `using namespace std;`"),
    (r"afxtempl\.h", "VC6 的 MFC 头文件，g++ 没有：换成标准库容器"),
    (r"default arguments|may not have default arguments", "默认参数只留在类内声明，类外定义处删掉"),
    (r"undeclared identifier .i.|.i. was not declared", "`LinkSort.h` 的 `PrintAddr` 里循环变量 `i` 没声明：写成 `for (int i=0; ...)`"),
]
TOOLCHAIN_LABEL = {
    "gcc/libstdc++": "g++",
    "clang/libstdc++": "clang",
    "clang/libc++": "clang + libc++",
    "apple-clang/libc++": "Apple clang",
}


def _cell(text: str) -> str:
    text = str(text).replace("\n", " ").strip()
    if "|" in text:
        # build_site 的表格按 | 直接切分，不认 \| 转义：生成的格子里一个 | 就会把表拆坏
        raise ValueError(f"表格单元里不能有 |：{text}")
    prose = re.sub(r"`[^`]*`", "", text)
    if re.search(r"[<>*\[\]_]", prose):
        # `Link<T>*` 不加反引号：<T> 会被 HTML 当标签吞掉，成对的 * 会变成斜体
        raise ValueError(f"代码片段要放进反引号：{text}")
    return text


def _code(text: str) -> str:
    """整格就是一段代码或路径：直接包进反引号。"""
    text = str(text).replace("\n", " ").strip()
    if "|" in text or "`" in text:
        raise ValueError(f"代码格里不能有 | 或反引号：{text}")
    return f"`{text}`"


def compile_hint(error: str) -> str:
    for pattern, hint in COMPILE_HINTS:
        if re.search(pattern, error):
            return hint
    return "—"


def section_listing_map(data):
    lines = [
        "",
        "原书 105 条清单在包里的去处。符号是函数或类名；同一文件里有几个同名版本时注明第几个。",
        "",
        "| 清单 | 包里的文件 | 符号 |",
        "| --- | --- | --- |",
    ]
    for item in ledger.parse_inventory():
        entry = data.get("listings", {}).get(item["id"])
        if entry is None:  # 未登记：--check 的第 1 项会报，这里别先崩
            lines.append(f"| {item['id']} | 未登记 | — |")
            continue
        if "absent" in entry:
            lines.append(f"| {item['id']} | 包里没有 | {_cell(entry['absent'])} |")
            continue
        symbols = []
        for symbol in entry["symbols"]:
            name, _, nth = symbol.partition("@")
            symbols.append(f"`{name}`" + (f"（第 {nth} 个）" if nth else ""))
        note = "；" + _cell(entry["note"]) if entry.get("note") else ""
        lines.append(f"| {item['id']} | {_code(entry['file'])} | {'、'.join(symbols)}{note} |")
    return lines


def section_compile(data):
    programs_ = data.get("programs", {})
    keys = recorded_toolchains(data)
    primary = "gcc/libstdc++" if "gcc/libstdc++" in keys else (keys[0] if keys else "")
    counts = "；".join(
        f"{TOOLCHAIN_LABEL.get(k, k)}（{k}）{sum(1 for e in programs_.values() if e.get(k, {}).get('compiles'))} 个编译得过"
        for k in keys
    )
    bad = [(rel, e) for rel, e in programs_.items() if any(not r.get("compiles") for r in e.values())]
    ok = [rel for rel, e in programs_.items() if all(r.get("compiles") for r in e.values())]
    lines = [
        "",
        f"包里能单独编译成程序的 `.cpp` 共 {len(programs_)} 个（自身或它包含的头文件里有 `main`；"
        "第 8 章的排序都把 `main` 放在 `SortMain.h` 里）。先把源码转成 UTF-8，再按 `-std=c++17` 只做语法检查："
        f"{counts}。**编译结果随工具链而变**：OpenJudge 的 C++ 用 g++；macOS 上的 `g++` 其实是 Apple clang + libc++，"
        "最接近「clang + libc++」那一列。",
        "",
        "编译不过的几乎全是 2008 年 VC6 能容忍、今天的编译器不再接受的写法，不是算法错。下表列出在任一工具链下编译不过的程序"
        "（✓ 编译得过，✗ 编译不过），报错取第一个失败工具链的第一条。",
        "",
        "| 程序 | " + " | ".join(TOOLCHAIN_LABEL.get(k, k) for k in keys) + " | 第一条报错 | 改法 |",
        "| --- | " + " | ".join("---" for _ in keys) + " | --- | --- |",
    ]
    for rel, entry in bad:
        marks = " | ".join("✓" if entry.get(k, {}).get("compiles") else "✗" for k in keys)
        failing = [k for k in ([primary] + keys) if k in entry and not entry[k].get("compiles")]
        error = entry[failing[0]].get("error", "")
        lines.append(f"| {_code(rel)} | {marks} | {_code(error)} | {compile_hint(error)} |")
    lines += [
        "",
        "还有一件事语法检查看不出来：第 8 章这些排序程序在 clang 下「编译得过」，只因为计时驱动里的排序调用被注释掉了"
        "（见「包里的坑」P06），模板从没被实例化。真去调用 `ShellSort`、`QuickSort`、`MergeSort` 时，"
        "clang 会按标准报「函数在模板定义处不可见」——作者都是先调用、后定义（`ModInsSort`、`Partition`、`Merge`……），"
        "只有 g++ 放行。在 clang 下用它们，把被调函数的定义挪到前面，或先写一行声明。",
        "",
        "所有工具链下都编译得过的：" + "、".join(_code(rel) for rel in ok) + "。",
    ]
    return lines


def section_traps(data):
    lines = [
        "",
        "下面几处缺陷**原书没有**——要么原书没印那段代码，要么印对了——只在包里。"
        "考场上直接拿包里的代码交题，会在这些地方读写到数组外面或得到错的结果。",
        "",
        "| 编号 | 位置 | 缺陷 |",
        "| --- | --- | --- |",
    ]
    for item in data.get("findings", []):
        if item.get("verdict") == "pack_only":
            files = "、".join(_code(c["file"]) for c in item["checks"][:1])
            lines.append(f"| {item['id']} | {_cell(item['listing'])}，{files} | {_cell(item['summary'])} |")
    same_traps = [i for i in data.get("findings", []) if i.get("verdict") == "same" and not i.get("errata")]
    if same_traps:
        lines += ["", "另有与原书同病、但不在勘误表里的："]
        for item in same_traps:
            lines.append(f"- **{item['id']}**（{_cell(item['listing'])}）：{_cell(item['summary'])}")
    lines += [
        "",
        "与原书同病的那些（印出来就错、包里也错）见书末「原书勘误」的「作者代码包里有没有」一节。",
    ]
    return lines


def section_errata(data):
    lines = [
        "",
        "对照原书配套的作者代码包（2025 秋期末机考的考场资料之一，见附录「考场代码包」），"
        "上面每条编译不过或跑起来错的勘误都能再问一句：**作者自己的代码里也这样吗？**",
        "",
        "- **包里也有**：作者的代码本来就这样，不是排印或 OCR 造成的；",
        "- **包里没有**：印出来的与作者的代码不一致，包里是另一种（通常能编译的）写法。"
        "包与原书同为 2008 年 6 月，谁先谁后不可考，所以只说「不一致」，不说「排印时引入」；",
        "- **包里无此代码**：包里没有对应的实现，无从比较。",
        "",
        "| 勘误 | 清单 | 包里 | 说明 |",
        "| --- | --- | --- | --- |",
    ]
    for item in data.get("findings", []):
        ids = as_list(item.get("errata"))
        if not ids:
            continue
        lines.append(
            f"| {' / '.join(ids)} | {_cell(item['listing'])} | {VERDICTS[item['verdict']]} | {_cell(item['summary'])} |"
        )
    lines += ["", "只在包里、原书没有的缺陷见附录「考场代码包」的「包里的坑」。"]
    return lines


OWNED_SECTIONS = [
    (APPENDIX, "## 清单在包里的位置", section_listing_map),
    (APPENDIX, "## 今天的 g++ 编译得过吗", section_compile),
    (APPENDIX, "## 包里的坑", section_traps),
    (ERRATA_PAGE, "## 作者代码包里有没有", section_errata),
]


def render_owned(text: str, heading: str, body_lines) -> str:
    """把 text 里 heading 这一节的正文换成 body_lines；heading 不存在时抛错。"""
    lines = text.split("\n")
    try:
        start = lines.index(heading)
    except ValueError:
        raise ValueError(f"找不到节标题 {heading!r}")
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
    tail = lines[end:]
    new_body = body_lines + ([""] if tail else [])
    return "\n".join(lines[: start + 1] + new_body + tail)


def book_sections(data):
    """{路径: 生成后的全文}。"""
    out = {}
    for path, heading, builder in OWNED_SECTIONS:
        text = out.get(path) or path.read_text(encoding="utf-8")
        out[path] = render_owned(text, heading, builder(data))
    return out


def check_book(data):
    problems = []
    try:
        rendered = book_sections(data)
    except (OSError, ValueError) as err:
        return [f"书稿生成节：{err}"]
    for path, text in rendered.items():
        if path.read_text(encoding="utf-8") != text:
            problems.append(f"{rel_label(path)}：由登记表生成的节已过期，运行 python3 tools/authorsrc.py --write-book")
    return problems


# ---------------------------------------------------------------- 核对


TOOLCHAIN_ORDER = ("gcc/libstdc++", "clang/libstdc++", "clang/libc++", "apple-clang/libc++")


def recorded_toolchains(data):
    """已登记基线的工具链，g++ 在前（OpenJudge 用的是它）。"""
    keys = {key for entry in data.get("programs", {}).values() for key in entry}
    rank = {key: i for i, key in enumerate(TOOLCHAIN_ORDER)}
    return sorted(keys, key=lambda k: (rank.get(k, len(rank)), k))


def check_programs(data, compiler, out=print):
    """程序清单全工具链一致；本工具链若有基线，逐个程序的编译结果要与之一致。"""
    problems = []
    expected = data.get("programs", {})
    actual_list = programs()
    for rel in actual_list:
        if rel not in expected:
            problems.append(f"{rel}：程序未登记编译结果")
    for rel in expected:
        if rel not in actual_list:
            problems.append(f"{rel}：登记了编译结果，但它已不是含 main 的程序")
    key = toolchain_key(compiler)
    if not any(key in entry for entry in expected.values()):
        out(f"  ⚠ 本机工具链 {key} 没有登记基线（已登记：{'、'.join(recorded_toolchains(data)) or '无'}），"
            f"跳过逐个程序的编译核对；要登记就运行 --write-hashes --cxx \"{' '.join(compiler)}\"")
        return problems
    for rel, entry in compile_sweep(compiler).items():
        want = expected.get(rel, {}).get(key)
        if want is None:
            problems.append(f"{rel}：{key} 下未登记编译结果")
        elif want.get("compiles") != entry["compiles"]:
            verdict = "编译得过" if entry["compiles"] else f"编译不过：{entry.get('error', '')}"
            problems.append(f"{rel}：{key} 下登记为 compiles={want.get('compiles')}，实测{verdict}")
    return problems


def check(data, compiler="auto", out=print):
    """返回问题列表（空 = 全部一致）。"""
    problems = []
    inventory = [item["id"] for item in ledger.parse_inventory()]
    listings = data.get("listings", {})
    if not PACK.is_dir():
        return [f"代码包目录不存在：{rel_label(PACK)}（见 site_visit/README.md）"]

    # 1. 105 条每条都有登记
    for lid in inventory:
        if lid not in listings:
            problems.append(f"{lid}：没有登记（要么指到代码包里的文件与符号，要么写 absent 理由）")
    for lid in listings:
        if lid not in inventory:
            problems.append(f"{lid}：登记表里有，原书清单里没有")

    # 2. 文件存在、符号找得到定义
    for lid, entry in listings.items():
        if "absent" in entry:
            if not str(entry["absent"]).strip():
                problems.append(f"{lid}：absent 必须写理由")
            continue
        if not (PACK / entry.get("file", "")).is_file():
            problems.append(f"{lid}：文件不存在 {entry.get('file')}")
            continue
        if not entry.get("symbols"):
            problems.append(f"{lid}：没有登记符号")
        for rel, symbol, line, _ in slices_for(entry):
            if line is None:
                problems.append(f"{lid}：{rel} 里找不到 `{symbol}` 的定义")

    # 3. 源文件哈希
    hashes = data.get("hashes", {})
    files = source_files(PACK)
    for rel, path in files.items():
        if rel not in hashes:
            problems.append(f"代码包多了未登记的源文件：{rel}（确认来源后用 --write-hashes 登记）")
        elif sha256_of(path) != hashes[rel]:
            problems.append(f"代码包源文件被改动：{rel}（证据不许手改；包真的换了版本才 --write-hashes）")
    for rel in hashes:
        if rel not in files:
            problems.append(f"登记过的源文件不见了：{rel}")

    # 4. 与 2021 版副本逐文件比对
    deltas = data.get("twin_deltas", {})
    if TWIN.is_dir():
        for rel, path in files.items():
            other = twin_path(rel)
            same = other is not None and other.is_file() and sha256_of(other) == sha256_of(path)
            if same and rel in deltas:
                problems.append(f"{rel}：登记为与 2021 版不同，实际相同——删掉这条 twin_deltas")
            elif not same and rel not in deltas:
                where = rel_label(other) if other is not None else "（2021 版无对应目录）"
                problems.append(f"{rel}：与 2021 版 {where} 不同，且不在 twin_deltas 里")

    # 5. 结论：每条的正则在包上成立，编译/运行/内存类勘误都有结论；书稿里的生成节与登记表一致
    problems.extend(check_findings(data))
    problems.extend(check_book(data))

    # 6. 编译结果与对拍
    if compiler == "auto":
        compiler = find_compiler()
    if compiler:
        problems.extend(check_programs(data, compiler, out))
        problems.extend(check_harnesses(data, compiler, out))
    elif compiler is not None:
        out("  ⚠ 找不到 g++/clang++，跳过编译核对")
    return problems


# ---------------------------------------------------------------- 命令


def cmd_summary(data):
    inventory = ledger.parse_inventory()
    listings = data.get("listings", {})
    mapped = [i["id"] for i in inventory if "file" in listings.get(i["id"], {})]
    absent = [i["id"] for i in inventory if "absent" in listings.get(i["id"], {})]
    print(f"作者代码包：{rel_label(PACK)}（{len(source_files(PACK))} 个源文件）")
    print(f"原书清单 {len(inventory)} 条：包里找得到 {len(mapped)} 条，包里没有 {len(absent)} 条，"
          f"未登记 {len(inventory) - len(mapped) - len(absent)} 条")
    if absent:
        print("\n包里没有的清单：")
        for lid in absent:
            print(f"  - {lid}：{listings[lid]['absent']}")
    progs = data.get("programs", {})
    print(f"\n能单独编译的程序 {len(progs)} 个，-std=c++17 语法检查：")
    for key in recorded_toolchains(data):
        ok = sum(1 for p in progs.values() if p.get(key, {}).get("compiles"))
        print(f"  {key}：编译得过 {ok} 个，编译不过 {len(progs) - ok} 个")
    for rel, entry in progs.items():
        bad = [f"{key}：{r.get('error', '')}" for key, r in entry.items() if not r.get("compiles")]
        if bad:
            print(f"  ✗ {rel}\n      " + "\n      ".join(bad))
    return 0


def cmd_listing(data, raw_id):
    inventory_ids = [i["id"] for i in ledger.parse_inventory()]
    lid = normalize_id(raw_id, inventory_ids)
    if lid is None:
        print(f"原书没有清单 {raw_id}", file=sys.stderr)
        return 2
    entry = data.get("listings", {}).get(lid)
    if entry is None:
        print(f"{lid} 尚未登记", file=sys.stderr)
        return 1
    if "absent" in entry:
        print(f"{lid}：作者代码包里没有。{entry['absent']}")
        return 0
    if entry.get("note"):
        print(f"# {lid} · {entry['note']}\n")
    for rel, symbol, line, body in slices_for(entry):
        where = f"{rel}:{line}" if line else rel
        print(f"// ---- {lid} · {symbol} · {rel_label(PACK)}/{where}")
        print(body if body is not None else f"// （找不到 `{symbol}` 的定义）")
        print()
    return 0


def cmd_compile(spec=None):
    compiler = find_compiler(spec)
    if not compiler:
        print("找不到 g++/clang++", file=sys.stderr)
        return 2
    results = compile_sweep(compiler)
    ok = sum(1 for r in results.values() if r["compiles"])
    for rel, entry in results.items():
        print(("  ✓ " if entry["compiles"] else "  ✗ ") + rel + ("" if entry["compiles"] else f"：{entry['error']}"))
    print(f"\n{' '.join(compiler)}（{toolchain_key(compiler)}）-std=c++17：{ok}/{len(results)} 编译得过")
    return 0


def cmd_write_hashes(data, spec=None):
    """重记哈希，并**只替换本工具链**那一列编译结果；别的工具链的基线原样保留。"""
    compiler = find_compiler(spec)
    if not compiler:
        print("找不到编译器，编译结果无法重记", file=sys.stderr)
        return 2
    key = toolchain_key(compiler)
    data["hashes"] = {rel: sha256_of(path) for rel, path in source_files(PACK).items()}
    results = compile_sweep(compiler)
    old = data.get("programs", {})
    merged = {}
    for rel in results:
        entry = {k: v for k, v in old.get(rel, {}).items() if k != key}
        entry[key] = results[rel]
        merged[rel] = dict(sorted(entry.items()))
    data["programs"] = merged
    save_manifest(data)
    ok = sum(1 for r in results.values() if r["compiles"])
    print(f"已重记 {len(data['hashes'])} 个源文件的 sha256；{key} 下 {len(results)} 个程序 {ok} 个编译得过")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--listing", metavar="ID", help="打印某条清单在作者包里的源码，如 3.2 或 算法3.3")
    group.add_argument("--check", action="store_true", help="闸门用：六项核对")
    group.add_argument("--diff", action="store_true", help="只跑对拍程序")
    group.add_argument("--write-book", action="store_true", help="重写书稿里由登记表生成的节")
    group.add_argument("--compile", action="store_true", help="重跑编译扫描并打印")
    group.add_argument("--write-hashes", action="store_true", help="重记 sha256 与编译结果")
    group.add_argument("--export", metavar="DIR", help="把代码包转成 UTF-8 + LF 写到 DIR")
    parser.add_argument("--cxx", metavar="CMD", help='指定编译器命令行，如 "clang++ -stdlib=libc++"（缺省找 g++，再找 clang++）')
    args = parser.parse_args(argv)
    data = load_manifest()

    if args.listing:
        return cmd_listing(data, args.listing)
    if args.compile:
        return cmd_compile(args.cxx)
    if args.write_book:
        for path, text in book_sections(data).items():
            path.write_text(text, encoding="utf-8")
            print(f"已重写 {rel_label(path)} 里由登记表生成的节")
        return 0
    if args.diff:
        compiler = find_compiler(args.cxx)
        if not compiler:
            print("找不到编译器", file=sys.stderr)
            return 2
        problems = check_harnesses(data, compiler)
        for problem in problems:
            print(f"  ✗ {problem}")
        return 1 if problems else 0
    if args.write_hashes:
        return cmd_write_hashes(data, args.cxx)
    if args.export:
        count = export(Path(args.export))
        print(f"已导出 {count} 个源文件到 {args.export}（UTF-8 + LF）")
        return 0
    if args.check:
        compiler = find_compiler(args.cxx) if args.cxx else "auto"
        skipped = []

        def out(line):
            print(line)
            if "没有登记基线" in line or "找不到" in line:
                skipped.append(line)

        problems = check(data, compiler, out)
        for problem in problems:
            print(f"  ✗ {problem}")
        if problems:
            print(f"\n❌ 作者代码包登记有 {len(problems)} 处不一致")
            return 1
        listings = data.get("listings", {})
        mapped = sum(1 for e in listings.values() if "file" in e)
        # 跳过了逐个程序的编译核对就要在结论行里说出来：交接包只贴尾部几行，
        # 一个「✅」加上几行之外的警告，读的人会当成完整验证（2026-09-19 Codex 复核 T-079 指出）。
        partial = (f"；⚠ 部分验证：本机工具链没有登记编译基线，{len(data.get('programs', {}))} 个程序逐个编译的核对被跳过"
                   if skipped else "")
        print(f"{'⚠️' if skipped else '✅'} 作者代码包：{len(listings)} 条清单已登记（包里有 {mapped} 条），"
              f"{len(data.get('hashes', {}))} 个源文件哈希一致，{len(data.get('programs', {}))} 个程序的程序清单一致，"
              f"{len(data.get('findings', []))} 条结论逐条成立，{len(data.get('harnesses', {}))} 个对拍程序通过{partial}")
        return 0
    return cmd_summary(data)


if __name__ == "__main__":
    sys.exit(main())
