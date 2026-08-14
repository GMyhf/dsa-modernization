#include "modern.hpp"
#include <cstdio>
int main(){int n=0,f=0;auto c=[&](bool x,const char*s){++n;if(!x){++f;std::printf("FAIL %s\n",s);}};dsa::advanced::AvlTree a;for(int x:{3,2,1,4,5,6,7})a.insert(x);c(a.height()<=3&&a.contains(5)&&a.contains(1),"AVL balance/search");a.erase(4);c(!a.contains(4)&&a.height()<=3,"AVL erase");dsa::advanced::SplayTree s;for(int x:{5,3,7,2,4,6,8})s.insert(x);c(s.contains(2)&&s.root_key()==2,"splay hit root");c(s.contains(7)&&s.root_key()==7,"splay second hit");std::printf("BalancedTrees: %d checks, %d failures\n",n,f);return f;}
