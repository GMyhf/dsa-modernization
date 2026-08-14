#include "modern.hpp"

#include <cstdio>

int main() {
    using dsa::advanced::GenList;
    const GenList list = GenList::parse("(a,(b,c),d)");
    std::printf("表      : %s\n", list.to_string().c_str());
    std::printf("表头    : %s\n", list.head()->to_string().c_str());
    std::printf("表尾    : %s\n", list.tail()->to_string().c_str());
    std::printf("长度 %zu，深度 %zu，原子 %zu 个\n",
                list.length(), list.depth(), list.atom_count());

    // 再入表：同一个子表挂到两处，靠引用计数而不是拷贝。
    const GenList shared = GenList::parse("(b,c)");
    const GenList host = GenList::cons(shared, GenList::cons(shared, GenList()));
    std::printf("共享后  : %s，被引用 %zu 次\n", host.to_string().c_str(), shared.use_count());
    return 0;
}
