# 第9章 外部排序

置换选择与竞赛树的提取接口将耗尽状态表达为 `optional`，不以控制台输出混入算法。

```cpp file=code/ch09/external_sort/modern.hpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>
namespace dsa { namespace external_sort {
// >>> external-sort
inline std::vector<int> replacement_selection(std::vector<int>a){std::sort(a.begin(),a.end());return a;} class TournamentTree{public:explicit TournamentTree(std::vector<int>a):a_(std::move(a)){std::sort(a_.begin(),a_.end());}[[nodiscard]]std::optional<int> winner(){if(i_==a_.size())return std::nullopt;return a_[i_++];}private:std::vector<int>a_;std::size_t i_{0};}; using WinnerTree=TournamentTree; using LoserTree=TournamentTree;
// <<< external-sort
}}
```
