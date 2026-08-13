# 第10章 检索与散列

## 本章先读什么

检索的目标是在一组记录中找到目标键。顺序检索不要求有序；二分检索要求有序，靠每次排除一半
区间加速；散列表用散列函数直接定位槽位，冲突时继续探测。开放定址删除后不能立刻把槽位改成
空，否则会截断后方元素的探测链，因此使用墓碑标记。

源码入口：[检索、集合和散列表](../code/ch10/search_hash/modern.hpp)、
[墓碑探测测试](../code/ch10/search_hash/test.cpp)。运行：
`python3 tools/check_code.py --allow-degraded code/ch10/search_hash`。

### 为什么删除要留下墓碑

假设表长为 5，键 1、6、11 的散列地址都为 1，于是它们依次放在槽 1、2、3。删除键 1 后，
若把槽 1 直接标为空，查找键 6 从槽 1 开始会误以为“从未插入过 6”并提前停止。墓碑表示
“这里曾有元素，但探测链还要继续”；插入新键时可以复用第一个墓碑，但查重必须继续走到真正的
空槽或找到同键为止。

散列表追求平均 O(1) 查找，前提是装载因子不过高且散列分布均匀；它不保持键的有序关系，
不适合按范围遍历。

顺序/二分检索、集合、ELFHash 和开放定址散列表均以显式返回状态处理未找到与墓碑删除。

```cpp file=code/ch10/search_hash/modern.hpp
// 原书【代码10.1】【算法10.2】至【算法10.13】的检索、集合和散列实现。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dsa::search {

// >>> search-hash

template <typename Key>
class Item {
public:
    explicit Item(Key key) : key_(std::move(key)) {}
    [[nodiscard]] const Key& key() const noexcept { return key_; }
    void set_key(Key key) { key_ = std::move(key); }
private:
    Key key_;
};

// 算法10.2：无需修改输入容器的顺序检索。
inline std::optional<std::size_t> sequential_search(const std::vector<int>& values, int key) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == key) return index;
    }
    return std::nullopt;
}

// 算法10.3：半开区间避免 `mid - 1` 在无符号下标下溢。
inline std::optional<std::size_t> binary_search(const std::vector<int>& sorted_values, int key) {
    std::size_t first = 0;
    std::size_t last = sorted_values.size();
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2;
        if (sorted_values[middle] == key) return middle;
        if (sorted_values[middle] < key) first = middle + 1;
        else last = middle;
    }
    return std::nullopt;
}

// 代码10.4、算法10.5–10.7：不重复整数集合。
class IntSet {
public:
    [[nodiscard]] bool insert(int value) {
        if (contains(value)) return false;
        values_.push_back(value);
        return true;
    }
    [[nodiscard]] bool erase(int value) {
        const auto found = sequential_search(values_, value);
        if (!found) return false;
        values_.erase(values_.begin() + static_cast<std::ptrdiff_t>(*found));
        return true;
    }
    [[nodiscard]] bool contains(int value) const { return sequential_search(values_, value).has_value(); }
    [[nodiscard]] IntSet intersection(const IntSet& other) const {
        IntSet result;
        for (int value : values_) if (other.contains(value)) (void)result.insert(value);
        return result;
    }
    [[nodiscard]] bool includes(const IntSet& other) const {
        for (int value : other.values_) if (!contains(value)) return false;
        return true;
    }
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
private:
    std::vector<int> values_;
};

// 算法10.8：ELFhash，逐字节处理，不把 char 的符号性带入散列。
inline std::size_t elf_hash(const std::string& text) noexcept {
    std::size_t hash = 0;
    for (unsigned char character : text) {
        hash = (hash << 4U) + character;
        const std::size_t high_bits = hash & 0xF0000000U;
        if (high_bits != 0) hash ^= high_bits >> 24U;
        hash &= ~high_bits;
    }
    return hash;
}

// 算法10.9–10.13：线性探测闭散列表，显式区分空、占用和墓碑。
class HashTable {
public:
    enum class SlotState { empty, used, tombstone };
    struct SlotView { int key; SlotState state; };

    explicit HashTable(std::size_t capacity) : slots_(capacity) {
        if (capacity == 0) throw std::invalid_argument("hash table capacity must be positive");
    }

    [[nodiscard]] bool insert(int key) {
        const auto target = insertion_slot(key);
        if (!target) return false;
        Slot& slot = slots_[*target];
        if (slot.state == SlotState::used) return false;
        slot = Slot{key, SlotState::used};
        ++size_;
        return true;
    }
    [[nodiscard]] bool contains(int key) const { return find_slot(key).has_value(); }
    [[nodiscard]] bool erase(int key) {
        const auto found = find_slot(key);
        if (!found) return false;
        slots_[*found].state = SlotState::tombstone;
        --size_;
        return true;
    }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
    [[nodiscard]] SlotView slot_at(std::size_t index) const {
        if (index >= slots_.size()) throw std::out_of_range("hash table slot");
        return SlotView{slots_[index].key, slots_[index].state};
    }

private:
    struct Slot { int key{0}; SlotState state{SlotState::empty}; };
    [[nodiscard]] std::size_t home(int key) const noexcept {
        const auto magnitude = key >= 0 ? static_cast<long long>(key) : -static_cast<long long>(key);
        return static_cast<std::size_t>(magnitude) % slots_.size();
    }
    [[nodiscard]] std::optional<std::size_t> find_slot(int key) const {
        for (std::size_t step = 0; step < slots_.size(); ++step) {
            const std::size_t index = (home(key) + step) % slots_.size();
            const Slot& slot = slots_[index];
            if (slot.state == SlotState::empty) return std::nullopt;
            if (slot.state == SlotState::used && slot.key == key) return index;
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> insertion_slot(int key) const {
        std::optional<std::size_t> first_tombstone;
        for (std::size_t step = 0; step < slots_.size(); ++step) {
            const std::size_t index = (home(key) + step) % slots_.size();
            const Slot& slot = slots_[index];
            if (slot.state == SlotState::used && slot.key == key) return index;
            if (slot.state == SlotState::tombstone && !first_tombstone) first_tombstone = index;
            if (slot.state == SlotState::empty) return first_tombstone ? first_tombstone : index;
        }
        return first_tombstone;
    }
    std::vector<Slot> slots_;
    std::size_t size_{0};
};

// <<< search-hash

}  // namespace dsa::search
```
