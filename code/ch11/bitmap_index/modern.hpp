#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace dsa::index {

// >>> bitmap-index
/// 位图索引：适合取值很少的属性（性别、省份、是否及格）。
/// 每个属性值一条位串，第 i 位表示第 i 条记录有没有这个值。
/// AND / OR / NOT 因此变成**机器字上的按位运算**——一次运算处理 64 条记录。
class BitmapIndex {
public:
    static constexpr std::size_t kBitsPerWord = 64;

    void add_record(const std::string& value) {
        const std::size_t record = records_;
        ++records_;
        for (auto& entry : maps_) {
            entry.second.resize(word_count());
        }
        std::vector<std::uint64_t>& bits = maps_[value];
        bits.resize(word_count());
        bits[record / kBitsPerWord] |= std::uint64_t{1} << (record % kBitsPerWord);
    }

    [[nodiscard]] std::vector<std::uint64_t> bitmap(const std::string& value) const {
        const auto at = maps_.find(value);
        if (at == maps_.end()) {
            return std::vector<std::uint64_t>(word_count(), 0);
        }
        return at->second;
    }

    [[nodiscard]] std::vector<std::size_t> select(const std::string& value) const {
        return to_records(bitmap(value));
    }

// >>> bitmap-ops
    /// 「与」：逐字 `&`。`word_ops()` 数的就是这里做了几次字运算。
    [[nodiscard]] std::vector<std::size_t> select_and(const std::string& a,
                                                      const std::string& b) const {
        return to_records(combine(bitmap(a), bitmap(b), Op::And));
    }

    [[nodiscard]] std::vector<std::size_t> select_or(const std::string& a,
                                                     const std::string& b) const {
        return to_records(combine(bitmap(a), bitmap(b), Op::Or));
    }

    [[nodiscard]] std::vector<std::size_t> select_not(const std::string& value) const {
        std::vector<std::uint64_t> bits = bitmap(value);
        for (auto& word : bits) {
            word = ~word;
            ++ops_;
        }
        mask_tail(bits);  // 最后一个字里超出记录数的那些位必须清掉
        return to_records(bits);
    }
// <<< bitmap-ops

    [[nodiscard]] std::size_t record_count() const noexcept { return records_; }
    [[nodiscard]] std::size_t distinct_values() const noexcept { return maps_.size(); }

    /// 空间代价：取值数 × 每条位串的字数。取值一多，位图就宽得离谱——所以要压缩。
    [[nodiscard]] std::size_t words() const noexcept {
        return maps_.size() * word_count();
    }

    [[nodiscard]] std::size_t word_ops() const noexcept { return ops_; }
    void reset_ops() const noexcept { ops_ = 0; }

private:
    enum class Op { And, Or };

    [[nodiscard]] std::size_t word_count() const noexcept {
        return (records_ + kBitsPerWord - 1) / kBitsPerWord;
    }

    [[nodiscard]] std::vector<std::uint64_t> combine(std::vector<std::uint64_t> left,
                                                     const std::vector<std::uint64_t>& right,
                                                     Op op) const {
        for (std::size_t i = 0; i < left.size(); ++i) {
            left[i] = op == Op::And ? (left[i] & right[i]) : (left[i] | right[i]);
            ++ops_;
        }
        return left;
    }

    void mask_tail(std::vector<std::uint64_t>& bits) const {
        const std::size_t used = records_ % kBitsPerWord;
        if (used != 0 && !bits.empty()) {
            bits.back() &= (std::uint64_t{1} << used) - 1;
        }
    }

    /// 把位串还原成记录号。不用 `__builtin_ctzll` 之类的编译器内建，
    /// 免得正文代码只在 GCC/Clang 上能编（D-001 第 1 条要求三大编译器都可用）。
    static std::vector<std::size_t> to_records(const std::vector<std::uint64_t>& bits) {
        std::vector<std::size_t> out;
        for (std::size_t word = 0; word < bits.size(); ++word) {
            for (std::size_t bit = 0; bit < kBitsPerWord; ++bit) {
                if ((bits[word] & (std::uint64_t{1} << bit)) != 0) {
                    out.push_back(word * kBitsPerWord + bit);
                }
            }
        }
        return out;
    }

    std::map<std::string, std::vector<std::uint64_t>> maps_;
    std::size_t records_ = 0;
    mutable std::size_t ops_ = 0;
};
// <<< bitmap-index

// >>> bitmap-compression
/// 字级游程压缩：把连续相同的机器字压成「重复次数 + 字值」两项。
/// 低基数列的位图大片全 0，这样能压得很小；随机稠密位图反而会变大——如实呈现，不粉饰。
inline std::vector<std::uint64_t> run_length_encode(const std::vector<std::uint64_t>& bits) {
    std::vector<std::uint64_t> out;
    std::size_t i = 0;
    while (i < bits.size()) {
        std::size_t run = 1;
        while (i + run < bits.size() && bits[i + run] == bits[i]) {
            ++run;
        }
        out.push_back(static_cast<std::uint64_t>(run));
        out.push_back(bits[i]);
        i += run;
    }
    return out;
}

inline std::vector<std::uint64_t> run_length_decode(const std::vector<std::uint64_t>& encoded) {
    if (encoded.size() % 2 != 0) {
        throw std::invalid_argument("encoded stream must be pairs");
    }
    std::vector<std::uint64_t> out;
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        out.insert(out.end(), static_cast<std::size_t>(encoded[i]), encoded[i + 1]);
    }
    return out;
}
// <<< bitmap-compression

// >>> signature-file
/// 签名文件：把一篇文档的词散列成一条较短的位串（各词位串按位或）。
/// 查询时先用签名粗筛掉不可能匹配的文档，再回原文确认。
///
/// 它**可能假阳性**（签名说可能有、原文里没有），但**绝不假阴性**——
/// 因为文档含某词时，该词的所有位一定已经或进了文档签名。所以它只能做粗筛，不能替代倒排。
class SignatureFile {
public:
    explicit SignatureFile(std::size_t bits_per_term = 2) : bits_per_term_(bits_per_term) {
        if (bits_per_term == 0 || bits_per_term > 8) {
            throw std::invalid_argument("bits_per_term out of range");
        }
    }

    void add(int doc_id, const std::vector<std::string>& terms) {
        docs_.push_back(doc_id);
        signatures_.push_back(signature_of(terms));
    }

    [[nodiscard]] std::uint64_t signature_of(const std::vector<std::string>& terms) const {
        std::uint64_t signature = 0;
        for (const auto& term : terms) {
            signature |= term_bits(term);
        }
        return signature;
    }

    /// 粗筛：文档签名必须**包含**查询签名的每一位。
    [[nodiscard]] std::vector<int> candidates(const std::vector<std::string>& query) const {
        const std::uint64_t wanted = signature_of(query);
        std::vector<int> out;
        for (std::size_t i = 0; i < docs_.size(); ++i) {
            if ((signatures_[i] & wanted) == wanted) {
                out.push_back(docs_[i]);
            }
        }
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return docs_.size(); }

private:
    [[nodiscard]] std::uint64_t term_bits(const std::string& term) const {
        // 固定的 FNV 变体：同一个词在任何一次运行里都散列到同样的位，测试才可复现。
        std::uint64_t hash = 1469598103934665603ULL;
        for (const char c : term) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;
        }
        std::uint64_t bits = 0;
        for (std::size_t i = 0; i < bits_per_term_; ++i) {
            bits |= std::uint64_t{1} << (hash % 64);
            hash /= 64;
        }
        return bits;
    }

    std::size_t bits_per_term_;
    std::vector<int> docs_;
    std::vector<std::uint64_t> signatures_;
};
// <<< signature-file

}  // namespace dsa::index
