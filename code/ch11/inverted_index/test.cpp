#include "modern.hpp"

#include <cstdio>
#include <set>
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

using dsa::index::InvertedIndex;
using Docs = std::vector<int>;

/// 书上 11.3 的例子：计算机系 -> [0310,0330,0341]，英语专长 -> [0310,0421]。
InvertedIndex book_example() {
    InvertedIndex index;
    index.add_document(310, {"计算机系", "英语专长"});
    index.add_document(330, {"计算机系"});
    index.add_document(341, {"计算机系"});
    index.add_document(421, {"英语专长"});
    return index;
}

void test_book_example() {
    const InvertedIndex index = book_example();
    check(index.postings("计算机系") == Docs({310, 330, 341}), "11.3 计算机系的倒排表");
    check(index.postings("英语专长") == Docs({310, 421}), "11.3 英语专长的倒排表");
    check(index.postings("不存在的词").empty(), "11.3 没有的词项返回空表");

    check(index.and_query({"计算机系", "英语专长"}) == Docs({310}),
          "11.3 计算机系且擅长英语 = 交集 [0310]");
    check(index.or_query({"计算机系", "英语专长"}) == Docs({310, 330, 341, 421}),
          "11.3 或查询 = 并集");
    check(index.not_query("英语专长") == Docs({330, 341}), "11.3 非查询 = 全集求差");
    check(index.document_count() == 4 && index.term_count() == 2, "11.3 4 篇文档 2 个词项");
    check(index.postings_size() == 5, "11.3 倒排表总长度");
}

void test_set_operations_are_ordered_merges() {
    const Docs a{1, 3, 5, 7, 9};
    const Docs b{3, 4, 5, 10};
    check(InvertedIndex::intersect(a, b) == Docs({3, 5}), "11.3 求交");
    check(InvertedIndex::unite(a, b) == Docs({1, 3, 4, 5, 7, 9, 10}), "11.3 求并");
    check(InvertedIndex::difference(a, b) == Docs({1, 7, 9}), "11.3 求差");

    check(InvertedIndex::intersect(a, {}).empty(), "11.3 与空表求交");
    check(InvertedIndex::unite({}, b) == b, "11.3 与空表求并");
    check(InvertedIndex::difference(a, {}) == a, "11.3 与空表求差");
    check(InvertedIndex::intersect(a, a) == a, "11.3 自身求交");
    check(InvertedIndex::difference(a, a).empty(), "11.3 自身求差");
    // 结果必须仍然有序，否则后续的多路求交会错。
    const Docs merged = InvertedIndex::unite(a, b);
    bool sorted = true;
    for (std::size_t i = 1; i < merged.size(); ++i) {
        sorted = sorted && merged[i - 1] < merged[i];
    }
    check(sorted, "11.3 集合运算的结果保持升序且不重复");
}

void test_multi_term_queries() {
    InvertedIndex index;
    index.add_document(1, {"a", "b", "c"});
    index.add_document(2, {"a", "b"});
    index.add_document(3, {"a"});
    check(index.and_query({"a", "b", "c"}) == Docs({1}), "11.3 三个词求交");
    check(index.and_query({"a", "b"}) == Docs({1, 2}), "11.3 两个词求交");
    check(index.and_query({"a"}) == Docs({1, 2, 3}), "11.3 单个词");
    check(index.and_query({}).empty(), "11.3 空查询");
    check(index.and_query({"a", "zzz"}).empty(), "11.3 含不存在词项的与查询为空");
    check(index.or_query({"c", "zzz"}) == Docs({1}), "11.3 或查询忽略不存在的词项");
}

void test_phrase_query_uses_positions() {
    InvertedIndex index;
    index.add_document(1, {"the", "quick", "brown", "fox"});
    index.add_document(2, {"the", "brown", "quick", "fox"});
    index.add_document(3, {"quick", "brown"});
    // 2 号文档两个词都有，但不相邻——只有位置信息能把它排除掉。
    check(index.and_query({"quick", "brown"}) == Docs({1, 2, 3}), "11.3 与查询三篇都命中");
    check(index.phrase_query({"quick", "brown"}) == Docs({1, 3}), "11.3 短语查询排除不相邻的");
    check(index.phrase_query({"brown", "quick"}) == Docs({2}), "11.3 短语有方向");
    check(index.phrase_query({"the", "quick", "brown", "fox"}) == Docs({1}), "11.3 四词短语");
    check(index.phrase_query({"quick"}) == Docs({1, 2, 3}), "11.3 单词短语等于词项查询");
    check(index.phrase_query({}).empty(), "11.3 空短语");
    check(index.phrase_query({"fox", "the"}).empty(), "11.3 顺序不对的短语");
}

void test_repeated_terms_and_ordering_rules() {
    InvertedIndex index;
    index.add_document(1, {"x", "y", "x", "x"});
    check(index.postings("x") == Docs({1}), "11.3 同一文档里重复出现只登记一次");
    check(index.phrase_query({"x", "x"}) == Docs({1}), "11.3 重复词的短语靠位置判定");
    check(index.phrase_query({"y", "x"}) == Docs({1}), "11.3 位置相邻");
    check(index.phrase_query({"x", "y"}) == Docs({1}), "11.3 另一个方向也相邻");

    bool threw = false;
    try {
        index.add_document(1, {"z"});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.3 文档号必须严格递增");
}

void test_against_a_reference_set_implementation() {
    // 用集合实现做对拍：倒排的集合运算必须和直接求交并差一致。
    InvertedIndex index;
    std::vector<std::set<int>> mirror(3);
    const std::vector<std::string> vocabulary{"p", "q", "r"};
    for (int doc = 1; doc <= 200; ++doc) {
        std::vector<std::string> terms;
        for (std::size_t t = 0; t < vocabulary.size(); ++t) {
            if ((doc % (static_cast<int>(t) + 2)) == 0) {
                terms.push_back(vocabulary[t]);
                mirror[t].insert(doc);
            }
        }
        index.add_document(doc, terms);
    }
    for (std::size_t a = 0; a < vocabulary.size(); ++a) {
        check(index.postings(vocabulary[a]) == Docs(mirror[a].begin(), mirror[a].end()),
              "11.3 倒排表与参照集合一致");
        for (std::size_t b = 0; b < vocabulary.size(); ++b) {
            std::vector<int> expected;
            for (const int doc : mirror[a]) {
                if (mirror[b].count(doc) != 0) {
                    expected.push_back(doc);
                }
            }
            check(index.and_query({vocabulary[a], vocabulary[b]}) == expected,
                  "11.3 与查询和参照集合一致");
        }
    }
}
}  // namespace

int main() {
    test_book_example();
    test_set_operations_are_ordered_merges();
    test_multi_term_queries();
    test_phrase_query_uses_positions();
    test_repeated_terms_and_ordering_rules();
    test_against_a_reference_set_implementation();
    std::printf("InvertedIndex: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
