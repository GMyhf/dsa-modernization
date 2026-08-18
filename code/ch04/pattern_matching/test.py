"""第 4 章模式匹配 Python 断言。"""

import random
import sys
from pathlib import Path
import modern

sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases

checks = 0


def check(condition: bool, name: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(name)


cases = [
    ("abc", "abc"), ("xabc", "abc"), ("aaab", "ab"),
    ("abc", ""), ("", "a"), ("abababd", "ababd"),
]
for text, pattern in cases:
    expected = text.find(pattern)
    expected = expected if expected >= 0 else None
    check(modern.naive_search(text, pattern) == expected, "算法4.6 朴素匹配")
    check(modern.kmp_search(text, pattern) == expected, "算法4.8 KMP")

check(modern.build_next("abcdaabcab") == [-1, 0, 0, 0, -1, 1, 0, 0, 3, 0],
      "算法4.7 图4.11")
random.seed(406)
alphabet = "abc"
for _ in range(300):
    text = "".join(random.choice(alphabet) for _ in range(random.randrange(30)))
    pattern = "".join(random.choice(alphabet) for _ in range(random.randrange(8)))
    expected = text.find(pattern)
    expected = expected if expected >= 0 else None
    check(modern.naive_search(text, pattern) == expected, "算法4.6 随机对拍")
    check(modern.kmp_search(text, pattern) == expected, "算法4.8 随机对拍")

raised = False
try:
    modern.kmp_search("abc", "a", [])
except ValueError:
    raised = True
check(raised, "算法4.8 拒绝长度错误的 next")
shared = shared_cases.load()
for case in shared:
    text, pattern = case.input.split("|", 1)
    if case.expected_error == "invalid_argument":
        raised = False
        try:
            modern.kmp_search(text, pattern, [])
        except ValueError:
            raised = True
        check(raised, "T-047 KMP exception")
    else:
        found = modern.kmp_search(text, pattern)
        actual = -1 if found is None else found
        check(actual == int(case.expected), "T-047 KMP result")
print(f"共享用例: {len(shared)}")
print(f"{checks} 项断言")
