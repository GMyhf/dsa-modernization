// 检索、集合与散列 —— 教学版。
// 原书【代码10.1】【算法10.2】–【算法10.13】。
//
// 一个文件、能直接编译运行，给「第一次读这一章」的人看。
//
//   sequential_search / binary_search   线性表上的两种检索
//   IntSet                              不重复整数集合与集合运算
//   elf_hash                            ELF 散列函数
//   HashTable                           线性探测的闭散列表，含「墓碑」删除
//
// 与 modern.hpp（工程版）的分工：教学版把 `[[nodiscard]]`/`noexcept` 和压成一行的
// if 分支全部展开，其余逻辑一模一样。这一章没有手写存储管理，所以没有三法则/五法则
// 之分——`std::vector` 在这里只是「一块连续的槽位」，不是被它替换掉的教学内容
// （见 unit.json 的 d001_exceptions）。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 10.1 基于线性表的检索
// ---------------------------------------------------------------------------

// 【算法10.2】顺序检索：从头比到尾。找到返回下标，没找到返回空 optional。
// 代价 O(n)。它对数据没有任何要求——这是它唯一的优点，也是全部优点。
inline std::optional<std::size_t> sequential_search(const std::vector<int>& values, int key) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == key) {
            return index;
        }
    }
    return std::nullopt;
}

// 【算法10.3】二分检索：要求**已排好序**，每比较一次砍掉一半，代价 O(log n)。
//
// 这里用的是**半开区间** [first, last)：`last` 指向"最后一个候选的下一个"。
// 为什么不用书上常见的闭区间 [low, high]？因为闭区间在没找到时要写 `high = mid - 1`，
// 而下标是无符号的，`mid == 0` 时这一句会下溢成一个天文数字。
// 半开区间只写 `last = middle`，永远不减 1，这个坑就不存在。
inline std::optional<std::size_t> binary_search(const std::vector<int>& sorted_values, int key) {
    std::size_t first = 0;
    std::size_t last = sorted_values.size();
    while (first < last) {
        // 写成 first + (last - first) / 2 而不是 (first + last) / 2：
        // 后者在两个下标都很大时会溢出。
        std::size_t middle = first + (last - first) / 2;
        if (sorted_values[middle] == key) {
            return middle;
        }
        if (sorted_values[middle] < key) {
            first = middle + 1;    // 目标在右半边
        } else {
            last = middle;         // 目标在左半边
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// 10.2 集合的检索
//
// 【代码10.4】【算法10.5】–【算法10.7】：用一个不含重复元素的表表示集合。
// 所有运算都建立在「查一个元素在不在集合里」之上，而这里的查是顺序检索 O(n)——
// 所以交集是 O(n·m)。10.3 节的散列就是来把这个 O(n) 压成 O(1) 的。
// ---------------------------------------------------------------------------
class IntSet {
public:
    // 插入。已经有了就返回 false——集合不含重复元素，这是可预期状态，不是错误。
    bool insert(int value) {
        if (contains(value)) {
            return false;
        }
        values_.push_back(value);
        return true;
    }

    bool erase(int value) {
        std::optional<std::size_t> found = sequential_search(values_, value);
        if (!found) {
            return false;
        }
        values_.erase(values_.begin() + static_cast<std::ptrdiff_t>(*found));
        return true;
    }

    bool contains(int value) const {
        return sequential_search(values_, value).has_value();
    }

    // 交集：本集合里凡是对方也有的，都收进结果。
    IntSet intersection(const IntSet& other) const {
        IntSet result;
        for (int value : values_) {
            if (other.contains(value)) {
                (void)result.insert(value);
            }
        }
        return result;
    }

    // 包含：对方的每个元素本集合都得有。
    bool includes(const IntSet& other) const {
        for (int value : other.values_) {
            if (!contains(value)) {
                return false;
            }
        }
        return true;
    }

    std::size_t size() const { return values_.size(); }

private:
    std::vector<int> values_;
};

// ---------------------------------------------------------------------------
// 10.3 散列方法
// ---------------------------------------------------------------------------

// 【算法10.8】ELF 散列：把字符串搅成一个整数。
//
// 逐字节读的是 `unsigned char` 而不是 `char`——`char` 在多数平台上是有符号的，
// 中文等非 ASCII 字节会变成负数，一进位运算就带出符号扩展，散列值随平台而变。
inline std::size_t elf_hash(const std::string& text) {
    std::size_t hash = 0;
    for (unsigned char character : text) {
        hash = (hash << 4U) + character;        // 左移 4 位，腾出位置放新字节
        std::size_t high_bits = hash & 0xF0000000U;   // 溢出到高 4 位的那部分
        if (high_bits != 0) {
            hash ^= high_bits >> 24U;           // 折回低位，别让它白白丢掉
        }
        hash &= ~high_bits;                     // 再把高 4 位清掉
    }
    return hash;
}

// 【算法10.9】–【算法10.13】线性探测的闭散列表。
//
// 「闭散列」是指所有元素都住在表里，不另开链表。key 先由散列函数算出一个
// **基地址**(home)；那一格被占了就往后一格一格找，这就是**线性探测**。
//
// 删除是这里唯一的难点。直接把槽位标成「空」是**错的**：
//
//   假设 A 和 B 的基地址都是 3，A 占了 3，B 探测一格住进 4。
//   现在删掉 A，把 3 标成空。再查 B：从 3 开始，看到「空」就认定 B 不在表里——
//   可 B 明明就在 4 号槽。**探测链被这个空格截断了。**
//
// 所以删除只能把槽位标成**墓碑**(tombstone)：查找路过它继续往下走，
// 插入则可以覆盖它。三种状态因此缺一不可。
class HashTable {
public:
    enum class SlotState { empty, used, tombstone };

    struct SlotView {
        int key;
        SlotState state;
    };

    explicit HashTable(std::size_t capacity) : slots_(capacity), size_(0) {
        if (capacity == 0) {
            throw std::invalid_argument("HashTable: 容量必须为正");
        }
    }

    // 插入。键已存在返回 false；表满且没有墓碑可用也返回 false。
    bool insert(int key) {
        std::optional<std::size_t> target = insertion_slot(key);
        if (!target) {
            return false;                       // 表满，插不进去
        }
        if (slots_[*target].state == SlotState::used) {
            return false;                       // 键已经在表里
        }
        slots_[*target].key = key;
        slots_[*target].state = SlotState::used;
        ++size_;
        return true;
    }

    bool contains(int key) const { return find_slot(key).has_value(); }

    // 删除：**标墓碑，不标空**。理由见上面类注释里那三行推演。
    bool erase(int key) {
        std::optional<std::size_t> found = find_slot(key);
        if (!found) {
            return false;
        }
        slots_[*found].state = SlotState::tombstone;
        --size_;
        return true;
    }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return slots_.size(); }

    // 让调用方（和测试）能看到每一格的状态，方便观察探测过程。
    SlotView slot_at(std::size_t index) const {
        if (index >= slots_.size()) {
            throw std::out_of_range("HashTable::slot_at: 下标越界");
        }
        return SlotView{slots_[index].key, slots_[index].state};
    }

private:
    struct Slot {
        int key = 0;
        SlotState state = SlotState::empty;
    };

    // 基地址：key 取绝对值再对表长取模。
    // 先转成 long long 再取绝对值，是因为 INT_MIN 的相反数在 int 里放不下。
    std::size_t home(int key) const {
        long long magnitude = (key >= 0) ? key : -static_cast<long long>(key);
        return static_cast<std::size_t>(magnitude) % slots_.size();
    }

    // 查找：从基地址起一格一格往后走。
    //   碰到「空」  → 探测链到头了，键不在表里；
    //   碰到「墓碑」→ 继续走（这正是墓碑存在的意义）；
    //   碰到「占用」且键相同 → 找到了。
    std::optional<std::size_t> find_slot(int key) const {
        for (std::size_t step = 0; step < slots_.size(); ++step) {
            std::size_t index = (home(key) + step) % slots_.size();
            if (slots_[index].state == SlotState::empty) {
                return std::nullopt;
            }
            if (slots_[index].state == SlotState::used && slots_[index].key == key) {
                return index;
            }
        }
        return std::nullopt;                    // 走遍全表也没有
    }

    // 找插入位置。比查找多做一件事：**记住路上第一个墓碑**。
    // 走到「空」时优先返回那个墓碑——回收墓碑，探测链才不会越来越长。
    // 但必须先走到「空」或找到同键才能停，否则会把一个已存在的键插第二遍。
    std::optional<std::size_t> insertion_slot(int key) const {
        std::optional<std::size_t> first_tombstone;
        for (std::size_t step = 0; step < slots_.size(); ++step) {
            std::size_t index = (home(key) + step) % slots_.size();
            if (slots_[index].state == SlotState::used && slots_[index].key == key) {
                return index;                   // 键已存在
            }
            if (slots_[index].state == SlotState::tombstone && !first_tombstone) {
                first_tombstone = index;
            }
            if (slots_[index].state == SlotState::empty) {
                return first_tombstone ? first_tombstone : std::optional<std::size_t>(index);
            }
        }
        return first_tombstone;                 // 全表没有空格，只能指望墓碑
    }

    std::vector<Slot> slots_;
    std::size_t size_;
};
