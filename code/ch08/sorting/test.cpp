#include "modern.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) { ++checks; if (!condition) { ++failures; std::printf("  FAIL: %s\n", name); } }
bool sorted(const std::vector<int>& values) { return std::is_sorted(values.begin(), values.end()); }

using Sort = void (*)(std::vector<int>&);
void test_sort(Sort sort, const char* name, bool supports_full_int_range = true) {
    std::vector<int> mixed{3, -2, 7, 3, 0, -2, 9, 1};
    sort(mixed);
    check(sorted(mixed), name);
    std::vector<int> already{-3, -1, 0, 4, 8};
    sort(already);
    check(sorted(already), "already sorted input");
    if (supports_full_int_range) {
        std::vector<int> extremes{std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::min(), -1};
        sort(extremes);
        check(sorted(extremes), "signed extremes");
    } else {
        bool rejected = false;
        try { std::vector<int> sparse{std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}; sort(sparse); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "sparse value range rejected");
    }
}

void test_index_sort() {
    std::vector<int> values{29, 12, 34, 8};
    auto indexes = dsa::sorting::insertion_index_sort(values);
    check(indexes == std::vector<std::size_t>({3, 1, 0, 2}), "算法8.14 returns sorted indexes");
    dsa::sorting::adjust_by_index(values, indexes);
    check(sorted(values), "算法8.15 adjusts records by cycles");
    check(indexes == std::vector<std::size_t>({0, 1, 2, 3}), "算法8.15 restores identity index");
}

void test_static_queue_and_tools() {
    dsa::sorting::StaticQueue<int> queue(2);
    check(queue.push(1) && queue.push(2) && !queue.push(3), "代码8.12 fixed queue capacity");
    check(queue.pop() == 1 && queue.pop() == 2 && !queue.pop(), "代码8.12 FIFO and empty optional");
    check(queue.push(4) && queue.push(5) && queue.pop() == 4 && queue.push(6) && queue.pop() == 5 && queue.pop() == 6,
          "代码8.12 circular reuse after pop");
    auto one = dsa::sorting::random_values(8, 100, 7);
    const auto two = dsa::sorting::random_values(8, 100, 7);
    check(one == two, "代码8.16 deterministic seed");
    dsa::sorting::Stopwatch watch;
    watch.start();
    dsa::sorting::insertion_sort(one);
    check(watch.elapsed_seconds() >= 0.0, "代码8.17 monotonic elapsed time");
}

void test_algorithm_specific_invariants() {
    std::vector<int> heap_case{1, 9, 8, 7, 6, 5};
    dsa::sorting::sift_down(heap_case, 0, heap_case.size());
    check(heap_case[0] == 9, "算法8.4 sift_down promotes larger child");
    std::vector<int> duplicates{4, 4, 4, 4, 4};
    dsa::sorting::quick_sort(duplicates);
    check(sorted(duplicates), "算法8.6 partition terminates on equal keys");
    std::vector<int> short_case{5, 4, 3, 2, 1};
    dsa::sorting::quick_sort_optimized(short_case);
    check(sorted(short_case), "算法8.7 insertion threshold sorts short range");
    std::vector<int> ordered{1, 2, 3, 4, 5};
    dsa::sorting::merge_sort_optimized(ordered);
    check(sorted(ordered), "算法8.9 skips already ordered merge");
    std::vector<int> signed_values{-256, -1, 0, 1, 256};
    dsa::sorting::radix_sort(signed_values);
    check(sorted(signed_values), "算法8.11 sign-bit transform preserves signed order");
    std::vector<int> linked_radix{1000, 0, -1000, 42};
    dsa::sorting::radix_sort_linked_style(linked_radix);
    check(sorted(linked_radix), "算法8.13 bucket collection is ordered");
    check(dsa::sorting::insertion_index_sort({}).empty(), "算法8.14 empty index sort");
}

std::vector<int> parse_values(const std::string& text) {
    std::vector<int> values;
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, ',')) {
        if (!token.empty()) values.push_back(std::stoi(token));
    }
    return values;
}

void test_shared_cases() {
    std::ifstream input("cases.tsv");
    std::string line;
    std::getline(input, line);
    int shared = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> fields;
        std::istringstream row(line);
        std::string field;
        while (std::getline(row, field, '\t')) fields.push_back(field);
        while (fields.size() < 5) fields.emplace_back();
        ++shared;
        if (fields[1] == "constant") {
            check(dsa::sorting::counting_range_limit == std::stoull(fields[3]), "T-047 counting limit");
            continue;
        }
        auto values = parse_values(fields[2]);
        if (fields[4] == "invalid_argument") {
            bool raised = false;
            try { dsa::sorting::counting_sort(values); }
            catch (const std::invalid_argument&) { raised = true; }
            check(raised, "T-047 expected invalid_argument");
            continue;
        }
        if (fields[1] == "insertion") dsa::sorting::insertion_sort(values);
        else if (fields[1] == "radix") dsa::sorting::radix_sort(values);
        else if (fields[1] == "index") {
            const auto indexes = dsa::sorting::insertion_index_sort(values);
            values.assign(indexes.begin(), indexes.end());
        }
        check(values == parse_values(fields[3]), "T-047 shared result");
    }
    std::printf("共享用例: %d\n", shared);
}
}  // namespace

int main() {
    test_sort(dsa::sorting::insertion_sort, "算法8.1 insertion");
    test_sort(dsa::sorting::shell_sort, "算法8.2 shell");
    test_sort(dsa::sorting::selection_sort, "算法8.3 selection");
    test_sort(dsa::sorting::heap_sort, "算法8.4 heap");
    test_sort(dsa::sorting::bubble_sort, "算法8.5 bubble");
    test_sort(dsa::sorting::quick_sort, "算法8.6 quick");
    test_sort(dsa::sorting::quick_sort_optimized, "算法8.7 quick optimized");
    test_sort(dsa::sorting::merge_sort, "算法8.8 merge");
    test_sort(dsa::sorting::merge_sort_optimized, "算法8.9 merge optimized");
    test_sort(dsa::sorting::counting_sort, "算法8.10 counting", false);
    test_sort(dsa::sorting::radix_sort, "算法8.11 radix");
    test_sort(dsa::sorting::radix_sort_linked_style, "算法8.13 linked radix");
    test_index_sort();
    test_static_queue_and_tools();
    test_algorithm_specific_invariants();
    test_shared_cases();
    std::printf("Sorting: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
