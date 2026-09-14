#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dsa::adt {

// >>> adt
class RumorNetwork {
public:
    static constexpr int infinity = std::numeric_limits<int>::max() / 4;
    /// 最短时间是若干段之和，可能超过 int；用 64 位，「到不了」另用 unreachable（D-039）。
    using distance_type = std::int64_t;
    static constexpr distance_type unreachable = std::numeric_limits<distance_type>::max();

    explicit RumorNetwork(std::size_t people)
        : distance_(people, std::vector<int>(people, infinity)) {
        for (std::size_t person = 0; person < people; ++person) {
            distance_[person][person] = 0;
        }
    }

    void add_route(std::size_t from, std::size_t to, int cost) {
        // cost 不小于 infinity 时，下面的 Floyd 会把这条路线当成「没有路线」，结果静默出错。
        if (from >= distance_.size() || to >= distance_.size() || cost < 0 || cost >= infinity) {
            throw std::invalid_argument("route");
        }
        if (cost < distance_[from][to]) {
            distance_[from][to] = cost;
        }
    }

    [[nodiscard]] std::optional<std::size_t> best_source() const {
        // >>> best-source-floyd
        const std::size_t people = distance_.size();
        std::vector<std::vector<distance_type>> shortest(
            people, std::vector<distance_type>(people, unreachable));
        for (std::size_t from = 0; from < people; ++from) {
            for (std::size_t to = 0; to < people; ++to) {
                if (distance_[from][to] != infinity) {
                    shortest[from][to] = distance_[from][to];
                }
            }
        }
        for (std::size_t via = 0; via < people; ++via) {
            for (std::size_t from = 0; from < people; ++from) {
                for (std::size_t to = 0; to < people; ++to) {
                    if (shortest[from][via] != unreachable &&
                        shortest[via][to] != unreachable &&
                        shortest[from][to] > shortest[from][via] + shortest[via][to]) {
                        shortest[from][to] = shortest[from][via] + shortest[via][to];
                    }
                }
            }
        }
        // <<< best-source-floyd

        // >>> best-source-pick
        std::optional<std::size_t> result;
        distance_type smallest_eccentricity = unreachable;
        for (std::size_t from = 0; from < people; ++from) {
            distance_type largest_distance = 0;
            for (distance_type distance : shortest[from]) {
                largest_distance = distance > largest_distance ? distance : largest_distance;
            }
            if (largest_distance < smallest_eccentricity) {
                smallest_eccentricity = largest_distance;
                result = from;
            }
        }
        return result;
        // <<< best-source-pick
    }

private:
    std::vector<std::vector<int>> distance_;
};
// <<< adt

}  // namespace dsa::adt
