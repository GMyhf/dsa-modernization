"""第 10 章 Python 实现断言；标准容器只在测试侧作裁判。"""
import random
import sys
from pathlib import Path
import modern
sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases

checks = 0
def check(value, name):
    global checks
    checks += 1
    if not value:
        raise AssertionError(name)

item = modern.Item("old")
check(item.key() == "old", "代码10.1 getter")
item.set_key("new")
check(item.key() == "new", "代码10.1 setter")

random.seed(1013)
for size in range(25):
    values = [random.randrange(-20, 21) for _ in range(size)]
    ordered = sorted(set(values))
    for key in range(-22, 23):
        expected = values.index(key) if key in values else None
        check(modern.sequential_search(values, key) == expected, "算法10.2 顺序检索对拍")
        found = modern.binary_search(ordered, key)
        check((found is not None and ordered[found] == key) == (key in ordered), "算法10.3 二分检索对拍")

left = modern.IntSet()
right = modern.IntSet()
for value in [1, 2, 2, 3]:
    left.insert(value)
for value in [2, 3, 4]:
    right.insert(value)
check(left.size() == 3, "算法10.5 插入去重")
check(left.intersection(right).size() == 2, "算法10.6 交集")
check(not left.includes(right), "算法10.7 包含关系")
check(left.erase(1) and not left.erase(1), "代码10.4 删除状态")
check(modern.elf_hash("abc") != modern.elf_hash("abd"), "算法10.8 邻近串散列不同")

table = modern.HashTable(7)
check(table.capacity() == 7 and table.size() == 0, "算法10.9 容量与计数")
for key in [1, 8, 15]:
    check(table.insert(key), "算法10.10 碰撞插入")
check(table.erase(8) and table.contains(15), "算法10.11 穿过墓碑检索")
check(table.insert(22) and table.slot_at(2) == 22, "算法10.13 复用首个墓碑")
check(not table.insert(15) and table.size() == 3, "墓碑前方不能漏掉重复键")
check(table.erase(1) and table.erase(22) and table.erase(15), "算法10.12 墓碑删除")
check(table.size() == 0 and not table.contains(999), "散列表删空")

raised = False
try:
    modern.HashTable(0)
except ValueError:
    raised = True
check(raised, "零容量被拒绝")
shared = shared_cases.load()
for case in shared:
    left, right = case.input.split("|", 1)
    if case.operation == "binary":
        check(modern.binary_search(shared_cases.integers(left), int(right)) == int(case.expected), "T-047 binary")
    elif case.expected_error:
        raised = False
        try:
            modern.HashTable(int(left))
        except ValueError:
            raised = True
        check(raised, "T-047 hash exception")
    else:
        table = modern.HashTable(int(left))
        for key in shared_cases.integers(right):
            table.insert(key)
        check(table.size() == int(case.expected), "T-047 hash")
print(f"共享用例: {len(shared)}")
print(f"{checks} 项断言")
