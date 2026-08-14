#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dsa::index {

// >>> linear-index
/// 线性索引：项是 `(key, 记录位置)`，按 key 排序。分两种：
///
/// - **稠密索引**每条记录一项，所以主文件**可以无序**；查不到时连数据页都不用读。
/// - **稀疏索引**每个数据页一项（记下该页最小 key），更省空间，但要求主文件**按 key 有序**，
///   而且查不到也得先把那一页读上来才知道。
///
/// 索引项太多、一页放不下时，就为索引再建一层，成为多级索引。查询路径是
/// 「内存里的顶层 → 逐层读索引页 → 读数据页」，所以关键指标是**页访问次数**，
/// 不是 CPU 比较次数。顶层常驻内存，不计入 `page_reads()`。
enum class IndexKind { Dense, Sparse };

class MultiLevelIndex {
public:
    MultiLevelIndex(IndexKind kind, std::size_t records_per_page, std::size_t entries_per_page)
        : kind_(kind), records_per_page_(records_per_page), entries_per_page_(entries_per_page) {
        if (records_per_page < 1 || entries_per_page < 2) {
            // 索引页至少要能放两项，否则「多级」永远收敛不到一页。
            throw std::invalid_argument("page capacity too small");
        }
    }

    /// 装入主文件。稀疏索引要求按 key 升序；稠密索引不要求，但 key 不能重复。
    void load(std::vector<std::pair<int, std::string>> records) {
        records_ = std::move(records);
        levels_.clear();

        std::vector<Entry> bottom;
        if (kind_ == IndexKind::Sparse) {
            for (std::size_t i = 1; i < records_.size(); ++i) {
                if (records_[i - 1].first >= records_[i].first) {
                    throw std::invalid_argument("sparse index needs a sorted main file");
                }
            }
            // 每个数据页一项，记下该页的最小 key。
            for (std::size_t i = 0; i < records_.size(); i += records_per_page_) {
                bottom.push_back(Entry{records_[i].first, i / records_per_page_});
            }
        } else {
            // 每条记录一项，指向它所在的数据页；索引自己排序，主文件保持原样。
            for (std::size_t i = 0; i < records_.size(); ++i) {
                bottom.push_back(Entry{records_[i].first, i});
            }
            std::sort(bottom.begin(), bottom.end(),
                      [](const Entry& a, const Entry& b) { return a.key < b.key; });
            for (std::size_t i = 1; i < bottom.size(); ++i) {
                if (bottom[i - 1].key == bottom[i].key) {
                    throw std::invalid_argument("duplicate key");
                }
            }
        }
        if (bottom.empty()) {
            return;
        }
        levels_.push_back(std::move(bottom));

        // 一层放不下就再建一层，直到顶层只剩一页。
        while (levels_.back().size() > entries_per_page_) {
            const std::vector<Entry>& lower = levels_.back();
            std::vector<Entry> upper;
            for (std::size_t i = 0; i < lower.size(); i += entries_per_page_) {
                upper.push_back(Entry{lower[i].key, i / entries_per_page_});
            }
            levels_.push_back(std::move(upper));
        }
    }

// >>> index-find
    [[nodiscard]] std::optional<std::string> find(int key) const {
        if (levels_.empty()) {
            return std::nullopt;
        }
        // 顶层常驻内存：定位到它所在的那一页，不计页访问。
        std::size_t page = locate(levels_.back(), 0, levels_.back().size(), key);
        for (std::size_t level = levels_.size() - 1; level > 0; --level) {
            page = levels_[level][page].target;
            ++reads_;  // 读一个下层索引页
            const std::size_t first = page * entries_per_page_;
            const std::size_t last = std::min(first + entries_per_page_, levels_[level - 1].size());
            if (first >= last) {
                return std::nullopt;
            }
            page = locate(levels_[level - 1], first, last, key);
        }

        const Entry& entry = levels_[0][page];
        if (kind_ == IndexKind::Dense) {
            // 稠密索引：索引里没有就是真没有，数据页一次都不用读。
            if (entry.key != key) {
                return std::nullopt;
            }
            ++reads_;
            return records_[entry.target].second;
        }
        // 稀疏索引：只能定位到页，页内还要再找一次；不命中也已经付出了这一页。
        ++reads_;
        const std::size_t first = entry.target * records_per_page_;
        const std::size_t last = std::min(first + records_per_page_, records_.size());
        for (std::size_t i = first; i < last; ++i) {
            if (records_[i].first == key) {
                return records_[i].second;
            }
        }
        return std::nullopt;
    }
// <<< index-find

    /// 索引层数（含常驻内存的顶层）。
    [[nodiscard]] std::size_t levels() const noexcept { return levels_.size(); }

    [[nodiscard]] std::size_t entries() const noexcept {
        return levels_.empty() ? 0 : levels_.front().size();
    }

    [[nodiscard]] std::size_t index_pages() const noexcept {
        std::size_t pages = 0;
        for (const auto& level : levels_) {
            pages += (level.size() + entries_per_page_ - 1) / entries_per_page_;
        }
        return pages;
    }

    [[nodiscard]] std::size_t data_pages() const noexcept {
        return (records_.size() + records_per_page_ - 1) / records_per_page_;
    }

    [[nodiscard]] std::size_t page_reads() const noexcept { return reads_; }
    void reset_counters() const noexcept { reads_ = 0; }

private:
    struct Entry {
        int key = 0;
        std::size_t target = 0;  // 下层页号，或（稠密索引底层）记录下标
    };

    /// 在 [first, last) 里找最后一个 key <= 目标的项，返回它的下标。
    static std::size_t locate(const std::vector<Entry>& level, std::size_t first,
                              std::size_t last, int key) {
        std::size_t slot = first;
        for (std::size_t i = first; i < last; ++i) {
            if (level[i].key <= key) {
                slot = i;
            } else {
                break;
            }
        }
        return slot;
    }

    IndexKind kind_;
    std::size_t records_per_page_;
    std::size_t entries_per_page_;
    std::vector<std::pair<int, std::string>> records_;
    std::vector<std::vector<Entry>> levels_;  // levels_[0] 是底层，back() 是顶层
    mutable std::size_t reads_ = 0;
};
// <<< linear-index

}  // namespace dsa::index
