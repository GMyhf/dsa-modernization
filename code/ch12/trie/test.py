"""第 12 章 Trie 与 Patricia 的 Python 断言测试（D-025）。

判据：**若实现退回「拿一个 list 存所有键，靠线性查找 + 公式报指标」的写法，
这里必须有断言变红。** Patricia 的那两条结构断言就是为此写的——
内部结点数任何实现都能凑对（恒为键数减一），所以真正分辨真假的是 `probe_depth()`。
"""

import sys
from pathlib import Path

import modern

sys.path.insert(0, str(Path(__file__).parents[2] / "support"))
import shared_cases  # noqa: E402  共享用例表的读取器（T-047）

checks = 0
failures = 0


def check(condition: bool, name: str) -> None:
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"  FAIL: {name}")


WORDS = ["can", "car", "cat", "do"]


def test_trie_basics() -> None:
    trie = modern.Trie()
    check(all(trie.insert(word) for word in WORDS), "Trie 逐个插入都是新键")
    check(not trie.insert("car"), "Trie 重复插入返回 False")
    check(trie.size() == 4, "Trie 键数")
    check(trie.node_count() == 7, "Trie 结点数（c-a-n/r/t 与 d-o 共 7 个）")
    check(all(trie.contains(word) for word in WORDS), "Trie 全部命中")
    check(not trie.contains("ca"), "Trie 前缀不是键")
    check(trie.starts_with("ca") and not trie.starts_with("cz"), "Trie 前缀存在性")


def test_trie_prefix_counting() -> None:
    trie = modern.Trie()
    for word in WORDS:
        trie.insert(word)
    check(trie.count_with_prefix("ca") == 3, "Trie 前缀计数")
    check(trie.count_with_prefix("") == 4, "Trie 空前缀就是全部")
    check(trie.count_with_prefix("zz") == 0, "Trie 不存在的前缀计数为 0")
    check(trie.keys_with_prefix("ca") == ["can", "car", "cat"], "Trie 按字典序收集")
    check(trie.longest_prefix_of("cartoon") == "car", "Trie 最长前缀匹配")
    check(trie.longest_prefix_of("ca") == "", "Trie 走得到但不是词尾则回退到空")
    raised = False
    try:
        trie.insert("Car")
    except ValueError:
        raised = True
    check(raised, "Trie 只接受 a..z")


def test_trie_erase_reclaims_nodes() -> None:
    """删除要把不再承载任何键的结点摘掉，而不是留一堆空壳。

    「重建一棵新树」的写法能过前两条，但过不了结点数这一条。
    """
    trie = modern.Trie()
    for word in WORDS:
        trie.insert(word)
    check(trie.erase("car") and not trie.contains("car"), "Trie 删除")
    check(not trie.erase("car"), "Trie 重复删除返回 False")
    check(trie.size() == 3 and trie.count_with_prefix("ca") == 2, "Trie 删除后计数跟上")
    check(trie.node_count() == 6, "Trie 删除后回收了那条独占的边")
    trie.insert("cab")
    check(trie.contains("cab") and trie.count_with_prefix("ca") == 3, "Trie 删完还能再插")


def test_patricia_is_a_real_bit_trie() -> None:
    """Patricia 的内部结点数恒等于键数减一——**任何实现都能凑对这个数**。

    所以这里真正的判据是 `probe_depth()`：它必须是**数出来的**降落深度。
    键集 {aa, ab, ba, bb} 的真实深度是 2（先分第一个字符，再分第二个），
    而「按键数减一估」的假实现会给出 3。这一条是分辨真假的那条线。
    """
    tree = modern.PatriciaTree()
    for key in ("aa", "ab", "ba", "bb"):
        check(tree.insert(key), f"Patricia 插入 {key}")
    check(tree.size() == 4, "Patricia 键数")
    check(tree.internal_count() == 3, "Patricia 内部结点数 = 键数 - 1")
    check(tree.probe_depth("aa") == 2, "Patricia 降落深度是数出来的，不是按键数估的")
    check(all(tree.contains(key) for key in ("aa", "ab", "ba", "bb")),
          "Patricia 全部命中")
    check(not tree.contains("ac") and not tree.contains("a"),
          "Patricia 不存在的键不命中")


def test_patricia_depth_does_not_grow_with_key_length() -> None:
    """Patricia 压掉了单孩子的层，所以深度只跟键数有关，跟键有多长无关。

    两个只在最后一位不同的长键：Trie 要 64 层，Patricia 只要 1 个内部结点。
    这是本节要讲的全部收益。
    """
    tree = modern.PatriciaTree()
    long_a = "a" * 64
    long_b = "a" * 63 + "b"
    check(tree.insert(long_a) and tree.insert(long_b), "Patricia 插入两个长键")
    check(tree.internal_count() == 1, "Patricia 两个长键只要一个内部结点")
    check(tree.probe_depth(long_a) == 1, "Patricia 深度不随键长增长")
    check(tree.contains(long_a) and tree.contains(long_b), "Patricia 长键都能命中")


def test_patricia_prefix_keys() -> None:
    """一个键是另一个键的前缀：靠 `bit_of` 超出键长返回 0 来分开。"""
    tree = modern.PatriciaTree()
    for key in ("a", "ab", "abc"):
        check(tree.insert(key), f"Patricia 插入前缀键 {key}")
    check(not tree.insert("ab"), "Patricia 重复键返回 False")
    check(tree.internal_count() == 2, "Patricia 前缀键的内部结点数")
    check(all(tree.contains(key) for key in ("a", "ab", "abc")),
          "Patricia 前缀键互不遮蔽")
    check(tree.size() == 3, "Patricia 前缀键计数")


def test_bit_of() -> None:
    check(modern.bit_of("a", 0) is False, "bit_of 最高位")
    check(modern.bit_of("a", 7) is True, "bit_of 'a'=0x61 的最低位是 1")
    check(modern.bit_of("a", 8) is False, "bit_of 超出键长返回 0")
    check(modern.bit_of("", 0) is False, "bit_of 空键")


def test_empty_patricia() -> None:
    tree = modern.PatriciaTree()
    check(not tree.contains("x"), "空 Patricia 不命中")
    check(tree.size() == 0 and tree.internal_count() == 0, "空 Patricia 计数")
    check(tree.probe_depth("x") == 0, "空 Patricia 深度为 0")
    check(tree.insert("x") and tree.internal_count() == 0,
          "Patricia 单键不需要内部结点")


def main() -> int:
    test_trie_basics()
    test_trie_prefix_counting()
    test_trie_erase_reclaims_nodes()
    test_patricia_is_a_real_bit_trie()
    test_patricia_depth_does_not_grow_with_key_length()
    test_patricia_prefix_keys()
    test_bit_of()
    test_empty_patricia()
    shared = shared_cases.load()
    for case in shared:
        left, right = case.input.split("|", 1)
        if case.expected_error:
            raised = False
            try:
                modern.Trie().insert(left)
            except ValueError:
                raised = True
            check(raised, "T-047 trie exception")
        elif case.operation == "trie":
            trie = modern.Trie()
            for word in left.split(","):
                trie.insert(word)
            check(trie.longest_prefix_of(right) == case.expected, "T-047 trie")
        elif case.operation == "patricia":
            tree = modern.PatriciaTree()
            for word in left.split(","):
                tree.insert(word)
            check(tree.contains(right) == (case.expected == "true"), "T-047 patricia")
    print(f"共享用例: {len(shared)}")
    print(f"Trie(Python): {checks} 项断言，{failures} 失败")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
