#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dsa::advanced {

// >>> fit-strategies
/// 从空闲区里挑哪一块，原书 12.2.3 给了三种经典策略：
///
/// | 策略 | 挑哪块 | 后果 |
/// | --- | --- | --- |
/// | 首次适应 First | 从头扫，第一块够大的就用 | 最快；小碎片会在低地址堆积 |
/// | 最佳适应 Best | 刚好够用的**最小**块 | 省下的大块留着；剩下的碎片最碎 |
/// | 最坏适应 Worst | 当前**最大**的块 | 剩下的还大到能再用；大块很快被拆光 |
///
/// 三者只在「挑哪一块」这一步不同，分裂与合并完全一样。判断实现有没有写对，
/// 唯一的办法是造一组**大小不同的空闲块**，看三种策略是不是挑了不同的那一块——
/// 只在一块空闲区上测，三种策略的结果必然相同，那样的测试等于没写。
enum class Fit { First, Best, Worst };
// <<< fit-strategies

// >>> boundary-allocator
/// 一段连续空间的分配器。块表按**偏移升序**排列，所以「相邻块」就是表里相邻的项，
/// 释放时左右各看一眼就能合并。
///
/// 原书讲的**边界标记**（块头块尾各记一份大小与忙闲）解决的是同一件事：
/// 让一个块能在 $O(1)$ 内找到物理相邻的邻居。这里用有序块表表达同一个想法，
/// 代价是定位要二分、插入删除要挪动表项；换来的是不必手工维护前后指针，
/// 也就不会出现「合并之后忘了归还元数据槽位」这类错误。
class BoundaryAllocator {
public:
    explicit BoundaryAllocator(std::size_t bytes) : capacity_(bytes) {
        if (bytes == 0) {
            throw std::invalid_argument("BoundaryAllocator: capacity must be positive");
        }
        blocks_.push_back(Block{0, bytes, true});
    }

    /// 按给定策略分配。空间不足返回 `nullopt`——这是预期结果，不是错误。
    [[nodiscard]] std::optional<std::size_t> allocate(std::size_t bytes, Fit fit) {
        if (bytes == 0) {
            throw std::invalid_argument("BoundaryAllocator: size must be positive");
        }
        const std::optional<std::size_t> chosen = select(bytes, fit);
        if (!chosen) {
            return std::nullopt;
        }
        Block& block = blocks_[*chosen];
        if (block.size > bytes) {
            // 分裂：剩下的那截仍然空闲，作为下一项插进块表。
            const Block rest{block.offset + bytes, block.size - bytes, true};
            block.size = bytes;
            blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(*chosen) + 1, rest);
        }
        blocks_[*chosen].free = false;
        return blocks_[*chosen].offset;
    }

    /// 归还。返回 false 表示这个偏移上没有已分配的块——重复释放会走到这里。
    bool release(std::size_t offset) {
        const std::optional<std::size_t> index = index_of(offset);
        if (!index || blocks_[*index].free) {
            return false;
        }
        blocks_[*index].free = true;
        coalesce(*index);
        return true;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t free_bytes() const noexcept {
        std::size_t total = 0;
        for (const Block& block : blocks_) {
            if (block.free) {
                total += block.size;
            }
        }
        return total;
    }

    /// 空闲块的个数。它和 `free_bytes()` 一起才说明问题：
    /// 空闲字节很多、但每块都很小，就是**外部碎片**。
    [[nodiscard]] std::size_t free_block_count() const noexcept {
        std::size_t count = 0;
        for (const Block& block : blocks_) {
            if (block.free) {
                ++count;
            }
        }
        return count;
    }

    /// 最大的那块空闲区。装不下一个请求时，看的是它而不是空闲总量。
    [[nodiscard]] std::size_t largest_free_block() const noexcept {
        std::size_t largest = 0;
        for (const Block& block : blocks_) {
            if (block.free && block.size > largest) {
                largest = block.size;
            }
        }
        return largest;
    }

    /// 当前的空闲块布局 `(偏移, 大小)`，按偏移升序。测试靠它断言分裂与合并的结果。
    [[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> free_blocks() const {
        std::vector<std::pair<std::size_t, std::size_t>> out;
        for (const Block& block : blocks_) {
            if (block.free) {
                out.emplace_back(block.offset, block.size);
            }
        }
        return out;
    }

    [[nodiscard]] std::size_t block_count() const noexcept { return blocks_.size(); }

    /// 上一次分配扫过多少个块。首次适应通常最小——这是它「快」的全部含义。
    [[nodiscard]] std::size_t last_scan_steps() const noexcept { return scanned_; }

private:
    struct Block {
        std::size_t offset;
        std::size_t size;
        bool free;
    };

    /// 三种策略的唯一分歧点。
    [[nodiscard]] std::optional<std::size_t> select(std::size_t bytes, Fit fit) {
        scanned_ = 0;
        std::optional<std::size_t> chosen;
        for (std::size_t i = 0; i < blocks_.size(); ++i) {
            ++scanned_;
            if (!blocks_[i].free || blocks_[i].size < bytes) {
                continue;
            }
            if (fit == Fit::First) {
                return i;  // 第一块够大的就走，不再往后看
            }
            if (!chosen) {
                chosen = i;
            } else if (fit == Fit::Best && blocks_[i].size < blocks_[*chosen].size) {
                chosen = i;
            } else if (fit == Fit::Worst && blocks_[i].size > blocks_[*chosen].size) {
                chosen = i;
            }
        }
        return chosen;
    }

    [[nodiscard]] std::optional<std::size_t> index_of(std::size_t offset) const {
        for (std::size_t i = 0; i < blocks_.size(); ++i) {
            if (blocks_[i].offset == offset) {
                return i;
            }
        }
        return std::nullopt;
    }

    /// 与左右两侧的空闲块合并。**先合右再合左**：先处理右边，左边的下标才不会失效。
    /// 不合并的话，外部碎片只会越积越多——这是本节的正题。
    void coalesce(std::size_t index) {
        if (index + 1 < blocks_.size() && blocks_[index + 1].free) {
            blocks_[index].size += blocks_[index + 1].size;
            blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index) + 1);
        }
        if (index > 0 && blocks_[index - 1].free) {
            blocks_[index - 1].size += blocks_[index].size;
            blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    std::vector<Block> blocks_;  // 按偏移升序，覆盖整段空间，无空洞
    std::size_t capacity_;
    std::size_t scanned_ = 0;
};
// <<< boundary-allocator

}  // namespace dsa::advanced
