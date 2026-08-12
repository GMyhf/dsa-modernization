#include "modern.hpp"
#include <cstdio>
int main(){int n=0,b=0;auto c=[&](bool x){++n;if(!x)++b;};c(dsa::external_sort::replacement_selection({3,1,2})==std::vector<int>({1,2,3}));dsa::external_sort::WinnerTree w({4,1,3});c(w.winner()==1&&w.winner()==3&&w.winner()==4&&!w.winner());std::printf("ExternalSort: %d 项断言，%d 失败\n",n,b);return b;}
