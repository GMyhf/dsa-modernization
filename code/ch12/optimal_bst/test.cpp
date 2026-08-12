#include "modern.hpp"
#include <cstdio>
int main(){auto r=dsa::advanced::optimal_bst({1,5,4,3},{5,4,3,2,1});bool ok=r.cost[0][4]>0&&r.root[0][4]>=1&&r.root[0][4]<=4;std::printf("OptimalBST: 1 项断言，%d 失败\n",ok?0:1);return ok?0:1;}
