#include "modern.hpp"

#include <algorithm>
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

using dsa::index::BitmapIndex;
using dsa::index::run_length_decode;
using dsa::index::run_length_encode;
using dsa::index::SignatureFile;
using Records = std::vector<std::size_t>;

/// 低基数属性：性别只有两个取值，正是位图最擅长的场合。
BitmapIndex small_index() {
    BitmapIndex index;
    for (const char* value : {"男", "女", "女", "男", "男"}) {
        index.add_record(value);
    }
    return index;
}

void test_one_bit_per_record() {
    const BitmapIndex index = small_index();
    check(index.record_count() == 5 && index.distinct_values() == 2, "11.5 5 条记录 2 个取值");
    check(index.select("男") == Records({0, 3, 4}), "11.5 男的记录号");
    check(index.select("女") == Records({1, 2}), "11.5 女的记录号");
    check(index.select("未知").empty(), "11.5 没出现过的取值是空位串");
    check(index.words() == 2, "11.5 5 条记录每个取值占一个机器字");
}

void test_queries_are_word_operations() {
    BitmapIndex index;
    for (int i = 0; i < 200; ++i) {
        index.add_record((i % 3) == 0 ? "及格" : "不及格");
    }
    check(index.select("及格").size() == 67, "11.5 及格的记录数");

    index.reset_ops();
    const auto both = index.select_and("及格", "不及格");
    check(both.empty(), "11.5 互斥取值求交为空");
    // 200 条记录只用了 4 个机器字，所以 4 次字运算就处理完了全部记录。
    check(index.word_ops() == 4, "11.5 200 条记录只做 4 次字运算");

    const auto either = index.select_or("及格", "不及格");
    check(either.size() == 200, "11.5 求并覆盖全部记录");
    const auto negated = index.select_not("及格");
    check(negated.size() == 133, "11.5 取反的记录数");
    check(negated == index.select("不及格"), "11.5 取反等于另一个取值");
}

void test_tail_bits_are_masked() {
    // 记录数不是 64 的整数倍时，最后一个字里多出来的位取反后会冒出不存在的记录号。
    BitmapIndex index;
    for (int i = 0; i < 5; ++i) {
        index.add_record("a");
    }
    check(index.select_not("a").empty(), "11.5 取反不会冒出不存在的记录");
    BitmapIndex exact;
    for (int i = 0; i < 64; ++i) {
        exact.add_record("a");
    }
    check(exact.select_not("a").empty(), "11.5 正好一个字时也不冒");
    check(exact.select("a").size() == 64, "11.5 正好一个字装满");
}

void test_run_length_compression() {
    // 稀疏位图：1000 条记录里只有 3 条命中，大片全 0 的字。
    BitmapIndex sparse;
    for (int i = 0; i < 1000; ++i) {
        sparse.add_record(i < 3 ? "命中" : "其他");
    }
    const auto bits = sparse.bitmap("命中");
    const auto encoded = run_length_encode(bits);
    check(encoded.size() < bits.size(), "11.5 稀疏位图压得下来");
    check(run_length_decode(encoded) == bits, "11.5 压缩可逆");

    // 交替的稠密位图压不动，反而更大——位图压缩不是万能的。
    std::vector<std::uint64_t> alternating;
    for (std::size_t i = 0; i < 16; ++i) {
        alternating.push_back(i % 2 == 0 ? 0xAAAAAAAAAAAAAAAAULL : 0x5555555555555555ULL);
    }
    check(run_length_encode(alternating).size() > alternating.size(),
          "11.5 交替模式压缩后反而更大，如实呈现");
    check(run_length_decode(run_length_encode(alternating)) == alternating,
          "11.5 压缩仍然可逆");

    check(run_length_encode({}).empty(), "11.5 空位图");
    check(run_length_decode({}).empty(), "11.5 空编码");
    bool threw = false;
    try {
        (void)run_length_decode({3});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.5 编码流必须成对");
}

void test_signature_has_no_false_negatives() {
    SignatureFile signatures(2);
    std::vector<std::vector<std::string>> corpus;
    for (int doc = 0; doc < 300; ++doc) {
        std::vector<std::string> terms;
        for (int t = 0; t < 4; ++t) {
            terms.push_back("t" + std::to_string((doc * 7 + t * 13) % 40));
        }
        corpus.push_back(terms);
        signatures.add(doc, terms);
    }
    check(signatures.size() == 300, "11.5 300 篇文档的签名");

    const std::vector<std::string> query{"t5"};
    const auto candidates = signatures.candidates(query);

    std::size_t truth = 0;
    std::size_t false_positives = 0;
    bool no_false_negative = true;
    for (int doc = 0; doc < 300; ++doc) {
        bool really_has = false;
        for (const auto& term : corpus[static_cast<std::size_t>(doc)]) {
            really_has = really_has || term == "t5";
        }
        const bool is_candidate =
            std::find(candidates.begin(), candidates.end(), doc) != candidates.end();
        if (really_has) {
            ++truth;
            no_false_negative = no_false_negative && is_candidate;  // 绝不能漏
        } else if (is_candidate) {
            ++false_positives;
        }
    }
    check(truth > 0, "11.5 确实有真正命中的文档");
    check(no_false_negative, "11.5 签名不产生假阴性——这是它能当粗筛的前提");
    check(candidates.size() >= truth, "11.5 候选集是真结果的超集");
    check(false_positives == 0, "11.5 文档词少、签名不挤时可以没有假阳性");

    bool threw = false;
    try {
        SignatureFile bad(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.5 每词位数为 0 被拒绝");
}

void test_signature_false_positives_appear_when_it_saturates() {
    // 文档词一多，签名里置 1 的位就密，别的词凑巧把查询的位都占上——假阳性就来了。
    // 这不是实现缺陷，是签名法本身的性质，所以必须「先粗筛、再回原文精查」。
    SignatureFile signatures(2);
    std::vector<std::vector<std::string>> corpus;
    for (int doc = 0; doc < 200; ++doc) {
        std::vector<std::string> terms;
        for (int t = 0; t < 20; ++t) {
            terms.push_back("w" + std::to_string((doc * 11 + t * 3) % 60));
        }
        corpus.push_back(terms);
        signatures.add(doc, terms);
    }
    const std::vector<std::string> query{"w7"};
    const auto candidates = signatures.candidates(query);

    std::size_t truth = 0;
    std::size_t false_positives = 0;
    bool no_false_negative = true;
    for (int doc = 0; doc < 200; ++doc) {
        bool really_has = false;
        for (const auto& term : corpus[static_cast<std::size_t>(doc)]) {
            really_has = really_has || term == "w7";
        }
        const bool is_candidate =
            std::find(candidates.begin(), candidates.end(), doc) != candidates.end();
        if (really_has) {
            ++truth;
            no_false_negative = no_false_negative && is_candidate;
        } else if (is_candidate) {
            ++false_positives;
        }
    }
    check(no_false_negative, "11.5 签名再挤也不会假阴性");
    check(false_positives > 0, "11.5 签名一挤就出现假阳性，必须回原文确认");
    check(candidates.size() == truth + false_positives, "11.5 候选 = 真命中 + 假阳性");
}
}  // namespace

int main() {
    test_one_bit_per_record();
    test_queries_are_word_operations();
    test_tail_bits_are_masked();
    test_run_length_compression();
    test_signature_has_no_false_negatives();
    test_signature_false_positives_appear_when_it_saturates();
    std::printf("BitmapIndex: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
