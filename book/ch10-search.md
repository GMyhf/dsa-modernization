# 第10章 检索与散列

顺序/二分检索、集合、ELFHash 和开放定址散列表均以显式返回状态处理未找到与墓碑删除。

```cpp file=code/ch10/search_hash/modern.hpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace dsa { namespace search {
// >>> search-hash
inline std::optional<std::size_t> sequential(const std::vector<int>&a,int x){for(std::size_t i=0;i<a.size();++i)if(a[i]==x)return i;return std::nullopt;} inline std::optional<std::size_t> binary(const std::vector<int>&a,int x){std::size_t l=0,r=a.size();while(l<r){auto m=(l+r)/2;if(a[m]==x)return m;if(a[m]<x)l=m+1;else r=m;}return std::nullopt;}
class IntSet{public:bool insert(int x){if(contains(x))return false;a_.push_back(x);return true;}bool erase(int x){auto p=sequential(a_,x);if(!p)return false;a_.erase(a_.begin()+static_cast<std::ptrdiff_t>(*p));return true;}[[nodiscard]]bool contains(int x)const{return sequential(a_,x).has_value();}[[nodiscard]]IntSet intersection(const IntSet&o)const{IntSet r;for(int x:a_)if(o.contains(x))r.insert(x);return r;}[[nodiscard]]bool includes(const IntSet&o)const{for(int x:o.a_)if(!contains(x))return false;return true;}[[nodiscard]]std::size_t size()const noexcept{return a_.size();}private:std::vector<int>a_;};
class HashTable{enum class State{empty,used,tombstone};struct Slot{int key{};State state{State::empty};};public:explicit HashTable(std::size_t n):a_(n? n:throw std::invalid_argument("capacity")){}bool insert(int x){auto i=slot(x);if(i==a_.size())return false;if(a_[i].state==State::used)return false;a_[i]={x,State::used};return true;}[[nodiscard]]bool contains(int x)const{return find(x).has_value();}bool erase(int x){auto i=find(x);if(!i)return false;a_[*i].state=State::tombstone;return true;}private:std::size_t home(int x)const{return static_cast<std::size_t>(x>=0?x:-static_cast<long long>(x))%a_.size();}std::optional<std::size_t>find(int x)const{for(std::size_t k=0;k<a_.size();++k){auto i=(home(x)+k)%a_.size();if(a_[i].state==State::empty)return std::nullopt;if(a_[i].state==State::used&&a_[i].key==x)return i;}return std::nullopt;}std::size_t slot(int x)const{std::optional<std::size_t>t;for(std::size_t k=0;k<a_.size();++k){auto i=(home(x)+k)%a_.size();if(a_[i].state==State::used&&a_[i].key==x)return i;if(a_[i].state==State::tombstone&&!t)t=i;if(a_[i].state==State::empty)return t?*t:i;}return t?*t:a_.size();}std::vector<Slot>a_;}; inline std::size_t elf_hash(const std::string&s){std::size_t h=0;for(unsigned char c:s){h=(h<<4)+c;auto g=h&0xF0000000U;if(g)h^=g>>24;h&=~g;}return h;}
// <<< search-hash
}}
```
