#include "modern.hpp"
#include <cstdio>
#include <string>
int main(){int n=0,f=0; auto c=[&](bool x,const char*s){++n;if(!x){++f;std::printf("FAIL %s\n",s);}};
 dsa::DoublyLinkedList<int> x; x.push_back(2);x.push_front(1);x.insert(2,3); c(x.size()==3&&x.at(1)==2,"insert"); c(x.pop_front()==1&&x.pop_back()==3&&x.at(0)==2,"ends"); x.push_back(4); auto y=x; int& y0=y.at(0); y0=9;c(x.at(0)==2&&y.at(0)==9,"deep copy"); auto z=std::move(y);c(z.size()==2&&y.empty(),"move"); int first=z.erase(0);int second=z.erase(0);c(first==9&&second==4&&z.size()==0,"erase empty"); bool bad=false;try{x.at(8);}catch(const std::out_of_range&){bad=true;}c(bad,"bounds"); std::printf("DoublyLinkedList: %d checks, %d failures\n",n,f);return f;}
