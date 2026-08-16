// 教学版 ArrayList 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
// 教学版比工程版少了移动语义与强异常保证，那两块由 test.cpp 守；
// 这里守的是教学版自己承诺的东西——O(1) 随机存取、插入/删除的搬迁方向、
// 翻倍扩容、深拷贝、越界抛异常、查找返回 optional，以及容器内零 I/O。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what);
    }
}

void test_append_and_random_access() {
    ArrayList<int> list;
    list.append(10);
    list.append(20);
    list.append(30);
    check(list.size() == 3, "append 三次后 size==3");
    check(list.at(0) == 10, "下标 0 是第一个追加的");
    check(list.at(2) == 30, "下标 2 是最后追加的");
    check(!list.empty(), "非空表 empty 为假");
}

// 【算法2.4】插入：pos 之后的元素整体右移一位，次序不能乱。
// 变异：把 make-gap 的循环方向改成从前往后（i 递增）→ 元素被自己覆盖，这条会红。
void test_insert_shifts_right() {
    ArrayList<int> list;
    list.append(1);
    list.append(3);
    list.insert(1, 2);          // 插到中间
    check(list.size() == 3, "插入后长度加一");
    check(list.at(0) == 1 && list.at(1) == 2 && list.at(2) == 3, "插入后次序是 1 2 3");
    list.insert(0, 0);          // 插到表头
    check(list.at(0) == 0 && list.at(3) == 3, "表头插入把整表右移");
    list.insert(list.size(), 4);  // pos == size() 合法，等于追加
    check(list.at(4) == 4, "pos 等于 size 时插到表尾");
}

// 【算法2.5】删除：pos 之后的元素整体左移一位。
void test_remove_shifts_left() {
    ArrayList<int> list;
    for (int i = 0; i < 5; ++i) {
        list.append(i);
    }
    check(list.remove(1) == 1, "remove 返回被删掉的那个元素");
    check(list.size() == 4, "删除后长度减一");
    check(list.at(0) == 0 && list.at(1) == 2 && list.at(2) == 3 && list.at(3) == 4,
          "删除后剩下 0 2 3 4，次序不乱");
}

// 原书用「出参 + bool」，忘了看 bool 就会读到没被写过的变量。
void test_find_returns_optional() {
    ArrayList<std::string> list;
    list.append("a");
    list.append("b");
    auto hit = list.find("b");
    check(hit.has_value(), "找得到时 optional 有值");
    check(hit.value() == 1, "找到的下标是 1");
    check(!list.find("zzz").has_value(), "找不到时返回 nullopt");
}

// 下标非法是调用方的错误，不是可预期状态：抛 std::out_of_range，不是打印一行再返回 false。
void test_out_of_range_throws() {
    ArrayList<int> list;
    list.append(1);

    bool thrown = false;
    try {
        (void)list.at(5);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "at 越界抛 out_of_range");

    thrown = false;
    try {
        list.insert(99, 7);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "insert 位置非法抛 out_of_range");

    thrown = false;
    try {
        ArrayList<int> empty_list;
        (void)empty_list.remove(0);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "空表上 remove 抛 out_of_range");
}

// 翻倍扩容，内容一个不少、次序一个不乱。
// 变异：把 grow() 里的 delete[] 挪到拷贝循环之前 → ASan heap-use-after-free。
void test_growth_preserves_contents() {
    ArrayList<int> list(1);
    for (int i = 0; i < 200; ++i) {
        list.append(i);
    }
    check(list.size() == 200, "连续 append 200 次");
    check(list.capacity() >= 200, "容量已经涨上去了");
    bool all_ok = true;
    for (int i = 0; i < 200; ++i) {
        if (list.at(static_cast<std::size_t>(i)) != i) {
            all_ok = false;
        }
    }
    check(all_ok, "扩容后 200 个元素逐个原样保留");
}

// 摊还 O(1) 的前提是翻倍。变异：capacity_ * 2 改成 capacity_ + 1 → 这条会红。
void test_growth_is_doubling() {
    ArrayList<int> list(4);
    for (int i = 0; i < 5; ++i) {
        list.append(i);
    }
    check(list.capacity() == 8, "容量从 4 翻倍到 8，不是加一");
}

// 原书 arrList 有析构却没有拷贝构造 → 二次释放。
// 变异实测：删掉拷贝构造，`ArrayList<int> b = a;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    ArrayList<int> a;
    a.append(1);
    a.append(2);
    ArrayList<int> b = a;
    check(b.size() == 2, "副本长度一致");
    check(b.at(1) == 2, "副本内容一致");
    a.append(3);
    check(b.size() == 2, "改动原表不影响副本");
    b.append(9);
    check(a.size() == 3, "改动副本不影响原表");
}

void test_copy_assignment_is_deep() {
    ArrayList<std::string> a;
    a.append("x");
    a.append("y");
    ArrayList<std::string> b;
    b.append("zzz");
    b = a;
    check(b.size() == 2, "赋值后长度取自右边");
    check(b.at(0) == std::string("x"), "赋值后内容取自右边");
    check(a.size() == 2, "赋值不改动右边");
}

void test_self_assignment_is_safe() {
    ArrayList<int> list;
    list.append(7);
    list.append(8);
    list = list;
    check(list.size() == 2, "自赋值后长度不变");
    check(list.at(0) == 7 && list.at(1) == 8, "自赋值后内容不变");
}

// 游标不住在容器里：const 表也能遍历，两处遍历互不干扰，嵌套遍历不打架。
// 原书的 setPos/next/prev 把 position 放进类里，这三件事一件也做不到。
void test_range_for_and_nested_traversal() {
    ArrayList<int> list;
    for (int i = 1; i <= 3; ++i) {
        list.append(i);
    }
    int sum = 0;
    for (int x : list) {
        sum += x;
    }
    check(sum == 6, "range-for 遍历得到 1+2+3");

    const ArrayList<int>& ref = list;
    int const_sum = 0;
    for (int x : ref) {
        const_sum += x;
    }
    check(const_sum == 6, "const 表也能遍历");

    int pairs = 0;
    for (int x : list) {
        for (int y : list) {
            pairs += (x == y) ? 1 : 0;
        }
    }
    check(pairs == 3, "嵌套遍历互不干扰");
}

void test_clear_keeps_capacity() {
    ArrayList<int> list(4);
    for (int i = 0; i < 10; ++i) {
        list.append(i);
    }
    std::size_t before = list.capacity();
    list.clear();
    check(list.empty(), "clear 之后为空");
    check(list.capacity() == before, "clear 不释放已分配的容量");
    list.append(42);
    check(list.at(0) == 42, "clear 之后还能继续用");
}

void test_set_overwrites_in_place() {
    ArrayList<int> list;
    list.append(1);
    list.append(2);
    list.set(0, 100);
    check(list.at(0) == 100, "set 改写指定位置");
    check(list.size() == 2, "set 不改变长度");
}

// D-001 第 3 条红线：容器内零 I/O。原书溢出/越界时直接 cout 打中文提示。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        ArrayList<int> list(1);
        list.append(1);
        list.append(2);          // 触发扩容
        list.insert(0, 0);
        (void)list.remove(0);
        (void)list.find(999);
        try {
            (void)list.at(99);   // 越界：原书在这里打印
        } catch (const std::out_of_range&) {
        }
        ArrayList<int> copy = list;
        copy = list;
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "容器没有往 stdout 打任何东西");
    check(err.str().empty(), "容器没有往 stderr 打任何东西");
}

void test_default_constructed_is_usable() {
    ArrayList<int> list;
    check(list.empty(), "默认构造是空表");
    check(list.capacity() > 0, "默认构造已经有容量");
    list.append(1);
    check(list.at(0) == 1, "默认构造的表可以直接 append");
}

}  // namespace

int main() {
    test_append_and_random_access();
    test_insert_shifts_right();
    test_remove_shifts_left();
    test_find_returns_optional();
    test_out_of_range_throws();
    test_growth_preserves_contents();
    test_growth_is_doubling();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_range_for_and_nested_traversal();
    test_clear_keeps_capacity();
    test_set_overwrites_in_place();
    test_no_console_output();
    test_default_constructed_is_usable();

    std::printf("ArrayList(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
