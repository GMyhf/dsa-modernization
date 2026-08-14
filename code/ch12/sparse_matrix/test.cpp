#include "modern.hpp"
#include <cstdio>
int main(){int n=0,f=0;auto c=[&](bool x,const char*s){++n;if(!x){++f;std::printf("FAIL %s\n",s);}};dsa::advanced::SparseMatrix m(4,5);m.set(2,3,7);m.set(0,1,4);m.set(2,0,9);c(m.get(2,3)==7&&m.get(1,1)==0&&m.nonzeros()==3,"三元组读取");int rows=0,sum=0;m.for_each_column(0,[&](std::size_t r,int v){rows+=static_cast<int>(r);sum+=v;});c(rows==2&&sum==9,"十字链表列链接");m.set(2,3,0);c(m.nonzeros()==2&&m.get(2,3)==0,"删除零元");std::printf("SparseMatrix: %d checks, %d failures\n",n,f);return f;}
