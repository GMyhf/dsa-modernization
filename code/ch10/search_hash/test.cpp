#include "modern.hpp"
#include <cstdio>
int main(){int n=0,b=0;auto c=[&](bool x){++n;if(!x)++b;};std::vector<int>a={1,3,5,7};c(dsa::search::sequential(a,5)==2&&dsa::search::binary(a,6)==std::nullopt);dsa::search::IntSet x,y;x.insert(1);x.insert(2);y.insert(2);c(x.intersection(y).size()==1&&x.includes(x));dsa::search::HashTable h(3);c(h.insert(1)&&h.insert(4)&&h.contains(4)&&h.erase(1)&&!h.contains(1)&&h.insert(7));c(dsa::search::elf_hash("abc")!=0);std::printf("SearchHash: %d 项断言，%d 失败\n",n,b);return b;}
