#include "modern.hpp"
#include <cstdio>
#include <vector>
int main(){int n=0,b=0;auto c=[&](bool x){++n;if(!x)++b;};dsa::GeneralTree<char>t;t.create_root('A');auto*b1=t.insert_first(t.root(),'B');auto*c1=t.insert_next(b1,'C');t.insert_first(b1,'D');std::vector<char>p,o,w;t.preorder([&](char x){p.push_back(x);});t.postorder([&](char x){o.push_back(x);});t.breadth_first([&](char x){w.push_back(x);});c(p==std::vector<char>({'A','B','D','C'}));c(o==std::vector<char>({'D','B','C','A'}));c(w==std::vector<char>({'A','B','C','D'}));c(t.parent_of(c1)==t.root());t.delete_subtree(b1);c(t.root()->child==c1);auto copy=t;c(copy.root()->value=='A');dsa::DisjointSet s(5);c(s.unite(0,1)&&s.unite(1,2)&&s.same(0,2)&&!s.unite(0,2));std::printf("GeneralTree: %d 项断言，%d 失败\n",n,b);return b;}
