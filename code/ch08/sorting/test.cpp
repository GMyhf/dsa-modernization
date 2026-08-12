#include "modern.hpp"
#include <cstdio>
#include <vector>
int main(){int n=0,b=0;auto c=[&](bool x){++n;if(!x)++b;};using F=void(*)(std::vector<int>&);F fs[]={dsa::sorting::insertion,dsa::sorting::shell,dsa::sorting::selection,dsa::sorting::heap,dsa::sorting::bubble,dsa::sorting::quick,dsa::sorting::merge,dsa::sorting::counting,dsa::sorting::radix,dsa::sorting::indexed_insertion,dsa::sorting::cycle_index};for(std::size_t i=0;i<sizeof(fs)/sizeof(fs[0]);++i){std::vector<int>a={3,-2,7,3,0,-2,9,1};fs[i](a);bool ok=a==std::vector<int>({-2,-2,0,1,3,3,7,9});if(!ok)std::printf("  FAIL: sorting #%zu\n",i+1);c(ok);}std::printf("Sorting: %d 项断言，%d 失败\n",n,b);return b;}
