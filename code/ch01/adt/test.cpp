#include "modern.hpp"
#include <cstdio>
int main(){dsa::adt::RumorNetwork r;r.tell("A","B");bool ok=r.source_of("B")=="A"&&!r.source_of("C");std::printf("ADT: 1 项断言，%d 失败\n",ok?0:1);return ok?0:1;}
