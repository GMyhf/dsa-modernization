#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

using dsa::advanced::PatriciaTree;
using dsa::advanced::Trie;

Trie book_example() {
    Trie trie;
    for (const char* word : {"can", "car", "cat", "do"}) {
        trie.insert(word);
    }
    return trie;
}

void test_prefix_sharing() {
    const Trie trie = book_example();
    check(trie.size() == 4, "12.3 四个词");
    // can/car/cat/do 一共 11 个字符，但公共前缀 "ca" 只存一次，所以只有 7 个结点。
    check(trie.node_count() == 7, "12.3 公共前缀只存一次（7 结点 < 11 字符）");
    check(trie.contains("can") && trie.contains("car") && trie.contains("cat") &&
              trie.contains("do"),
          "12.3 四个词都能查到");
    check(!trie.contains("ca"), "12.3 前缀本身不是词");
    check(trie.starts_with("ca"), "12.3 但它是一个存在的前缀");
    check(!trie.starts_with("cb"), "12.3 不存在的前缀");
}

void test_prefix_counting() {
    const Trie trie = book_example();
    check(trie.count_with_prefix("ca") == 3, "12.3 前缀 ca 下有 3 个词");
    check(trie.count_with_prefix("c") == 3, "12.3 前缀 c 下有 3 个词");
    check(trie.count_with_prefix("do") == 1, "12.3 前缀 do 下有 1 个词");
    check(trie.count_with_prefix("z") == 0, "12.3 不存在的前缀计数为 0");
    check(trie.count_with_prefix("") == 4, "12.3 空前缀就是全部");

    const auto keys = trie.keys_with_prefix("ca");
    check(keys == std::vector<std::string>({"can", "car", "cat"}), "12.3 按字典序列出前缀下的词");
    check(trie.keys_with_prefix("zz").empty(), "12.3 不存在的前缀没有词");
}

void test_longest_prefix_match() {
    // 路由表式查询：走到走不动为止，回退到最近的词尾。
    Trie trie;
    trie.insert("do");
    trie.insert("dog");
    check(trie.longest_prefix_of("dogma") == "dog", "12.3 最长前缀取更长的那个");
    check(trie.longest_prefix_of("dot") == "do", "12.3 走不动就回退到最近词尾");
    check(trie.longest_prefix_of("dinner").empty(), "12.3 一个都匹配不上");
    check(trie.longest_prefix_of("dog1") == "dog", "12.3 遇到字母表外字符停下");
}

void test_duplicate_insert_does_not_double_count() {
    Trie trie;
    check(trie.insert("cat"), "12.3 首次插入返回 true");
    check(!trie.insert("cat"), "12.3 重复插入返回 false");
    check(trie.size() == 1, "12.3 重复插入不增加词数");
    // 这一条专门盯 passing 计数：重复插入若不回退，前缀计数会虚高。
    check(trie.count_with_prefix("ca") == 1, "12.3 重复插入不虚增前缀计数");
    check(trie.node_count() == 3, "12.3 重复插入不新建结点");
}

void test_erase_shrinks_the_tree() {
    Trie trie = book_example();
    check(trie.erase("car"), "12.3 删掉存在的词");
    check(!trie.contains("car"), "12.3 删掉后查不到");
    check(trie.size() == 3 && trie.count_with_prefix("ca") == 2, "12.3 删除后计数下降");
    check(trie.node_count() == 6, "12.3 只承载该词的结点被摘掉");
    check(trie.contains("can") && trie.contains("cat"), "12.3 兄弟词不受影响");
    check(!trie.erase("car"), "12.3 重复删除返回 false");
    check(!trie.erase("zzz"), "12.3 删不存在的词返回 false");

    for (const char* word : {"can", "cat", "do"}) {
        trie.erase(word);
    }
    check(trie.size() == 0, "12.3 全删光");
    check(trie.node_count() == 0, "12.3 结点也全部回收，Trie 不是只增不减");
}

void test_alphabet_is_enforced() {
    Trie trie;
    for (const char* bad : {"Cat", "ca t", "ca1"}) {
        bool threw = false;
        try {
            trie.insert(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "12.3 字母表外的关键码被拒绝");
    }
}

void test_patricia_compresses_single_child_chains() {
    PatriciaTree tree;
    for (const char* word : {"can", "car", "cat", "do"}) {
        check(tree.insert(word), "12.3 Patricia 插入新词");
    }
    check(tree.size() == 4, "12.3 Patricia 四个词");
    // 纯 Trie 要 7 个结点；Patricia 把单孩子结点压掉，只剩 3 个内部结点。
    check(tree.internal_count() == 3, "12.3 Patricia 内部结点少于 Trie 结点");
    check(tree.internal_count() < book_example().node_count(), "12.3 压缩确实更省");

    check(tree.contains("can") && tree.contains("car") && tree.contains("cat") &&
              tree.contains("do"),
          "12.3 Patricia 四个词都能查到");
    check(!tree.contains("ca"), "12.3 Patricia 前缀不是词");
    check(!tree.contains("cars"), "12.3 更长的串不是词");
    check(!tree.contains("zzz"), "12.3 完全不相干的串");
    check(!tree.insert("car"), "12.3 Patricia 重复插入返回 false");
    check(tree.size() == 4, "12.3 重复插入不增加词数");
    // 走到叶只看了几位，所以最后那次完整比较不能省。
    check(tree.probe_depth("can") <= 3, "12.3 一次查找只检查少数几位");
}

void test_patricia_handles_prefix_pairs_and_bulk() {
    PatriciaTree tree;
    // 一个关键码是另一个的前缀，是位串 Patricia 最容易写错的情形。
    for (const char* word : {"a", "ab", "abc", "abcd"}) {
        check(tree.insert(word), "12.3 前缀链插入");
    }
    check(tree.contains("a") && tree.contains("ab") && tree.contains("abc") &&
              tree.contains("abcd"),
          "12.3 前缀链上每个词都能查到");
    check(!tree.contains("abcde"), "12.3 前缀链外的串查不到");

    PatriciaTree bulk;
    std::vector<std::string> words;
    for (int i = 0; i < 300; ++i) {
        std::string word = "k";
        int value = i;
        for (int digit = 0; digit < 3; ++digit) {
            word.push_back(static_cast<char>('a' + value % 26));
            value /= 26;
        }
        words.push_back(word);
        check(bulk.insert(word), "12.3 批量插入都是新词");
    }
    bool all_found = true;
    for (const auto& word : words) {
        all_found = all_found && bulk.contains(word);
    }
    check(all_found, "12.3 300 个关键码全部查得到");
    check(bulk.size() == 300, "12.3 批量插入后的词数");
    check(!bulk.contains("kzzz"), "12.3 未插入的关键码查不到");
    check(bulk.internal_count() == 299, "12.3 n 个叶对应 n-1 个内部结点");
}
}  // namespace

int main() {
    test_prefix_sharing();
    test_prefix_counting();
    test_longest_prefix_match();
    test_duplicate_insert_does_not_double_count();
    test_erase_shrinks_the_tree();
    test_alphabet_is_enforced();
    test_patricia_compresses_single_child_chains();
    test_patricia_handles_prefix_pairs_and_bulk();
    std::printf("Trie: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
