# 第1章 抽象数据类型

本章把原书 1.1 的“股市传言”问题写成一个可直接调用的程序。它不是在找“边最多”的经纪人，
而是在找一个起点：从这个人出发能把消息传给**所有**人，并且最慢的那条最短传播路径尽可能短。

## 1.1 先把题目说清楚

把 B1 到 B5 看成五个顶点。一条 `Bi -> Bj` 边表示消息能从 `Bi` 直接传给 `Bj`；边权是传播
时间。没有路径就记为 `∞`，表示消息永远传不到那里。B3 是唯一可以到达全部经纪人的起点。

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

## 1.2 如何调用

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

输出是：

```console
最佳传播起点是 B3
```

如果删除 B3 的某条出边，例如 `network.add_route(2, 1, 7)`，B3 将无法到达 B2；当不存在任何
能覆盖全部顶点的起点时，`best_source()` 返回空值，程序会输出“不存在能到达全部经纪人的起点”。

可以先只改边和权值，再运行同一条命令观察结果。例如把 `B3 -> B4` 的时间从 10 改为 1，答案
仍是 B3，只是它的最慢传播时间从 10 缩短为 7。

## 1.3 再读实现

先只关注三个公开操作：构造函数建立距离矩阵，`add_route` 填入直接边，`best_source` 计算答案。
`best_source` 内部复制一份矩阵，因而多次调用不会改变原始网络。三重循环是 Floyd：外层枚举
中转站 `via`，内层尝试用 `from -> via -> to` 更新 `from -> to`。之后的双重循环正是上节的
“每行取最大、所有行再取最小”。

时间复杂度为 O(V^3)，空间复杂度为 O(V^2)。这适合顶点数较少、需要比较所有起点的示例；大图
通常应根据图的稀疏度和查询需求选择其他最短路径算法。

## 1.4 现代实现

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
