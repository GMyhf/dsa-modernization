"""多级线性索引的 Python 断言测试（D-025 / D-026）。

判据：**若实现退回「把主文件从头扫一遍」的写法，这里必须有断言变红。**
多级索引的全部意义是「页访问次数随层数走，而不是随记录数走」，
所以 `page_reads()` 是被测对象本身，不是附带信息。
"""

import sys
from pathlib import Path

import modern
sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases

checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


def test_dense_index() -> None:
    """稠密索引：每条记录一个索引项，所以主文件不必有序。"""
    index = modern.MultiLevelIndex(modern.DENSE, 2, 2)
    index.load([(3, "c"), (1, "a"), (2, "b")])
    check(index.find(2) == "b", "稠密索引在无序主文件上也能命中")
    check(index.find(1) == "a" and index.find(3) == "c", "稠密索引全部命中")
    check(index.find(9) is None, "稠密索引不存在的键返回 None")
    check(index.entries() == 3, "稠密索引每条记录一个索引项")
    check(index.levels() == 2, "稠密索引 3 项、每页 2 项 → 两层")


def test_sparse_index() -> None:
    """稀疏索引：每页一个索引项，因此要求主文件按键有序。"""
    index = modern.MultiLevelIndex(modern.SPARSE, 2, 2)
    index.load([(1, "a"), (2, "b"), (3, "c"), (4, "d")])
    check(index.find(3) == "c", "稀疏索引命中页内记录")
    check(all(index.find(k) == v for k, v in ((1, "a"), (2, "b"), (4, "d"))),
          "稀疏索引全部命中")
    check(index.find(5) is None, "稀疏索引不存在的键返回 None")
    check(index.entries() == 2, "稀疏索引 4 条记录、每页 2 条 → 2 个索引项")
    raised = False
    try:
        index.load([(2, "b"), (1, "a")])
    except ValueError:
        raised = True
    check(raised, "稀疏主文件必须有序")


def test_page_reads_follow_levels_not_records() -> None:
    """多级索引的卖点：查一次的页访问次数跟**层数**走，不跟记录数走。

    128 条记录、每页 4 条、索引页每页 4 项：层数是个位数，而线性扫描要读 32 页。
    退化成「从头扫主文件」的实现在这里必红。
    """
    records = [(key, f"v{key}") for key in range(128)]
    index = modern.MultiLevelIndex(modern.SPARSE, 4, 4)
    index.load(records)
    levels = index.levels()
    check(index.data_pages() == 32, "128 条记录、每页 4 条 → 32 个数据页")
    check(levels <= 4, f"多级索引层数是对数级（实得 {levels} 层）")
    index.reset_counters()
    check(index.find(100) == "v100", "大表上仍能命中")
    reads = index.page_reads()
    check(reads <= levels + 1, f"一次查找只读 {reads} 页（层数 {levels}）")
    check(reads < index.data_pages(), "页访问次数远少于数据页数——这才叫索引")


def test_bad_page_capacity() -> None:
    for records_per_page, entries_per_page in ((0, 2), (2, 1)):
        raised = False
        try:
            modern.MultiLevelIndex(modern.DENSE, records_per_page, entries_per_page)
        except ValueError:
            raised = True
        check(raised, f"页容量 ({records_per_page}, {entries_per_page}) 必须被拒绝")


def main() -> int:
    test_dense_index()
    test_sparse_index()
    test_page_reads_follow_levels_not_records()
    test_bad_page_capacity()
    shared = shared_cases.load()
    for case in shared:
        rows_text, key_text = case.input.split("|", 1)
        rows = [(int(row.split(":", 1)[0]), row.split(":", 1)[1]) for row in rows_text.split(",")]
        index = modern.MultiLevelIndex(case.operation, 2, 2)
        if case.expected_error:
            raised = False
            try:
                index.load(rows)
            except ValueError:
                raised = True
            check(raised, "T-047 linear exception")
        else:
            index.load(rows)
            check(index.find(int(key_text)) == case.expected, "T-047 linear index")
    print(f"共享用例: {len(shared)}")
    print(f"LinearIndex(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
