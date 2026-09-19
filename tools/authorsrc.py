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

`--check`（闸门用）核对五件事：
  1. 原书 105 条清单**每一条**都有登记——要么指到代码包里的文件与符号，要么写明
     「包里没有」的理由（与 exclusions.json 同一个规矩：不许悄悄漏掉）；
  2. 登记的文件存在、每个符号都能在文件里找到**定义**（不是声明）；
  3. 代码包源文件与登记的 sha256 一致——包是证据，证据不许手改；
  4. 与 `ref_数据结构与算法A 2021秋/SourceCodes/`（同一套代码的 2021 版副本）逐文件
     比对，只允许登记过的差异；
  5. 每个带 `main` 的程序在今天的 g++（`-std=c++17 -fsyntax-only`）下编译得过或
     编译不过，与登记一致。编译器缺失时这一项只提示不判红。

用法:
  python3 tools/authorsrc.py                    # 概况：登记了几条、几条包里没有、编译情况
  python3 tools/authorsrc.py --listing 3.2      # 打印代码3.2 在作者包里的那一段（UTF-8）
  python3 tools/authorsrc.py --listing 代码3.2  # 同上
  python3 tools/authorsrc.py --compile          # 重跑编译扫描并打印（不写登记表）
  python3 tools/authorsrc.py --check            # 闸门用：五项核对，不一致退出码 1
  python3 tools/authorsrc.py --write-hashes     # 代码包更新后重记 sha256 与编译结果
  python3 tools/authorsrc.py --export DIR       # 把代码包转成 UTF-8 + LF 写到 DIR
"""
import argparse
import concurrent.futures
import hashlib
import json
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


def programs(files=None):
    """包里含 main 的源文件（相对路径列表）。"""
    files = source_files(PACK) if files is None else files
    return [rel for rel, path in files.items() if rel.endswith(".cpp") and MAIN_RE.search(_mask(decode(path)))]


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


def find_compiler():
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return name
    return None


def first_error(stderr: str) -> str:
    for line in stderr.splitlines():
        if "error:" in line:
            return line.split("error:", 1)[1].strip()
    return stderr.strip().splitlines()[0] if stderr.strip() else ""


def compile_sweep(compiler):
    """{程序相对路径: {"compiles": bool, "error": 首条 error 文本}}；在 UTF-8 副本上编译。"""
    work = BUILD / "utf8"
    export(work)
    results = {}

    def one(rel):
        src = work / rel
        proc = subprocess.run(
            [compiler, "-std=c++17", "-w", "-fsyntax-only", "-I", str(src.parent), str(src)],
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


# ---------------------------------------------------------------- 核对


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

    # 5. 编译结果
    if compiler == "auto":
        compiler = find_compiler()
    if compiler:
        expected = data.get("programs", {})
        actual = compile_sweep(compiler)
        for rel, entry in actual.items():
            want = expected.get(rel)
            if want is None:
                problems.append(f"{rel}：程序未登记编译结果")
            elif want.get("compiles") != entry["compiles"]:
                verdict = "编译得过" if entry["compiles"] else f"编译不过：{entry.get('error', '')}"
                problems.append(f"{rel}：登记为 compiles={want.get('compiles')}，实测{verdict}")
        for rel in expected:
            if rel not in actual:
                problems.append(f"{rel}：登记了编译结果，但它已不是含 main 的程序")
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
    ok = sum(1 for p in progs.values() if p.get("compiles"))
    print(f"\n含 main 的程序 {len(progs)} 个：今天的 g++ -std=c++17 编译得过 {ok} 个，编译不过 {len(progs) - ok} 个")
    for rel, entry in progs.items():
        if not entry.get("compiles"):
            print(f"  ✗ {rel}：{entry.get('error', '')}")
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


def cmd_compile():
    compiler = find_compiler()
    if not compiler:
        print("找不到 g++/clang++", file=sys.stderr)
        return 2
    results = compile_sweep(compiler)
    ok = sum(1 for r in results.values() if r["compiles"])
    for rel, entry in results.items():
        print(("  ✓ " if entry["compiles"] else "  ✗ ") + rel + ("" if entry["compiles"] else f"：{entry['error']}"))
    print(f"\n{compiler} -std=c++17：{ok}/{len(results)} 编译得过")
    return 0


def cmd_write_hashes(data):
    compiler = find_compiler()
    if not compiler:
        print("找不到 g++/clang++，编译结果无法重记", file=sys.stderr)
        return 2
    data["hashes"] = {rel: sha256_of(path) for rel, path in source_files(PACK).items()}
    data["programs"] = compile_sweep(compiler)
    save_manifest(data)
    print(f"已重记 {len(data['hashes'])} 个源文件的 sha256、{len(data['programs'])} 个程序的编译结果")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--listing", metavar="ID", help="打印某条清单在作者包里的源码，如 3.2 或 算法3.3")
    group.add_argument("--check", action="store_true", help="闸门用：五项核对")
    group.add_argument("--compile", action="store_true", help="重跑编译扫描并打印")
    group.add_argument("--write-hashes", action="store_true", help="重记 sha256 与编译结果")
    group.add_argument("--export", metavar="DIR", help="把代码包转成 UTF-8 + LF 写到 DIR")
    args = parser.parse_args(argv)
    data = load_manifest()

    if args.listing:
        return cmd_listing(data, args.listing)
    if args.compile:
        return cmd_compile()
    if args.write_hashes:
        return cmd_write_hashes(data)
    if args.export:
        count = export(Path(args.export))
        print(f"已导出 {count} 个源文件到 {args.export}（UTF-8 + LF）")
        return 0
    if args.check:
        problems = check(data)
        for problem in problems:
            print(f"  ✗ {problem}")
        if problems:
            print(f"\n❌ 作者代码包登记有 {len(problems)} 处不一致")
            return 1
        listings = data.get("listings", {})
        mapped = sum(1 for e in listings.values() if "file" in e)
        print(f"✅ 作者代码包：{len(listings)} 条清单已登记（包里有 {mapped} 条），"
              f"{len(data.get('hashes', {}))} 个源文件哈希一致，{len(data.get('programs', {}))} 个程序编译结果与登记一致")
        return 0
    return cmd_summary(data)


if __name__ == "__main__":
    sys.exit(main())
