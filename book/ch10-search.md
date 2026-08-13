# 第10章 检索

检索的目标是在一组记录中找到目标键。顺序检索不要求有序；二分检索要求有序，靠每次排除一半区间加速；散列表用散列函数直接定位槽位，冲突时继续探测。

源码：[检索、集合和散列表](../code/ch10/search_hash/modern.hpp)、
[可运行示例](../code/ch10/search_hash/demo.cpp)、
[墓碑探测测试](../code/ch10/search_hash/test.cpp)。

检索是在一组记录里定位关键码等于给定值的那一条，或属性满足条件的那些条。成功就是至少找到一条，失败就是没有。精确匹配查单个值，范围查询查一个区间。

平均检索长度 $\mathrm{ASL}=\sum P_i C_i$，其中 $P_i$ 是查到第 i 个元素的概率，$C_i$ 是比较次数。衡量算法还要看额外空间和实现复杂度。

可以把检索分成四类：基于线性表、按关键码直接访问（含散列）、树形索引、基于属性（倒排）。本章做前两类；树形索引见第 5、11、12 章，倒排见第 11 章。

## 10.1 基于线性表的检索

数据放在数组或链表里，按给定值 $K$ 与表中元素比较，直到命中或能确定不在表中。这一节不要求散列函数，也不建树，只靠线性结构本身。

### 10.1.1 顺序检索

从表头逐个比到表尾。表可以无序，实现最简单，链表和数组都能用。最好情况目标在第一个位置，比较 1 次；最坏情况目标不在表里，比较 $n$ 次；等概率时平均检索长度 $\mathrm{ASL}=(n+1)/2$。有时在表尾放一个与 $K$ 相等的「监视哨」，循环里就不必每次都判断下标有没有出界，比较次数的常数会小一点，阶不变。

### 10.1.2 二分检索

表必须按关键码有序，并且能按下标随机访问，所以适合数组，不适合链表。每次取当前区间的中点：相等则停；目标更大则丢掉左半，否则丢掉右半。区间长度每次至少减半，比较次数是 $O(\log n)$。

实现时用半开区间 `[first, last)`：中点是 `first + (last - first) / 2`，走左半时 `last = middle`，走右半时 `first = middle + 1`。这样不必写 `middle - 1`，无符号下标不会在 `middle == 0` 时下溢。循环条件是 `first < last`；区间空了还没找到，就返回空。

### 10.1.3 分块检索

把 $n$ 个元素分成 $b$ 块。块与块之间有序（后一块的最小关键码不小于前一块的最大关键码），块内部可以无序。另造一张块索引，每项记下该块的最大（或最小）关键码和块的起始位置。检索时先在索引上顺序或二分，确定目标可能在哪一块，再在那一块里顺序查。

块数选 $\sqrt{n}$ 量级时，索引和块内的平均比较次数比较均衡，ASL 大约是 $O(\sqrt{n})$，介于顺序的 $O(n)$ 和二分的 $O(\log n)$ 之间。它适合「主表很大、不能整表排序，但可以按块组织」的场合。本章不另写未验证的分块实现。

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

检索也可以用来实现集合。集合只关心一个元素在不在里面，不保留重复，也不保证顺序。交、并、差、包含都可以建立在「是否属于」之上。

### 10.2.1 集合的数学特性

集合的元素互异：同一个值不能出现两次。「$x$ 属于 $S$」是一个命题，不是一个位置。所以插入一个已经在里面的键、删除一个不在里面的键，都是正常、可预期的失败，应当返回「没做成」，而不是抛异常或打印一行。空集上的任何包含询问都是假。

### 10.2.2 计算机中的集合

同一组集合运算，底下可以用不同结构：线性表加顺序检索（实现简单，适合很小的集合）、有序表加二分、二叉搜索树、散列表。规模上去以后，线性表的 $O(n)$ 检索会拖垮交和并。本章的 `IntSet` 故意用线性表，把接口先钉死：`insert` / `erase` 返回 `bool`，`contains` 只回答在不在，`intersection` 和 `includes` 用检索组合出来。换成散列表时，调用方不用改。

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

前面的检索都要和表里的元素比较，至少看 $\log n$ 或 $\sqrt{n}$ 个。散列换一条路：用函数从关键码直接算出槽位下标，期望一次就能到。它不保持键的次序，不能按范围遍历；装载因子过高或散列不均匀时，冲突变多，会退化成接近线性。

### 10.3.1 散列函数

散列函数 $h$ 把关键码映到 $\{0,1,\ldots,m-1\}$。它应当算得快，并且尽量把地址铺匀，否则很多键挤在同一段，再好的冲突处理也救不了。常见做法有：除留余数（$h(k)=k \bmod m$，$m$ 最好是素数）、折叠、平方取中、以及按字节处理的字符串散列。

本章实现了 ELFHash：每个字节先左移 4 位再加进去，高 4 位若非零就折回到低位并清掉。字节按 `unsigned char` 读，避免 `char` 在有的平台上是有符号的、高位 1 被当成负数。它不是密码学哈希，只求快和够匀。

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

开散列不在表内挤位置：每个槽挂一条链表（或动态数组），散列到同一地址的关键码都串在这条链上。插入是算地址再接到链头或链尾；查找、删除只在那一条链上走。删掉一个结点不会影响别的链，也不存在「探测序列被截断」的问题。

装载因子 $\alpha=n/m$ 可以大于 1，链只是变长。成功查找的期望比较次数大约是 $1+\alpha/2$。实现简单，空间上每个结点多一个指针。本章主实现是闭散列，开散列不另写一份。

### 10.3.3 闭散列方法（开地址法）

闭散列把冲突消化在表内：回家地址被占了，就按某种规则探测下一个空槽。线性探测是最简单的一种——第 $i$ 次看 $(h(k)+i)\bmod m$。二次探测、双重散列是为了减轻「挤成一团」的一次聚集。

表满了就插不进去，所以 $\alpha$ 必须小于 1，实务上常在 0.5 到 0.7 之间扩表。删除是闭散列的难点：不能把槽直接标成空。否则沿这条探测链查找后面的键时，会在空槽处误判「从来没插入过」而提前停止。

假设表长为 5，键 1、6、11 的散列地址都是 1，它们依次放在槽 1、2、3。删除 1 后若把槽 1 标空，再查 6 就会从槽 1 出发立刻停。正确做法是留下墓碑：查找时墓碑当作「继续往前」；插入时可以复用沿途第一个墓碑，但必须先走完、确认后面没有同一个键。


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

