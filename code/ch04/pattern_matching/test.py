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


# T-072：最小周期。参照物是定义本身，与 border_lengths 不共用一行。
def brute_minimal_period(s: str) -> int:
    for p in range(1, len(s) + 1):
        if all(s[i] == s[i + p] for i in range(len(s) - p)):
            return p
    return 0


def brute_repetition_count(s: str) -> int:
    for d in range(1, len(s)):
        if len(s) % d == 0 and s[:d] * (len(s) // d) == s:
            return len(s) // d
    return 1 if s else 0


all_strings = [""]
for length in range(1, 11):
    for mask in range(1 << length):
        all_strings.append("".join("b" if (mask >> i) & 1 else "a" for i in range(length)))
check(len(all_strings) == 2047, "{a,b} 上长度 0..10 的串全部穷举")
period_bad = 0
power_bad = 0
border_bad = 0
next_wrong = 0
for s in all_strings:
    period_bad += modern.minimal_period(s) != brute_minimal_period(s)
    k = brute_repetition_count(s)
    power_bad += modern.repetition_count(s) != k or modern.is_repetition(s) != (k > 1)
    border = modern.border_lengths(s)
    optimized = modern.build_next(s)
    for i in range(1, len(s) + 1):
        truth = brute_minimal_period(s[:i])
        border_bad += border[i - 1] != i - truth
        if i < len(s):
            next_wrong += i - optimized[i] != truth
check(period_bad == 0, "T-072 minimal_period 穷举对拍")
check(power_bad == 0, "T-072 repetition_count / is_repetition 穷举对拍")
check(border_bad == 0, "T-072 border_lengths 每个前缀对拍")
check(next_wrong > 1000, "陷阱：优化版 next 在上千个前缀上给出错误周期")

check(modern.build_next("aaaa") == [-1, -1, -1, -1], "aaaa 的优化版 next 全是 -1")
check(3 - modern.build_next("aaaa")[3] == 4, "陷阱：用优化版 next 算前缀 aaa 的周期得 4")
check(3 - modern.border_lengths("aaaa")[2] == 1, "用未优化的边界长度得 1")
check(modern.minimal_period("abcd") == 4, "边界为 0 时周期等于串长")
check(not modern.is_repetition("abcd"), "陷阱：4 % 4 == 0 但 abcd 不是循环串")
check(not modern.is_repetition("a"), "单字符串不是循环串")
check(not modern.is_repetition("ababa"), "周期不整除长度不是循环串")
check(modern.is_repetition("abab"), "abab 是循环串")
check(modern.minimal_period("") == 0 and modern.repetition_count("") == 0, "空串约定")
check(not modern.is_repetition(""), "空串不是循环串")

shared = shared_cases.load()
for case in shared:
    if case.operation == "period":
        check(modern.minimal_period(case.input) == int(case.expected), f"T-072 最小周期 {case.name}")
        continue
    if case.operation == "repetition":
        check(modern.repetition_count(case.input) == int(case.expected), f"T-072 字符串乘方 {case.name}")
        continue
    check(case.operation in ("search", "bad_next"), f"共享用例 operation 可识别：{case.operation}")
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
