# AGENTS.md

This file provides guidance to the AI agent when working with code in this repository.

## What this is

Textbook-modernization project: `dsa_raw.md` (OCR of《数据结构与算法》, 高教社 2008) is being
rewritten into `book/*.md` with compilable modern C++ in `code/`. Everything is verified by
gates, not by hand. Full detail: `CLAUDE.md`, `collab/README.md`, `collab/DECISION_LOG.md`.

## Commands

```bash
python3 tools/handoff.py --verify      # the full gate: self-tests → ledger → book → compile+run
python3 tools/ledger.py                # coverage of the book's 105 listings
python3 tools/check_code.py [unit]     # -Werror + ASan/UBSan AND -O2, under g++ AND clang++
python3 tools/check_doc.py [file]      # book/ hygiene (R1–R17)
python3 tools/sync_book.py --write     # after editing code/, push sources into book code blocks
python3 -m unittest discover -s tests -p 'test_*.py'   # tool self-tests
```

Needs Python 3 (stdlib only) and both g++ and clang++ with C++17 + sanitizers.
**Zero third-party dependencies** in `tools/` and `code/` — adding any is an architecture
decision requiring human sign-off in `collab/PLAN.md`.

## Non-negotiables (the AI will get these wrong without being told)

1. **`dsa_raw.md` is read-only.** It is the only evidence of what the book actually said;
   tests anchor it at 105 listings. All revisions go in `book/`.
2. **Modernizing ≠ wrapping STL.** The hand-written stack/list/tree IS the lesson.
   `std::vector`/`std::stack`/`std::list` as implementation = deleting the chapter.
   `modern.*` containing `<iostream>`/`std::cout`/STL container headers fails the gate
   (exemptions go in `unit.json`'s `d001_exceptions` with a reason).
3. **Book code = compiled code.** C++ in `book/*.md` must be
   ` ```cpp file=code/.../modern.hpp#anchor ` and byte-identical to the file (rule R3).
   Never hand-edit a code block for typesetting — edit the source, run `sync_book.py --write`.
4. **Every defect claim needs evidence** in `legacy.md`: real `error:` or `AddressSanitizer:`
   output plus the reproducing command. "Not modern" is not evidence.
5. **Preserve original numbering** (章/节/算法/图); cross-references are checked (R5/R6/R7).

## C++ style (D-001 — signed convention, not a suggestion)

- C++17 default. `std::size_t`, `std::optional`, `std::swap`, `<type_traits>` — allowed.
- No I/O inside containers: expected-empty ops return `std::optional`, errors throw.
- Storage-management chapters use raw owning pointers + explicit Rule of Five
  (`unique_ptr` there degenerates the lesson).
- `PascalCase` types, trailing-underscore private members, never a data member sharing
  a name with a member function (the book's first compile error).

## Unit layout

`code/<chapter>/<unit>/`: `unit.json` (claimed listings, owner), `legacy.md` (defect
evidence), `modern.hpp` (with `// >>> anchor` markers), `test.cpp` (zero-framework
assertions, non-zero exit = fail). Test standard: **reverting to the book's version must
turn some assertion red** — run a mutation self-check before handing off.
Worked example: `code/ch03/array_stack` + `book/ch03-stack.md`.

## Gotchas (learned the hard way)

- **After editing `book/*.md`, rebuild in this order**: `build_book_pdf.py` → `build_site.py`
  (the site's front page reads the PDF's page count/size from the file) → sync README.md's
  "页 / MB" numbers (`tests/test_readme.py` checks them against the PDF sidecar).
- **Read gate output in full, not just the tail.** A red step scrolls away as later steps
  print; the main-PDF staleness check once went red unnoticed for exactly this reason.
- **Restoring a section number** (原书有、新书缺的小节) means touching four places: the
  heading itself (title must match the original verbatim, R11), `collab/section_gaps.json`
  (remove the gap entry), `collab/slide_coverage.json` (R16 entry), and any 「原书 X.Y.Z 节」
  back-references in the prose.
- `--allow-degraded` is a `check_code.py` flag; `handoff.py` has no pass-through. On this
  macOS machine ASan/UBSan can't run at all (interceptor failure) — the code gate only runs
  degraded here; sanitizer profiles need a Linux machine.

## Collaboration protocol

Two AI agents alternate implement/review via shared files in `collab/` (see
`collab/README.md`). Claim tasks in `collab/PLAN.md` before starting; decisions live only in
`DECISION_LOG.md` (append to overturn, never delete). Before handing off:
`tools/handoff.py --verify` must pass and its tail counts get pasted into
`collab/HANDOFF.md` — "looks fine" is not accepted.
