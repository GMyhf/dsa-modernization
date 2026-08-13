# 第10章 检索

检索的目标是在一组记录中找到目标键。顺序检索不要求有序；二分检索要求有序，靠每次排除一半区间加速；散列表用散列函数直接定位槽位，冲突时继续探测。

源码：[检索、集合和散列表](../code/ch10/search_hash/modern.hpp)、
[可运行示例](../code/ch10/search_hash/demo.cpp)、
[墓碑探测测试](../code/ch10/search_hash/test.cpp)。

## 10.1 先把题目说清楚

开放定址删除后**不能**立刻把槽位改成空，否则会截断后方元素的探测链。

假设表长为 5，键 1、6、11 的散列地址都为 1，于是它们依次放在槽 1、2、3：

```text
槽:  0    1    2    3    4
     空   1    6    11   空
```

删除键 1 后，若把槽 1 直接标为空，查找键 6 从槽 1 开始会误以为「从未插入过 6」并提前停止。墓碑表示「这里曾有元素，但探测链还要继续」。插入新键时可以复用第一个墓碑，但查重必须继续走到真正的空槽或找到同键为止。

![图 10.5 例 10.2 中关键码对应的散列表](assets/55bc014a79d6a905.jpg)

图 10.5 例 10.2 中关键码对应的散列表

散列表追求平均 O(1) 查找，前提是装载因子不过高且散列分布均匀；它不保持键的有序关系，不适合按范围遍历。

## 10.2 如何调用

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

## 10.3 再读实现

顺序检索从头扫到尾，没找到返回 `nullopt`。二分检索用半开区间 `[first, last)`，避免无符号下标上 `mid - 1` 下溢。

`HashTable::home` 先取绝对值再取模，所以负键也能进表。`find_slot` 遇到 `empty` 就停，遇到 `tombstone` 继续；`insertion_slot` 记下沿途第一个墓碑，确认后方没有同键后复用它。表满且全是墓碑时，插入仍可复用墓碑；真正一个空位都没有且键不在表中，才返回失败。

按键删除返回 `bool`：删到返回 `true`，键不存在返回 `false`。这是 D-001 §3c 的口径，不是逻辑错误。

## 10.4 现代实现

顺序与二分检索：

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

开放定址散列表：

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
