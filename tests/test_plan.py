"""collab/PLAN.md 的形状判据。

**缘由（2026-08-18，清账时撞到的）**：`PLAN.md` 开头写着一条硬约束——
「每行恰好 5 列；超出表头的单元格在 GitHub 渲染时被直接丢弃，人看到的会是另一份 PLAN」——
但**没有任何东西在守它**。同一段话还写着「每条任务用一个 `T-<编号>` 标识，
提交与交接里引用它」，而清账时发现 `T-014`、`T-015` 各被两行占用：
`T-014` 是同一件事的两个阶段（已合并），`T-015` 是两件不同的事（已拆成 `T-015a`/`T-015b`）。

编号撞车的代价和习题答案编号撞车是一回事：**引用一个编号时，你不知道说的是哪一条。**
所以把这三条写成判据：列数、编号唯一、状态取值合法。

内容对不对（这条任务是不是真做完了）仍是人工复核项——与 D-014 同一条边界。
"""
import collections
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "collab" / "PLAN.md"
LOG = ROOT / "collab" / "DECISION_LOG.md"

TASK_ROW_RE = re.compile(r"^\| (T-[0-9a-z]+) \|")
STATUSES = {"Backlog", "In progress", "Review", "Done"}


def task_rows(text):
    """返回 [(行号, 任务号, 该行按 | 切出来的单元格)]。"""
    rows = []
    for lineno, line in enumerate(text.splitlines(), 1):
        hit = TASK_ROW_RE.match(line)
        if hit:
            rows.append((lineno, hit.group(1), line.split("|")))
    return rows


class TestPlanShape(unittest.TestCase):
    def setUp(self):
        self.text = PLAN.read_text(encoding="utf-8")
        self.rows = task_rows(self.text)

    def test_there_are_tasks_at_all(self):
        """判据本身要是空转的，后面几条就都是摆设。"""
        self.assertGreater(len(self.rows), 20, "PLAN 里几乎没有任务行，解析多半坏了")

    def test_every_row_has_exactly_five_columns(self):
        """PLAN 自己写的硬约束：多出来的单元格 GitHub 会直接丢掉。"""
        for lineno, tid, cells in self.rows:
            # 首尾各有一个空串，5 列 → 7 段
            self.assertEqual(len(cells), 7,
                             f"PLAN.md:{lineno} {tid} 有 {len(cells) - 2} 列，应当是 5 列")

    def test_task_ids_are_unique(self):
        """`T-<编号>` 是提交与交接引用的地址，撞车了就没法指认。"""
        seen = {}
        for lineno, tid, _ in self.rows:
            self.assertNotIn(tid, seen,
                             f"PLAN.md:{lineno} {tid} 与第 {seen.get(tid)} 行重号")
            seen[tid] = lineno

    def test_status_column_is_one_of_the_four(self):
        for lineno, tid, cells in self.rows:
            status = cells[3].strip()
            self.assertIn(status, STATUSES,
                          f"PLAN.md:{lineno} {tid} 的状态 `{status}` 不在 {sorted(STATUSES)} 里")

    def test_owner_and_note_are_not_empty(self):
        """没有负责人、没有备注的任务行，等于没登记。"""
        for lineno, tid, cells in self.rows:
            self.assertTrue(cells[4].strip(), f"PLAN.md:{lineno} {tid} 没写负责人")
            self.assertTrue(cells[5].strip(), f"PLAN.md:{lineno} {tid} 备注是空的")


if __name__ == "__main__":
    unittest.main()


class TestDecisionNumbers(unittest.TestCase):
    """决策编号也要唯一，索引也要对得上。

    **缘由（2026-08-20，复核方报的）**：`D-032` 被两条不同的决策同时占用——
    Codex 记的「T-044/T-045 分开验收」和我记的「算法侧可以用 STL」。
    我写第二条时没查重号。查下去发现这不是第一次：`D-017` 从 2026-08-17 起就被
    两条内容不同的决策占着（Claude 的 R12、Codex 的 R11），三个月没人发现。

    编号撞车的代价与任务号撞车一模一样——**引用一个编号时，你不知道说的是哪一条**，
    而 `unit.json`、书稿、交接记录里到处是「见 D-0xx」。PLAN 的索引同样会烂：
    同一天查出来 8 条决策**根本没进索引**（D-007~D-010、D-025~D-028）。

    所以三条判据：编号唯一、每条决策都进索引、索引里的编号都指得到真东西。
    决策内容对不对仍是人工复核项——与 D-014 同一条边界。
    """

    HEADING_RE = re.compile(r"^## (D-\S+(?: §\S+)?) ·", re.M)
    INDEX_RE = re.compile(r"^\| (D-\S+(?: §\S+)?) \|", re.M)

    def headings(self, text=None):
        return self.HEADING_RE.findall(text if text is not None else LOG.read_text(encoding="utf-8"))

    def index_ids(self, text=None):
        text = text if text is not None else PLAN.read_text(encoding="utf-8")
        return self.INDEX_RE.findall(text[text.index("## Decision Log"):])

    def test_decision_numbers_are_unique(self):
        counts = collections.Counter(self.headings())
        dupes = sorted(number for number, times in counts.items() if times > 1)
        self.assertEqual(dupes, [], f"DECISION_LOG 里有重号：{dupes}——引用它的人不知道指的是哪一条")

    def test_every_decision_is_in_the_plan_index(self):
        missing = sorted(set(self.headings()) - set(self.index_ids()))
        self.assertEqual(missing, [], f"这些决策没进 PLAN 的索引，等于查不到：{missing}")

    def test_every_index_row_points_at_something_real(self):
        """索引里可以写 `D-001 §3b` 这种子条款，但它的母条必须真的存在。"""
        headings = set(self.headings())
        bases = {h.split(" §")[0] for h in headings}
        dangling = [i for i in self.index_ids()
                    if i not in headings and i.split(" §")[0] not in bases]
        self.assertEqual(dangling, [], f"索引指向不存在的决策：{dangling}")

    def test_duplicate_number_would_go_red(self):
        """变异自检：把两条决策写成同一个号，第一条必须红。"""
        faked = LOG.read_text(encoding="utf-8").replace("## D-030 ·", "## D-029 ·", 1)
        numbers = self.headings(faked)
        self.assertNotEqual(len(numbers), len(set(numbers)))

    def test_missing_index_row_would_go_red(self):
        """变异自检：从索引里删掉一行，第二条必须红。"""
        plan = PLAN.read_text(encoding="utf-8")
        i = plan.index("| D-030 |")
        faked = plan[:i] + plan[plan.index("\n", i) + 1:]
        self.assertTrue(set(self.headings()) - set(self.index_ids(faked)))
