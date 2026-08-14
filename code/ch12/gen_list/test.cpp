#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

using dsa::advanced::GenList;

void test_parse_and_print() {
    const GenList list = GenList::parse("(a,(b,c),d)");
    check(list.to_string() == "(a,(b,c),d)", "12.2.1 书面形式往返");
    check(list.length() == 3, "12.2.1 顶层长度是 3（子表算一个元素）");
    check(list.atom_count() == 4, "12.2.1 原子共 4 个");
    check(list.depth() == 2, "12.2.1 深度是 2");
    check(GenList::parse("()").to_string() == "()", "12.2.1 空表");
    check(GenList::parse("()").depth() == 1, "12.2.1 空表深度 1");
    check(GenList::parse("((((a))))").depth() == 4, "12.2.1 深嵌套深度");
    check(GenList::parse("( a , ( b ) )").to_string() == "(a,(b))", "12.2.1 忽略空格");
}

void test_head_and_tail() {
    // 原书 12.2.1：任何非空广义表都能唯一拆成表头和表尾。
    const GenList list = GenList::parse("(a,(b,c),d)");
    const auto head = list.head();
    const auto tail = list.tail();
    check(head.has_value() && head->is_atom() && head->value() == 'a', "12.2.1 表头是 a");
    check(tail.has_value() && tail->to_string() == "((b,c),d)", "12.2.1 表尾是剩下的表");
    // 拆到底再拼回来，必须还原成原来的表。
    check(GenList::cons(*head, *tail).to_string() == "(a,(b,c),d)", "12.2.1 头尾可以拼回去");

    const auto second = tail->head();
    check(second.has_value() && !second->is_atom() && second->to_string() == "(b,c)",
          "12.2.1 第二个元素本身是子表");

    const GenList empty;
    check(empty.is_empty() && !empty.head().has_value() && !empty.tail().has_value(),
          "12.2.1 空表既没有头也没有尾");
    const GenList single = GenList::parse("(a)");
    check(single.tail().has_value() && single.tail()->is_empty(), "12.2.1 单元素表的尾是空表");
}

void test_atom_interface() {
    const GenList a = GenList::atom('x');
    check(a.is_atom() && a.value() == 'x', "12.2.1 原子存值");
    check(a.depth() == 0 && a.atom_count() == 1, "12.2.1 原子深度 0");
    check(!a.head().has_value(), "12.2.1 原子没有表头");

    bool threw = false;
    try {
        (void)GenList::parse("(a)").value();  // 表不是原子
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "12.2.1 对表取 value() 抛异常");

    threw = false;
    try {
        (void)GenList::cons(GenList::atom('a'), GenList::atom('b'));  // 表尾必须是表
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "12.2.1 表尾不能是原子");

    for (const char* bad : {"(a", "(a,)", "a b", "(,)", ""}) {
        threw = false;
        try {
            (void)GenList::parse(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "12.2.1 残缺输入被拒绝");
    }
}

void test_sharing_is_counted_not_copied() {
    // 再入表：同一个子表挂在两处。原书 12.2 的难点就在这里——按树递归 delete 会二次释放。
    const GenList shared = GenList::parse("(b,c)");
    check(shared.use_count() == 1, "12.2.1 未共享时计数为 1");
    {
        const GenList first = GenList::cons(shared, GenList());
        const GenList second = GenList::cons(shared, GenList());
        check(shared.use_count() == 3, "12.2.1 挂到两处后计数为 3");
        check(first.to_string() == "((b,c))" && second.to_string() == "((b,c))",
              "12.2.1 两处看到同一个子表");
        check(first.head()->use_count() == 4, "12.2.1 取表头会再加一次计数");
    }
    // 两个宿主都析构了，被共享的子表必须还活着——它自己还持有一份。
    check(shared.use_count() == 1, "12.2.1 宿主析构后计数回到 1");
    check(shared.to_string() == "(b,c)", "12.2.1 共享子表在宿主析构后仍然可用");
}

void test_value_semantics() {
    GenList original = GenList::parse("(a,(b))");
    GenList copy = original;
    check(copy.to_string() == "(a,(b))", "12.2.1 拷贝构造");
    check(original.use_count() == 2, "12.2.1 拷贝共享同一份存储");

    GenList moved = std::move(copy);
    check(moved.to_string() == "(a,(b))", "12.2.1 移动构造");
    check(original.use_count() == 2, "12.2.1 移动不改变引用总数");

    GenList assigned;
    assigned = original;
    check(assigned.to_string() == "(a,(b))" && original.use_count() == 3, "12.2.1 拷贝赋值");

    // 走别名，避免编译器把自赋值直接优化/警告掉——要测的是运行期语义。
    GenList& alias = assigned;
    assigned = alias;
    check(assigned.to_string() == "(a,(b))", "12.2.1 自赋值不炸");

    GenList target = GenList::parse("(z)");
    target = std::move(moved);
    check(target.to_string() == "(a,(b))", "12.2.1 移动赋值会释放原有的表");

    GenList self_move = GenList::parse("(q)");
    GenList& self_alias = self_move;
    self_move = std::move(self_alias);
    check(self_move.to_string() == "(q)", "12.2.1 自移动不炸");
}

void test_long_list_does_not_recurse_on_tail() {
    // 表尾方向用循环释放：三万个元素的长表析构时不该把栈压穿。
    GenList list;
    for (int i = 0; i < 30000; ++i) {
        list = GenList::cons(GenList::atom('a'), list);
    }
    check(list.length() == 30000, "12.2.1 长表长度");
    check(list.atom_count() == 30000, "12.2.1 长表原子数");
}
}  // namespace

int main() {
    test_parse_and_print();
    test_head_and_tail();
    test_atom_interface();
    test_sharing_is_counted_not_copied();
    test_value_semantics();
    test_long_list_does_not_recurse_on_tail();
    std::printf("GenList: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
