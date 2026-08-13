# 第1章 概论

数据结构刻画信息及其关系，算法描述求解过程。二者互相依赖：结构选错了，算法再巧也补不回来。本章先用股市传言把问题说到能写程序，再补回逻辑结构、存储映射、抽象数据类型和渐进分析——这些原书没错，是后面十一章共用的词汇。

## 1.1 问题求解

程序是指令的组合，算法是求解过程的逻辑抽象，数据结构是数据及其关系在计算机里的反映。Wirth 的「程序 = 数据结构 + 算法」说的就是这件事。求解路径通常是：把现实问题抽象成模型，选定逻辑结构与存储方式，再设计算法并写成可运行的程序。

### 1.1.1 问题描述：股市的传言

本章把原书 1.1 的“股市传言”问题写成一个可直接调用的程序。它不是在找“边最多”的经纪人，
而是在找一个起点：从这个人出发能把消息传给**所有**人，并且最慢的那条最短传播路径尽可能短。

### 1.1.2 问题分析和抽象

把 B1 到 B5 看成五个顶点。一条 `Bi -> Bj` 边表示消息能从 `Bi` 直接传给 `Bj`；边权是传播
时间。没有路径就记为 `∞`，表示消息永远传不到那里。B3 是唯一可以到达全部经纪人的起点。

![图1.1 经纪人间的消息传递图](assets/4d3c879e7409ef1c.jpg)

图1.1 经纪人间的消息传递图

原书算法 1.1 的工作可以拆成三步：

1. Floyd 算法算出任意两人间的最短传播时间。
2. 对每一个候选起点，找出“从它出发到每个人的最短时间”中最慢的一项。
3. 从这些最大值中选最小的一个；如果某行仍有不可达的 `∞`，该起点不能胜任。

原书中的 `D` 对应现代代码里的 `shortest` 矩阵，`max[i]` 对应每一行的
`largest_distance`，`pos` 对应 `best_source()` 的返回值。下标从 0 开始，因此 B3 的返回值是 2。

### 为什么要取“最慢的一项”

假设从 B3 开始，Floyd 算出的最短传播时间为：到 B1 是 6，到 B2 是 7，到自身是 0，到 B4
是 10，到 B5 是 2。消息要“传遍所有人”，必须等最慢的 B4 收到消息，因此 B3 的完成时间是
这五个数里的最大值 10，而不是它们的和，也不是最小值。

把每个人都当一次起点，结果如下。`∞` 表示至少有一人永远收不到消息，所以这一行没有可用的
完成时间。

| 起点 | 到 B1 | 到 B2 | 到 B3 | 到 B4 | 到 B5 | 最慢的最短时间 | 是否可选 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| B1 | 0 | ∞ | ∞ | 4 | 3 | ∞ | 否 |
| B2 | 13 | 0 | ∞ | 17 | 8 | ∞ | 否 |
| B3 | 6 | 7 | 0 | 10 | 2 | **10** | 是 |
| B4 | ∞ | ∞ | ∞ | 0 | ∞ | ∞ | 否 |
| B5 | 5 | 5 | ∞ | 9 | 0 | ∞ | 否 |

因此答案是 B3。这个目标常称为“最小化最大距离”：不是选离某一个人最近的起点，而是选让
**最后一个**收到消息的人也尽可能早收到消息的起点。

### 1.1.3 数据结构和算法设计

下面是完整可运行的程序。

下面是完整可运行的程序。`RumorNetwork(5)` 创建 B1 至 B5；`add_route(from, to, cost)` 添加一条
**有向**传播路径，三个参数均从 0 开始编号。`best_source()` 返回 `std::optional<std::size_t>`：
有解时保存起点下标，无解时是 `std::nullopt`。

源码文件在这里：[可运行示例](../code/ch01/adt/demo.cpp)、
[数据结构实现](../code/ch01/adt/modern.hpp)、[测试用例](../code/ch01/adt/test.cpp)。
`demo.cpp` 负责输入/输出；`modern.hpp` 只负责数据和计算，这就是把“怎么展示”与“怎么算”分开。

```cpp file=code/ch01/adt/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    // B1 ... B5 对应下标 0 ... 4。
    dsa::adt::RumorNetwork network(5);
    network.add_route(0, 3, 4);   // B1 -> B4，耗时 4
    network.add_route(0, 4, 3);   // B1 -> B5，耗时 3
    network.add_route(1, 4, 8);   // B2 -> B5，耗时 8
    network.add_route(2, 0, 6);   // B3 -> B1，耗时 6
    network.add_route(2, 1, 7);   // B3 -> B2，耗时 7
    network.add_route(2, 3, 10);  // B3 -> B4，耗时 10
    network.add_route(2, 4, 2);   // B3 -> B5，耗时 2
    network.add_route(4, 0, 5);   // B5 -> B1，耗时 5
    network.add_route(4, 1, 5);   // B5 -> B2，耗时 5

    const auto source = network.best_source();
    if (source) {
        std::cout << "最佳传播起点是 B" << *source + 1 << '\n';
    } else {
        std::cout << "不存在能到达全部经纪人的起点\n";
    }
}
```

在仓库根目录运行：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch01/adt code/ch01/adt/demo.cpp -o /tmp/rumor-demo
/tmp/rumor-demo
```

第一行是“编译”，第二行是“运行刚刚编译出的程序”。逐项解释如下：

| 片段 | 含义 |
| --- | --- |
| `c++` | C++ 编译器命令。在 macOS 上通常指 Clang；Linux 上也可能指 GCC。 |
| `-std=c++17` | 按 C++17 语言规则编译；本教程的 `std::optional` 需要 C++17。 |
| `-Wall -Wextra` | 打开常见和额外的编译器警告，例如未使用变量或可疑转换。 |
| `-Werror` | 把警告当作错误；用于学习时可及早发现问题。初学调试时可暂时去掉它。 |
| `-Icode/ch01/adt` | 加一个头文件搜索目录，因此 `#include "modern.hpp"` 能找到 `code/ch01/adt/modern.hpp`。 |
| `code/ch01/adt/demo.cpp` | 要编译的源文件。它包含 `main()`，所以可以成为可执行程序。 |
| `-o /tmp/rumor-demo` | 指定输出文件名为 `/tmp/rumor-demo`。`/tmp` 是临时目录，重启后文件可能消失。 |
| `/tmp/rumor-demo` | 运行该可执行文件，打印程序结果。 |

如果系统提示 `c++: command not found`，需要先安装 C++ 编译器；如果想把程序留在当前目录，
可以把 `-o /tmp/rumor-demo` 改成 `-o rumor-demo`，再运行 `./rumor-demo`。

输出是：

```console
最佳传播起点是 B3
```

如果删除 B3 的某条出边，例如 `network.add_route(2, 1, 7)`，B3 将无法到达 B2；当不存在任何
能覆盖全部顶点的起点时，`best_source()` 返回空值，程序会输出“不存在能到达全部经纪人的起点”。

可以先只改边和权值，再运行同一条命令观察结果。例如把 `B3 -> B4` 的时间从 10 改为 1，答案
仍是 B3，只是它的最慢传播时间从 10 缩短为 7。

### 实现导读

先只关注三个公开操作：构造函数建立距离矩阵，`add_route` 填入直接边，`best_source` 计算答案。
`best_source` 内部复制一份矩阵，因而多次调用不会改变原始网络。下面按代码出现顺序解释。

#### 预处理、头文件与命名空间

`#pragma once` 告诉编译器：同一个头文件即使被间接包含多次，也只处理一次，避免类定义重复。

| 头文件 | 本程序用它做什么 |
| --- | --- |
| `<cstddef>` | 提供 `std::size_t`，表示非负的大小或下标。 |
| `<limits>` | 提供 `std::numeric_limits<int>::max()`，取得 `int` 可表示的最大值。 |
| `<optional>` | 提供 `std::optional`，表达“可能没有答案”。 |
| `<stdexcept>` | 提供 `std::invalid_argument`，用于报告非法参数。 |
| `<vector>` | 提供动态数组 `std::vector`，这里用它保存二维距离矩阵。 |

`namespace dsa::adt { ... }` 把名字放入 `dsa::adt` 命名空间。完整类名因此是
`dsa::adt::RumorNetwork`，可避免项目中另一个人也定义 `RumorNetwork` 时发生重名。
代码块中的 `// >>> adt` 与 `// <<< adt` 只是本书稿的同步标记，不参与 C++ 逻辑。

#### 类、常量和构造函数

`class RumorNetwork` 定义一种新类型。`public:` 以下是调用者可以使用的接口；`private:` 以下是
实现细节，调用者不能直接改写。

```text
static constexpr int infinity = std::numeric_limits<int>::max() / 4;
```

`static` 表示 `infinity` 属于类本身，而不是每个对象各存一份；`constexpr` 表示编译期常量。
取最大值的四分之一而非最大值，是为了计算 `a + b` 时仍留有余量，避免整数溢出。

```text
explicit RumorNetwork(std::size_t people)
    : distance_(people, std::vector<int>(people, infinity)) { ... }
```

这是构造函数：`RumorNetwork network(5)` 会调用它。冒号后的部分叫**成员初始化列表**，先创建
`distance_`，再进入花括号。它构造一个 5 行、每行 5 列的整数矩阵，初始全为 `infinity`。随后
循环把对角线改为 0，因为一个人到自己不需要传播时间。矩阵的第 `from` 行、第 `to` 列总是表示
从 `from` 到 `to` 的当前已知最短时间。

#### 添加一条边

```text
void add_route(std::size_t from, std::size_t to, int cost)
```

这三个参数分别是起点编号、终点编号和直接传播时间。第一段 `if` 检查编号是否越界、时间是否为
负；不满足题目定义时抛出 `std::invalid_argument`，调用者可用 `try/catch` 处理。第二段 `if` 只在
新边更短时更新矩阵，所以即使重复添加同一方向的边，也保留较小的时间。

#### Floyd 三重循环

`auto shortest = distance_;` 复制输入矩阵。`auto` 让编译器自动推断类型，这里实际是
`std::vector<std::vector<int>>`。

三层循环的含义不是“随便循环三次”，而是按中转站逐步扩大可用路径：

```text
for each via:       允许路径经过 via
  for each from:    固定起点 from
    for each to:    固定终点 to
      比较 原来的 from->to 与 from->via->to
```

条件 `shortest[from][via] != infinity && shortest[via][to] != infinity` 先确认两段都可达。
若 `from -> via -> to` 的总时间更小，就更新 `shortest[from][to]`。例如 B2 到 B4 没有直接边，
但 B2→B5 是 8、B5→B1 是 5、B1→B4 是 4，所以 Floyd 最终得到 B2→B4 是 `8 + 5 + 4 = 17`。

#### 从最短路矩阵选答案

```text
std::optional<std::size_t> result;
int smallest_eccentricity = infinity;
```

默认构造的 `result` 是空值，表示“还没有找到合格起点”。`smallest_eccentricity` 保存目前见过的
最小完成时间。这里的 eccentricity（离心率）就是某起点到全部顶点的最短距离中的最大值。

外层循环每次处理一行，即一个候选起点；内层循环扫描该行。三目表达式
`distance > largest_distance ? distance : largest_distance` 等价于“若新值更大就采用新值，否则保留
旧值”，最终得到这行的最大值。若它比当前最佳值小，就同时更新最佳时间与 `result`。

任何一行含 `infinity` 时，该行最大值也是 `infinity`，不会优于有限答案；若所有行都是
`infinity`，`result` 保持为空，函数返回 `std::nullopt`。`[[nodiscard]]` 提醒编译器：调用者不应
无意丢弃这个可能为空的重要返回值。

#### 私有数据成员

```text
std::vector<std::vector<int>> distance_;
```

外层 `vector` 是行，内层 `vector<int>` 是一行中的列，合起来是二维矩阵。末尾下划线是本教程的
约定，表示私有成员；它与函数参数 `distance` 或局部变量 `shortest` 不会混淆。

时间复杂度为 O(V^3)，空间复杂度为 O(V^2)。这适合顶点数较少、需要比较所有起点的示例；大图
通常应根据图的稀疏度和查询需求选择其他最短路径算法。

### 现代实现

```cpp file=code/ch01/adt/modern.hpp
#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dsa::adt {

// >>> adt
class RumorNetwork {
public:
    static constexpr int infinity = std::numeric_limits<int>::max() / 4;

    explicit RumorNetwork(std::size_t people)
        : distance_(people, std::vector<int>(people, infinity)) {
        for (std::size_t person = 0; person < people; ++person) {
            distance_[person][person] = 0;
        }
    }

    void add_route(std::size_t from, std::size_t to, int cost) {
        if (from >= distance_.size() || to >= distance_.size() || cost < 0) {
            throw std::invalid_argument("route");
        }
        if (cost < distance_[from][to]) {
            distance_[from][to] = cost;
        }
    }

    [[nodiscard]] std::optional<std::size_t> best_source() const {
        auto shortest = distance_;
        for (std::size_t via = 0; via < shortest.size(); ++via) {
            for (std::size_t from = 0; from < shortest.size(); ++from) {
                for (std::size_t to = 0; to < shortest.size(); ++to) {
                    if (shortest[from][via] != infinity &&
                        shortest[via][to] != infinity &&
                        shortest[from][to] > shortest[from][via] + shortest[via][to]) {
                        shortest[from][to] = shortest[from][via] + shortest[via][to];
                    }
                }
            }
        }

        std::optional<std::size_t> result;
        int smallest_eccentricity = infinity;
        for (std::size_t from = 0; from < shortest.size(); ++from) {
            int largest_distance = 0;
            for (int distance : shortest[from]) {
                largest_distance = distance > largest_distance ? distance : largest_distance;
            }
            if (largest_distance < smallest_eccentricity) {
                smallest_eccentricity = largest_distance;
                result = from;
            }
        }
        return result;
    }

private:
    std::vector<std::vector<int>> distance_;
};
// <<< adt

}  // namespace dsa::adt
```

## 1.2 数据结构

数据的逻辑结构描述对象之间的关系，不依赖计算机；存储结构是同一套关系在内存或外存上的映射。同一种逻辑结构可以有多种存储方式，后面各章都在做这种选择。

### 1.2.1 数据的逻辑结构

常见的逻辑结构有四类：集合（元素之间只有「同属」）、线性结构（除两端外每个元素一个前驱一个后继）、树形结构（一个前驱、多个后继）、图形结构（前驱后继都可以多个）。传言问题的经纪人关系是有向带权图。

### 1.2.2 数据的存储结构

四种基本映射：

1. **顺序**：逻辑相邻用地址相邻表示，随机访问 O(1)，插入删除要搬迁。
2. **链接**：结点自带后继指针，插入删除改常数条指针，按位置访问仍须循链。
3. **索引**：另造一张「整数 → 地址」的表，用空间换定位时间。
4. **散列**：由关键码直接算出地址；冲突与装载因子是后话，见第 10 章。

![图 1.4 索引示例](assets/40916b3631d4d819.jpg)

图 1.4 索引示例

### 1.2.3 抽象数据类型

ADT 是三元组 (D, R, P)：数据对象、对象上的关系、一组运算。对外只看见运算，看不见存储。原书【代码1.2】用一个空模板类描述这件事；本书各章把运算直接写在实现类上，对元素类型的要求用 `static_assert` 写在类头——空基类给不了多态，还会带来非虚析构。

## 1.3 算法

算法是有穷、确定、有效的指令序列，对符合类型的任意输入都能停机并给出正确结果。程序是算法在某种语言里的实现。

### 1.3.1 算法的概念

四条性质：通用（同类输入都能算）、有效（每一步都能执行）、确定（下一步不模糊）、有穷（不会死循环）。「有穷条指令」并不自动保证有穷步执行。

### 1.3.2 算法设计

常用路数：穷举、回溯、分治与递归、贪心与动态规划。第 3 章背包是回溯，第 8 章快排和归并是分治，第 7 章 Prim / Kruskal / Dijkstra 是贪心，Floyd 和最优 BST 是动态规划。传言问题选 Floyd，是因为它要任意两点最短路，而且顶点数很小。

## 1.4 算法分析

比较算法不看墙上的秒数，而看基本操作次数随输入规模 n 怎么长。同一段代码在巨型机和 PC 上差几个数量级，但增长趋势不变。

### 1.4.1 渐进分析方法

只保留主导项，丢掉常数和低阶项。$f(n)=n^2+100n+\log n+1000$ 在 n 很大时就是 $\Theta(n^2)$。

- **大 O**：存在 $c,N$ 使 $n\ge N$ 时 $f(n)\le c\,g(n)$，增长率的上限。
- **Ω**：不等式反过来，下限。
- **Θ**：上下限同阶。

常见阶：$O(1)$、$O(\log n)$、$O(n)$、$O(n\log n)$、$O(n^2)$、$O(a^n)$。指数增长比任何多项式都快：$n=1000$ 时 $2^n$ 的微秒数已经远超地球年龄。

![图 1.5 常用函数的增长趋势](assets/c33523f30da594f0.jpg)

图 1.5 常用函数的增长趋势

对数组求和是 $2+2n$ 次赋值，即 $O(n)$；两层循环扫子数组就是 $O(n^2)$。后面各章的「时间代价」都按这个口径写。

### 1.4.2 最佳、最差和平均情况

同一算法对不同输入的操作次数可以差很多。快排平均 $O(n\log n)$、最坏 $O(n^2)$；顺序检索最好 1 次、最坏 n 次。分析时必须说清是哪一种，不能只报一个数字。

### 1.4.3 时间和空间的折衷

预排序、建索引、开散列表都是用预处理或额外空间换检索时间。没有免费的加速。

### 1.4.4 求解问题时数据结构的选择和评价

先看运算集合（要不要随机访问、会不会频繁插入），再看规模和存储层次（内存还是外存）。传言问题要任意两点最短路且 n 很小，所以选邻接矩阵加 Floyd，而不是邻接表加反复 Dijkstra。
