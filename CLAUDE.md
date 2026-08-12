# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

A textbook-modernization project. `dsa_raw.md` (~1 MB, ~12,000 lines) is a PDF/OCR
extraction of 《数据结构与算法》 (张铭、王腾蛟、赵海燕编著, 高等教育出版社 2008,
ISBN 978-7-04-023961-4) — a Peking University data-structures text whose algorithms are
written in 2008-vintage C++. The goal is to modernize the prose and the code while keeping
the book's pedagogy, numbering, and figures intact.

That code is not merely dated — parts of it **do not compile as printed**. Two confirmed so
far (代码3.2: `int top` collides with member `bool top(T&)`; 算法3.3: loop variable `i`
never declared), plus uninitialized members and a rule-of-three double-free that ASan
reproduces. Assume any listing lifted from the book is broken until a compiler says otherwise.

**Two AI agents (Claude Code and Codex) work this repo in alternating implement/review
rounds.** Before doing anything else, read `collab/README.md` (protocol, red lines,
claim-a-task-first) and `collab/DECISION_LOG.md` (**signed-off decisions — D-001 is the
mandatory C++ style convention**). `collab/PLAN.md` holds what is in flight and who owns it;
decisions live only in DECISION_LOG so the two can't drift apart.

## Commands

```bash
python3 tools/handoff.py --verify      # the gate: self-tests → ledger → book → compile+run
python3 tools/ledger.py                # coverage over the book's 105 listings
python3 tools/ledger.py --pending      # listings nobody has claimed yet
python3 tools/check_code.py [unit]     # -Werror + ASan/UBSan and -O2, both must run green
python3 tools/check_doc.py [file]      # book/ hygiene; --list-rules explains R1–R7
python3 tools/sync_book.py --write     # push code/ sources into the book's code blocks
python3 tools/vendor_figures.py <md>   # download remote figures into book/assets/
python3 -m unittest discover -s tests -p 'test_*.py'          # tool self-tests only
python3 -m unittest tests.test_check_doc.TestR3IncludeContract -v   # a single test class
python3 tools/handoff.py --from claude --to codex   # generate the review packet
```

Requires Python 3 (stdlib only) and g++/clang++ with C++17 + sanitizers. No third-party
dependencies in `tools/` or `code/` — adding any is an architecture decision for `PLAN.md`.

## The style convention (D-001, signed off 2026-08-12)

Full text in `collab/DECISION_LOG.md`; the four load-bearing rules:

1. **C++17** by default (`unit.json` may override per unit, with a reason in `legacy.md`).
2. **STL is infrastructure, not a substitute.** `std::size_t`, `std::optional`,
   `<type_traits>`, `std::swap` — yes. Replacing the chapter's hand-written structure with
   `std::vector`/`std::stack`/`std::list` — no. Where storage management *is* the lesson
   (e.g. ch3's array stack), use a raw owning pointer plus an explicit Rule of Five; with
   `unique_ptr` the Rule of Five degenerates into ceremony and stops teaching anything.
3. **No I/O inside containers.** Expected empty states (`pop()`/`top()` on empty) return
   `std::optional`; genuine errors throw (`std::out_of_range`, `std::overflow_error`,
   `std::invalid_argument`).
4. **Names:** `PascalCase` types, trailing-underscore private members, and never a data
   member sharing a name with a member function — that collision is the book's very first
   compile error.

## How the pieces fit

The gate is the architecture. Three arbiters, each answering a question documents cannot:

- **`tools/ledger.py`** — derives coverage instead of tracking it by hand. Inventory comes
  from parsing `【算法X.Y】`/`【代码X.Y】` out of `dsa_raw.md` (105 listings); "covered" is
  the union of `listings` across `code/**/unit.json`; "excluded" comes from
  `collab/exclusions.json` (which *requires* reason + owner + date). Pending is what's left.
  A hand-maintained progress table rots; a derived one cannot. The invariant
  `covered + excluded + pending == 105` is what stops work from silently disappearing.
- **`tools/check_doc.py`** — 7 rules over `book/`. The load-bearing one is **R3**: every
  ```cpp block must carry `file=code/.../modern.hpp#anchor` and match that file (or the
  slice between `// >>> anchor` and `// <<< anchor`) verbatim. Printed code and compiled
  code are the same bytes, by construction. `sync_book.py --write` is the writer half of
  that contract; R3 is the checker half.
- **`tools/check_code.py`** — compiles every unit twice (`-Werror` + ASan/UBSan, and
  `-O2`) and runs it. Both profiles matter: a heap overflow that UBSan aborts on in the
  debug build passes *silently* under `-O2`.

`tools/repo.py` exists only because `Path.relative_to(ROOT)` throws outside the repo and
three tools each hit it independently.

### A unit of work

`code/<chapter>/<unit>/` holds `unit.json` (which listings it claims, C++ standard, owner),
`legacy.md` (original listing → each defect **with reproducible command + real output** →
modern version), `modern.hpp`, and `test.cpp` (zero-framework assertions; non-zero exit
fails). The standard for a test: **if the implementation regressed to the book's version,
some assertion here must go red.** Run a mutation self-check before handing off.

`code/ch03/array_stack` + `book/ch03-stack.md` is the worked example; copy its shape.

## Non-negotiables

1. **`dsa_raw.md` is read-only.** It is the only evidence of what the book actually said —
   the line between "the book's bug" and "our bug" depends on it. Revisions go in `book/`.
   `tests/test_ledger.py` anchors it at 105 listings / 70 算法 / 35 代码 / 5 missing end
   markers; editing the raw file turns the gate red.
2. **Modernizing ≠ replacing with STL.** This is a data-structures textbook: the
   hand-written stack, list, and tree *are* the lesson. Fix ownership, copy semantics,
   exception safety, interfaces, testability — not the implementation strategy. Wrapping
   `std::stack` deletes the section.
3. **Every defect claim needs evidence.** "Not modern" is not evidence; `error:` and
   `AddressSanitizer:` are.
4. Numbering and cross-references must not drift from the original (R5/R6/R7).

## Document structure of `dsa_raw.md`

| Region | Lines | Contents |
|---|---|---|
| Front matter | 1–447 | Title, 内容提要, CIP data, preface, full table of contents (dot-leader lines with page numbers) |
| 第1章 概论 | 448 | Problem solving, ADTs, asymptotic analysis |
| 第2章 线性表 | 1145 | |
| 第3章 栈与队列 | 1775 | |
| 第4章 字符串 | 2899 | |
| 第5章 二叉树 | 3528 | |
| 第6章 树 | 4735 | |
| 第7章 图 | 5639 | |
| 第8章 内排序 | 6740 | |
| 第9章 文件管理和外排序 | 8142 | |
| 第10章 检索 | 8629 | |
| 第11章 索引技术 | 9871 | |
| 第12章 高级数据结构 | 10675 | |
| 参考文献 | 11927 | |

Heading convention (240 headings total): `#` = chapter, `##` = section (`1.1`) or one of
the three per-chapter tail sections, `###` = subsection (`1.1.1`). Every one of the 12
chapters ends with `## 本章小结`, `## 习题`, `## 上机题` — these are reliable chapter-boundary
markers.

Algorithm and code listings are delimited by CJK bracket markers in the prose:
`【算法3.3】…【算法3.3结束】` and `【代码3.2】…【代码3.2结束】`. **105 listings total**
(70 算法 + 35 代码), spread over 11 chapters — 第11章 has none. The opening marker sits
*outside* the fenced block; the closing marker is usually swallowed *inside* it, and for
five listings (算法2.11, 代码3.1, 代码5.8, 算法7.6, 算法7.9) OCR ate it entirely, so their
end boundary has to be set by hand. `tools/ledger.py` is the parser of record for all of
this — don't re-derive these counts by hand.

## OCR artifacts — expect all of these

The file has never been cleaned. Anything derived from it must account for:

- **Code fences carry wrong languages.** 170 fences: 85 bare, 45 `cpp`, 25 `c`, plus
  bogus `javascript` (5), `csv` (5), `hcl` (2), `typescript`, `matlab`, `lisp`. Every
  listing in the book is C++.
- **Code is not compilable.** Tokens are split by spurious spaces (`i + +`, `< <`,
  `= =`, `G. VerticesNum( )`, `#include < iostream >`, `arrStack < T > : : push`);
  closing braces `}` are frequently misread as `1`, `一`, or dropped; full-width
  punctuation leaks in (`；` for `;`, `−` for `-`).
- **Code comments are torn out of their blocks.** Right-margin comments in the original
  print often land as standalone prose paragraphs between two fences (see lines 536–544).
  Re-attaching them requires reading the surrounding text, not a regex.
- **Math is LaTeX with OCR-mangled spacing**, e.g. `${ \mathsf { B } } _ { 3 }$`.
  4,960 `$` delimiters in total — 182 `$$` display blocks, the rest inline `$…$` spans.
- **Tables are raw single-line HTML**, not Markdown: 49 `<table>` blocks, 275 `<tr>`,
  1,534 `<td>`, with `rowspan`/`colspan` and frequently garbled cells (`8` for `∞`).
- **All 292 figures are remote hotlinks** to
  `https://raw.githubusercontent.com/GMyhf/img/main/img/<sha256>.jpg` — no local copies,
  no alt text, all `![]()`. Captions (`图3.3 顺序栈的存储结构`) follow the image as a
  soft-broken line. Any offline or reproducible build must vendor these first.
- **1,787 lines end in a trailing double-space soft break**, a leftover of the print
  line wrapping rather than meaningful structure.
- TOC lines use `…` dot leaders and are broken mid-title across lines.

## Navigating the file

It is one 1 MB file — read slices, don't load it whole.

```bash
# Map the document
grep -n '^#' dsa_raw.md

# Line range of one chapter
grep -n '^# 第7章' dsa_raw.md

# Extract a chapter to the scratchpad for focused work
awk '/^# 第3章/,/^# 第4章/' dsa_raw.md > /tmp/ch3.md

# Find a specific listing and its end marker
grep -n '【算法3.3' dsa_raw.md

# Audit the artifacts above
grep -o '^```[a-zA-Z]*' dsa_raw.md | sort | uniq -c | sort -rn
grep -o '!\[\](http[^)]*)' dsa_raw.md | sort -u | wc -l
```

`Read` with `offset`/`limit` on a known line range is preferable to `grep`-piping large
regions into context.

## Editing conventions

- Preserve the original numbering (章/节/算法/图) when restructuring — the prose
  cross-references it by number ("如图3.3所示", "可参考本书第7章的Floyd 算法"), and
  `check_doc.py` R6/R7 enforce that those references still resolve.
- Two different fidelity standards, don't mix them up:
  - **`legacy.md`** quotes the book. Repair only OCR damage (`1`→`}`, `− `→`-`, spacing)
    and change **no logic** — its whole value is being an accurate record of what was printed.
  - **`modern.hpp`** is new code. It keeps the book's *data structure and strategy*
    (hand-managed buffer, algorithm 3.3's doubling) but not its C++ idioms.
- Working in the raw file's units (chapters, listings) beats working in line ranges:
  `tools/ledger.py --json` gives each listing's id, chapter, and line.
- Before handing off: mutation self-check (break the implementation, confirm a test goes
  red), then `tools/handoff.py --verify`, then append to `collab/HANDOFF.md` with the
  gate's actual tail counts pasted in — not "looks fine".
