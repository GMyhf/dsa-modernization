#include "modern.hpp"
#include <cstdio>
int main(){int n=0,f=0;auto c=[&](bool x,const char*s){++n;if(!x){++f;std::printf("FAIL %s\n",s);}};for(auto fit:{dsa::advanced::Fit::First,dsa::advanced::Fit::Best,dsa::advanced::Fit::Worst}){dsa::advanced::BoundaryAllocator a(100);auto x=a.allocate(20,fit);auto y=a.allocate(30,fit);c(x&&y,"分配");c(a.release(*x),"释放首块");c(!a.release(*x),"重复释放拒绝");auto z=a.allocate(15,fit);c(z.has_value(),"分裂后复用空闲块");c(a.release(*y)&&a.release(*z)&&a.free_bytes()==100,"相邻空闲块合并");}std::printf("BoundaryAllocator: %d checks, %d failures\n",n,f);return f;}
