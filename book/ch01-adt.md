# 第1章 抽象数据类型

传言网络以状态与操作展示 ADT，不将交互输入输出绑进数据模型。

```cpp file=code/ch01/adt/modern.hpp
#pragma once
#include <optional>
#include <string>
#include <unordered_map>
namespace dsa { namespace adt {
// >>> adt
class RumorNetwork { public: void tell(const std::string&from,const std::string&to){heard_[to]=from;} [[nodiscard]]std::optional<std::string> source_of(const std::string&person)const{auto i=heard_.find(person);return i==heard_.end()?std::nullopt:std::optional<std::string>(i->second);} private:std::unordered_map<std::string,std::string>heard_;};
// <<< adt
}}
```
