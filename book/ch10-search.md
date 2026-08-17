# 第10章 检索

检索的目标是在一组记录中找到目标键。顺序检索不要求有序；二分检索要求有序，靠每次排除一半区间加速；散列表用散列函数直接定位槽位，冲突时继续探测。

源码：[检索、集合和散列表·教学版](../code/ch10/search_hash/teaching.hpp)、
[检索、集合和散列表·工程版](../code/ch10/search_hash/modern.hpp)、
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

### 教学版：完整实现

本章的四件东西——两种检索、集合、散列函数、闭散列表——放在同一个文件里。
后面 10.2、10.3 各节就是把它拆开逐段讲。

```cpp file=code/ch10/search_hash/teaching.hpp
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
```


## 10.2 集合的检索

检索也可以用来实现集合。集合只关心一个元素在不在里面，不保留重复，也不保证顺序。交、并、差、包含都可以建立在「是否属于」之上。

### 10.2.1 集合的数学特性

集合的元素互异：同一个值不能出现两次。「$x$ 属于 $S$」是一个命题，不是一个位置。所以插入一个已经在里面的键、删除一个不在里面的键，都是正常、可预期的失败，应当返回「没做成」，而不是抛异常或打印一行。空集上的任何包含询问都是假。

### 10.2.2 计算机中的集合

同一组集合运算，底下可以用不同结构：线性表加顺序检索（实现简单，适合很小的集合）、有序表加二分、二叉搜索树、散列表。规模上去以后，线性表的 $O(n)$ 检索会拖垮交和并。本章的 `IntSet` 故意用线性表，把接口先钉死：`insert` / `erase` 返回 `bool`，`contains` 只回答在不在，`intersection` 和 `includes` 用检索组合出来。换成散列表时，调用方不用改。

`IntSet` 的实现见上面那份清单。注意它的每一个运算最后都落到 `contains`，
而 `contains` 是顺序检索 O(n)——所以交集是 O(n·m)。**10.3 节的散列就是来把这个
O(n) 压成 O(1) 的**，接口一个字都不用改。


## 10.3 散列方法

前面的检索都要和表里的元素比较，至少看 $\log n$ 或 $\sqrt{n}$ 个。散列换一条路：用函数从关键码直接算出槽位下标，期望一次就能到。它不保持键的次序，不能按范围遍历；装载因子过高或散列不均匀时，冲突变多，会退化成接近线性。

### 10.3.1 散列函数

散列函数 $h$ 把关键码映到 $\{0,1,\ldots,m-1\}$。它应当算得快，并且尽量把地址铺匀，否则很多键挤在同一段，再好的冲突处理也救不了。常见做法有：除留余数（$h(k)=k \bmod m$，$m$ 最好是素数）、折叠、平方取中、以及按字节处理的字符串散列。

本章实现了 ELFHash：每个字节先左移 4 位再加进去，高 4 位若非零就折回到低位并清掉。字节按 `unsigned char` 读，避免 `char` 在有的平台上是有符号的、高位 1 被当成负数。它不是密码学哈希，只求快和够匀。

`elf_hash` 的实现见上面那份清单。它逐字节读的是 `unsigned char`——
教学版的测试里有一条拿 UTF-8 的「中」（E4 B8 AD）做输入：按无符号读得到
`0xF02D`，按有符号读会被符号扩展成 `0xFFFFFF000FFF00DD`。两者天差地别，
所以这一条能把两种写法分开。


### 10.3.2 开散列方法(拉链法)

开散列不在表内挤位置：每个槽挂一条链表（或动态数组），散列到同一地址的关键码都串在这条链上。插入是算地址再接到链头或链尾；查找、删除只在那一条链上走。删掉一个结点不会影响别的链，也不存在「探测序列被截断」的问题。

装载因子 $\alpha=n/m$ 可以大于 1，链只是变长。成功查找的期望比较次数大约是 $1+\alpha/2$。实现简单，空间上每个结点多一个指针。本章主实现是闭散列，开散列不另写一份。

### 10.3.3 闭散列方法(开地址法)

闭散列把冲突消化在表内：回家地址被占了，就按某种规则探测下一个空槽。线性探测是最简单的一种——第 $i$ 次看 $(h(k)+i)\bmod m$。二次探测、双重散列是为了减轻「挤成一团」的一次聚集。

表满了就插不进去，所以 $\alpha$ 必须小于 1，实务上常在 0.5 到 0.7 之间扩表。删除是闭散列的难点：不能把槽直接标成空。否则沿这条探测链查找后面的键时，会在空槽处误判「从来没插入过」而提前停止。解决办法是给槽加第三种状态——除了「空」和「有元素」，再加一个**墓碑**（tombstone），意思是「这里曾经有元素，被删掉了，但探测链还要继续往下走」。

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
// 第 10 章「先跑一遍」：用教学版 HashTable 观察线性探测与墓碑删除。
// 编译运行：
//   g++ -std=c++17 -I code/ch10/search_hash code/ch10/search_hash/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    HashTable table(5);
    if (!table.insert(1) || !table.insert(6) || !table.insert(11)) {
        std::cout << "插入 1、6、11 失败\n";
        return 1;
    }

    std::cout << "插入 1、6、11 后:\n";
    for (std::size_t index = 0; index < table.capacity(); ++index) {
        const auto slot = table.slot_at(index);
        std::cout << "  槽 " << index << ": ";
        if (slot.state == HashTable::SlotState::used) {
            std::cout << slot.key << '\n';
        } else if (slot.state == HashTable::SlotState::tombstone) {
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

`HashTable` 的实现见上面那份清单。三处值得停一下，教学版的测试各有一条具名断言守着：

- **删除只能标墓碑。** 上面那段推演不是纸上谈兵——把 `erase` 改成标 `empty`，
  「删掉 3 之后 10 还找不找得到」这条立刻变红。
- **插入要回收墓碑**，否则表用久了探测链只会越来越长。
- **回收墓碑时不能提前停**：碰到墓碑就返回，会把一个已经在后面的键插第二遍。
  这一条最隐蔽，所以单列了一个用例。

### 10.3.4a 进阶（选读）：工程版差在哪

工程版在 `code/ch10/search_hash/modern.hpp`。这一章没有手写存储管理，
所以没有三法则/五法则之分，差别只有标注与排版：查询函数标了 `[[nodiscard]]`
（防止「查了却忘了看结果」）与 `noexcept`，若干 `if` 分支压成一行。逻辑完全一致。


### 10.3.5 散列方法的效率分析

成功检索的期望探测次数随装载因子 α 上升。线性探测大约是 $(1+1/(1-\alpha))/2$；拉链法是 $1+\alpha/2$。α 接近 1 时闭散列急剧变差，应扩表或改开散列。

### 10.3.6 散列方法的应用

编译器符号表、缓存、去重都常用散列。需要范围查询或有序遍历时，应改用树或有序表。

## 本章小结

检索是按关键码或属性定位记录。线性表上有顺序、二分和分块：分别要求无序、有序可随机访问、块间有序。集合只关心「在不在」，插入已存在或删除不存在都是正常失败。散列用函数直接定位槽位，期望 $O(1)$；开散列把冲突挂到链上，闭散列在表内探测。闭散列删除必须留墓碑，否则会截断探测链。装载因子过高时要扩表。

## 习题

### 补充检索题（参考课程第 10 章）

1. 给定一个有序双向链表和当前指针，设计可向前或向后移动的检索算法，并求平均检索长度。
2. 说明堆排序后再对另一集合逐项二分检索时，总复杂度如何由排序成本和检索次数共同决定。
3. 比较拉链法和线性探测在装填因子变化时的成功检索代价。

1. 写出顺序检索在最好、最坏、等概率平均时的 ASL。
2. 用半开区间手算二分查找 18 在有序表 `{1,3,7,8,12,15,18,21}` 中的过程。
3. 分块检索的块数取多少时 ASL 较均衡？为什么。
4. 说明集合的 `insert` 为什么返回 `bool` 而不是抛异常。
5. 表长 5，依次插入 1、6、11，画出线性探测的槽；删除 1 后若标空，查找 6 会怎样。
6. 比较开散列与闭散列的删除、装载因子上限和指针开销。
7. ELFHash 为什么按 `unsigned char` 读字节。

## 上机题

1. 对同一组随机键比较顺序、二分和散列的平均探测次数。
2. 实现闭散列的墓碑删除，并写测试：删掉头键后仍能查到其后的同址键。
3. 装载因子超过 0.7 时扩表再散列，观察聚集是否缓解。
4. 用 `IntSet` 的接口换上散列表实现，确认交、包含的测试不用改。
