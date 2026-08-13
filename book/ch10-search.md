# 第10章 检索

检索的目标是在一组记录中找到目标键。顺序检索不要求有序；二分检索要求有序，靠每次排除一半区间加速；散列表用散列函数直接定位槽位，冲突时继续探测。

源码：[检索、集合和散列表](../code/ch10/search_hash/modern.hpp)、
[可运行示例](../code/ch10/search_hash/demo.cpp)、
[墓碑探测测试](../code/ch10/search_hash/test.cpp)。

检索是在一组记录里定位关键码等于给定值的那一条，或属性满足条件的那些条。成功就是至少找到一条，失败就是没有。精确匹配查单个值，范围查询查一个区间。

平均检索长度 $\mathrm{ASL}=\sum P_i C_i$，其中 $P_i$ 是查到第 i 个元素的概率，$C_i$ 是比较次数。衡量算法还要看额外空间和实现复杂度。

可以把检索分成四类：基于线性表、按关键码直接访问（含散列）、树形索引、基于属性（倒排）。本章做前两类；树形索引见第 5、11、12 章，倒排见第 11 章。

## 10.1 基于线性表的检索

数据放在数组或链表里，按给定值 K 比较，直到命中或确定不在表中。

### 10.1.1 顺序检索

从表头逐个比到表尾。元素可以无序。最好 1 次，最坏 n 次，等概率平均 $(n+1)/2$。

### 10.1.2 二分检索

表必须有序。每次取中点，相等则停，否则丢掉一半。半开区间 `[first, last)` 避免无符号下标上 `mid - 1` 下溢。比较次数是 $O(\log n)$，不适合链表。

### 10.1.3 分块检索

把表分成若干块，块间有序、块内可以无序。先在块的索引上定位，再在块内顺序查。是顺序与二分之间的折中。本章不另写未验证的分块实现。

```cpp file=code/ch10/search_hash/modern.hpp#sequential-binary
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
```

## 10.2 集合的检索

集合只关心「在不在」，不保留重复，也不保证顺序。交、并、差都可以建立在检索之上。

### 10.2.1 集合的数学特性

元素互异；属于关系是命题，不是位置。因此插入已存在的键、删除不存在的键都是可预期的失败，返回 `false`。

### 10.2.2 计算机中的集合

可以用线性表、二叉搜索树或散列表实现。本章的 `IntSet` 用线性表加顺序检索，把接口钉死；大规模时应换成后面的散列表。

```cpp file=code/ch10/search_hash/modern.hpp#int-set
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
```

## 10.3 散列方法

散列用函数把关键码映射成槽位下标，期望平均 O(1)。它不保持有序，不适合范围查询。装载因子过高或散列不均匀时会退化。

### 10.3.1 散列函数

好的散列函数计算要简单，地址要均匀。本章实现了 ELFHash：逐字节移位异或，并用无符号字符，避免 `char` 的符号性进入计算。

```cpp file=code/ch10/search_hash/modern.hpp#elf-hash
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
```

### 10.3.2 开散列方法（拉链法）

每个槽挂一条链表，冲突的关键码串在同一条链上。删除简单，不会截断别的链。装载因子可以大于 1。本章主实现是闭散列，开散列不另写一份。

### 10.3.3 闭散列方法（开地址法）

冲突时在表内继续探测。线性探测就是「回家地址 + i」。删除后**不能**立刻把槽位改成空，否则会截断后方元素的探测链。


假设表长为 5，键 1、6、11 的散列地址都为 1，于是它们依次放在槽 1、2、3：

```text
槽:  0    1    2    3    4
     空   1    6    11   空
```

删除键 1 后，若把槽 1 直接标为空，查找键 6 从槽 1 开始会误以为「从未插入过 6」并提前停止。墓碑表示「这里曾有元素，但探测链还要继续」。插入新键时可以复用第一个墓碑，但查重必须继续走到真正的空槽或找到同键为止。

图 10.5 就是上面这张槽位表。

散列表追求平均 O(1) 查找，前提是装载因子不过高且散列分布均匀；它不保持键的有序关系，不适合按范围遍历。

### 10.3.4 闭散列表的算法

先跑一遍：


```cpp file=code/ch10/search_hash/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::search::HashTable table(5);
    if (!table.insert(1) || !table.insert(6) || !table.insert(11)) {
        std::cout << "插入 1、6、11 失败\n";
        return 1;
    }

    std::cout << "插入 1、6、11 后:\n";
    for (std::size_t index = 0; index < table.capacity(); ++index) {
        const auto slot = table.slot_at(index);
        std::cout << "  槽 " << index << ": ";
        if (slot.state == dsa::search::HashTable::SlotState::used) {
            std::cout << slot.key << '\n';
        } else if (slot.state == dsa::search::HashTable::SlotState::tombstone) {
            std::cout << "墓碑\n";
        } else {
            std::cout << "空\n";
        }
    }

    if (!table.erase(1)) {
        std::cout << "删除 1 失败\n";
        return 1;
    }
    std::cout << "删除 1 后仍能找到 6? " << (table.contains(6) ? "是" : "否") << '\n';
    if (!table.insert(16)) {
        std::cout << "插入 16 失败\n";
        return 1;
    }
    std::cout << "插入 16 后槽 1 是 "
              << table.slot_at(1).key
              << "（复用了墓碑）\n";
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch10/search_hash \
    code/ch10/search_hash/demo.cpp -o /tmp/hash-demo
/tmp/hash-demo
```

```console
插入 1、6、11 后:
  槽 0: 空
  槽 1: 1
  槽 2: 6
  槽 3: 11
  槽 4: 空
删除 1 后仍能找到 6? 是
插入 16 后槽 1 是 16（复用了墓碑）
```

若把删除写成「标空」而不是「标墓碑」，第二行会变成「否」——测试里有两条具名断言守住这件事。

`HashTable::home` 先取绝对值再取模，所以负键也能进表。`find_slot` 遇到 `empty` 就停，遇到 `tombstone` 继续；`insertion_slot` 记下沿途第一个墓碑，确认后方没有同键后复用它。按键删除返回 `bool`。

```cpp file=code/ch10/search_hash/modern.hpp#hash-table
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
```

### 10.3.5 散列方法的效率分析

成功检索的期望探测次数随装载因子 α 上升。线性探测大约是 $(1+1/(1-\alpha))/2$；拉链法是 $1+\alpha/2$。α 接近 1 时闭散列急剧变差，应扩表或改开散列。

### 10.3.6 散列方法的应用

编译器符号表、缓存、去重都常用散列。需要范围查询或有序遍历时，应改用树或有序表。

