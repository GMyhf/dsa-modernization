#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace dsa::index {

// >>> inverted-index
/// 倒排索引：从**属性值**走到**记录集合**，和前面「从记录走到属性」的方向相反。
///
/// 每个词项挂一条按文档号升序的倒排表，所以「与」是有序表求交、「或」是求并、「非」是求差。
/// 交集用归并做，代价是 `O(n + m)`——这是本节的核心，所以这里手写归并，
/// 不调 `std::set_intersection`（D-001 第 2 条：别把要教的算法换成库调用）。
///
/// 倒排表里只存文档号和词在文档中的位置，完整记录仍在主文件里；短语查询靠位置相邻来过滤。
class InvertedIndex {
public:
    /// 建索引。文档号必须严格递增——倒排表按文档号有序是后面所有集合运算的前提。
    void add_document(int doc_id, const std::vector<std::string>& terms) {
        if (!documents_.empty() && doc_id <= documents_.back()) {
            throw std::invalid_argument("document ids must strictly increase");
        }
        documents_.push_back(doc_id);
        for (std::size_t position = 0; position < terms.size(); ++position) {
            Postings& postings = terms_[terms[position]];
            if (postings.docs.empty() || postings.docs.back() != doc_id) {
                postings.docs.push_back(doc_id);
                postings.positions.emplace_back();
            }
            postings.positions.back().push_back(static_cast<int>(position));
        }
    }

    [[nodiscard]] std::vector<int> postings(const std::string& term) const {
        const auto at = terms_.find(term);
        return at == terms_.end() ? std::vector<int>{} : at->second.docs;
    }

// >>> inverted-intersect
    /// 有序表求交。归并一遍，两个指针各走一趟，代价 O(n+m)。
    [[nodiscard]] static std::vector<int> intersect(const std::vector<int>& left,
                                                    const std::vector<int>& right) {
        std::vector<int> out;
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < left.size() && j < right.size()) {
            if (left[i] < right[j]) {
                ++i;
            } else if (right[j] < left[i]) {
                ++j;
            } else {
                out.push_back(left[i]);
                ++i;
                ++j;
            }
        }
        return out;
    }
// <<< inverted-intersect

    [[nodiscard]] static std::vector<int> unite(const std::vector<int>& left,
                                                const std::vector<int>& right) {
        std::vector<int> out;
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < left.size() || j < right.size()) {
            if (j >= right.size() || (i < left.size() && left[i] < right[j])) {
                out.push_back(left[i++]);
            } else if (i >= left.size() || right[j] < left[i]) {
                out.push_back(right[j++]);
            } else {
                out.push_back(left[i++]);
                ++j;
            }
        }
        return out;
    }

    [[nodiscard]] static std::vector<int> difference(const std::vector<int>& left,
                                                     const std::vector<int>& right) {
        std::vector<int> out;
        std::size_t j = 0;
        for (const int doc : left) {
            while (j < right.size() && right[j] < doc) {
                ++j;
            }
            if (j >= right.size() || right[j] != doc) {
                out.push_back(doc);
            }
        }
        return out;
    }

    /// 「与」查询：例如「计算机系且擅长英语」。
    [[nodiscard]] std::vector<int> and_query(const std::vector<std::string>& terms) const {
        if (terms.empty()) {
            return {};
        }
        std::vector<int> result = postings(terms.front());
        for (std::size_t i = 1; i < terms.size() && !result.empty(); ++i) {
            result = intersect(result, postings(terms[i]));
        }
        return result;
    }

    [[nodiscard]] std::vector<int> or_query(const std::vector<std::string>& terms) const {
        std::vector<int> result;
        for (const auto& term : terms) {
            result = unite(result, postings(term));
        }
        return result;
    }

    /// 「非」查询：全集减去该词项的倒排表。全集就是建过索引的所有文档。
    [[nodiscard]] std::vector<int> not_query(const std::string& term) const {
        return difference(documents_, postings(term));
    }

    /// 短语查询：先求交把候选文档缩小，再用位置是否相邻过滤。
    [[nodiscard]] std::vector<int> phrase_query(const std::vector<std::string>& phrase) const {
        if (phrase.empty()) {
            return {};
        }
        std::vector<int> candidates = and_query(phrase);
        std::vector<int> out;
        for (const int doc : candidates) {
            for (const int start : positions_of(phrase.front(), doc)) {
                bool adjacent = true;
                for (std::size_t step = 1; step < phrase.size() && adjacent; ++step) {
                    adjacent = has_position(phrase[step], doc, start + static_cast<int>(step));
                }
                if (adjacent) {
                    out.push_back(doc);
                    break;
                }
            }
        }
        return out;
    }

    [[nodiscard]] std::size_t term_count() const noexcept { return terms_.size(); }
    [[nodiscard]] std::size_t document_count() const noexcept { return documents_.size(); }

    /// 倒排表总长度：倒排的空间代价，也是「每次改记录都要同步维护」的工作量来源。
    [[nodiscard]] std::size_t postings_size() const noexcept {
        std::size_t total = 0;
        for (const auto& entry : terms_) {
            total += entry.second.docs.size();
        }
        return total;
    }

private:
    struct Postings {
        std::vector<int> docs;                    // 升序
        std::vector<std::vector<int>> positions;  // 与 docs 一一对应
    };

    [[nodiscard]] std::vector<int> positions_of(const std::string& term, int doc) const {
        const auto at = terms_.find(term);
        if (at == terms_.end()) {
            return {};
        }
        for (std::size_t i = 0; i < at->second.docs.size(); ++i) {
            if (at->second.docs[i] == doc) {
                return at->second.positions[i];
            }
        }
        return {};
    }

    [[nodiscard]] bool has_position(const std::string& term, int doc, int position) const {
        for (const int candidate : positions_of(term, doc)) {
            if (candidate == position) {
                return true;
            }
        }
        return false;
    }

    std::map<std::string, Postings> terms_;
    std::vector<int> documents_;  // 升序，充当全集
};
// <<< inverted-index

}  // namespace dsa::index
