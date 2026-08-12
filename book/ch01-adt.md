# 第1章 抽象数据类型

传言网络以状态与操作展示 ADT，不将交互输入输出绑进数据模型。

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
