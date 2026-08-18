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
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "collab" / "PLAN.md"

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
