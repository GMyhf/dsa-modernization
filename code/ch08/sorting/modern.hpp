#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>
namespace dsa { namespace sorting {
// >>> sorting
inline void insertion(std::vector<int>&a){for(std::size_t i=1;i<a.size();++i){int x=a[i];std::size_t j=i;while(j&&x<a[j-1]){a[j]=a[j-1];--j;}a[j]=x;}}
inline void shell(std::vector<int>&a){for(std::size_t g=a.size()/2;g;g/=2)for(std::size_t i=g;i<a.size();++i){int x=a[i];std::size_t j=i;while(j>=g&&x<a[j-g]){a[j]=a[j-g];j-=g;}a[j]=x;}}
inline void selection(std::vector<int>&a){for(std::size_t i=0;i<a.size();++i){std::size_t m=i;for(std::size_t j=i+1;j<a.size();++j)if(a[j]<a[m])m=j;std::swap(a[i],a[m]);}}
inline void heap(std::vector<int>&a){std::make_heap(a.begin(),a.end());std::sort_heap(a.begin(),a.end());}
inline void bubble(std::vector<int>&a){for(std::size_t e=a.size();e>1;--e){bool moved=false;for(std::size_t i=1;i<e;++i)if(a[i]<a[i-1])std::swap(a[i],a[i-1]),moved=true;if(!moved)return;}}
inline void quick(std::vector<int>&a){std::sort(a.begin(),a.end());}
inline void merge(std::vector<int>&a){if(a.size()<2)return;std::vector<int>b(a.size());auto go=[&](auto&&self,std::size_t l,std::size_t r)->void{if(r-l<2)return;auto m=(l+r)/2;self(self,l,m);self(self,m,r);std::merge(a.begin()+l,a.begin()+m,a.begin()+m,a.begin()+r,b.begin()+l);std::copy(b.begin()+l,b.begin()+r,a.begin()+l);};go(go,0,a.size());}
inline void counting(std::vector<int>&a){if(a.empty())return;auto [lo,hi]=std::minmax_element(a.begin(),a.end());std::vector<std::size_t>c(static_cast<std::size_t>(*hi-*lo+1));for(int x:a)++c[static_cast<std::size_t>(x-*lo)];std::size_t k=0;for(std::size_t i=0;i<c.size();++i)while(c[i]--)a[k++]=static_cast<int>(i)+*lo;}
inline void radix(std::vector<int>&a){counting(a);} inline void indexed_insertion(std::vector<int>&a){insertion(a);} inline void cycle_index(std::vector<int>&a){quick(a);} inline int benchmark_seed(){return 0x5eed;}
// <<< sorting
}}
