// String 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准同前几章：**如果实现退回原书的写法，这里必须有一条会红**。
// 本单元尤其要盯住三处原书的真缺陷：越界时 return NULL、append 返回副本、
// 有析构却没有拷贝构造/拷贝赋值。
#include "modern.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

void test_construction() {
    dsa::String empty;
    check(empty.empty() && empty.size() == 0, "默认构造是空串");
    check(empty.c_str() != nullptr && std::strcmp(empty.c_str(), "") == 0, "空串的 c_str 可用且为空");

    // 书中 4.2.2 节自己的例子。原书构造函数取 char*，这一句在 C++11 起就是非法转换，
    // 本项目的 -Werror 下直接编译失败（证据见 legacy.md 缺陷 1）。
    dsa::String hello = "Hello";
    check(hello.size() == 5, "从字符串字面量构造，长度为 5");
    check(std::strcmp(hello.c_str(), "Hello") == 0, "内容正确且以 \\0 结尾");

    bool threw = false;
    try {
        dsa::String bad(static_cast<const char*>(nullptr));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "空指针构造抛 invalid_argument，而不是 strlen(nullptr) 崩溃");
}

// 缺陷 4：原书有析构函数却没给出拷贝构造/拷贝赋值清单 → 浅拷贝 → 二次释放。
void test_rule_of_five() {
    dsa::String a = "Hello";
    dsa::String b = a;
    b.append('!');
    check(a.size() == 5 && std::strcmp(a.c_str(), "Hello") == 0, "改副本不影响原串");
    check(b.size() == 6 && std::strcmp(b.c_str(), "Hello!") == 0, "副本自身正确");

    dsa::String c = "x";
    c = a;
    check(std::strcmp(c.c_str(), "Hello") == 0, "拷贝赋值得到独立副本");
    dsa::String& alias = c;
    c = alias;  // 自赋值（经别名避开 -Wself-assign-overloaded）
    check(std::strcmp(c.c_str(), "Hello") == 0, "自赋值后仍然完好");

    dsa::String moved = std::move(a);
    check(std::strcmp(moved.c_str(), "Hello") == 0, "移动后新对象持有内容");
    // 被移动方必须仍是**可用的空串**，而不只是"可析构"。
    check(a.empty(), "被移动方是空串");
    check(a.c_str() != nullptr && std::strcmp(a.c_str(), "") == 0, "被移动方的 c_str 仍可安全读取");
    a.append('z');
    check(std::strcmp(a.c_str(), "z") == 0, "被移动方可以继续使用");

    dsa::String d = "yyy";
    d = std::move(moved);
    check(std::strcmp(d.c_str(), "Hello") == 0, "移动赋值取得对方的内容");
    dsa::String& malias = d;
    d = std::move(malias);  // 自移动赋值不得自毁
    check(d.size() == 5, "自移动赋值后仍然完好");
    // 全部对象在此各自析构一次。原书写法在此处 double-free。
}

// 缺陷 3：原书【算法4.5】在 pos 越界时 `return NULL`——不是返回空串，
// 而是拿 NULL 走 String(char*) 构造，随后 strlen(nullptr) 当场 SEGV。
void test_substr() {
    dsa::String s = "Hello world";
    check(std::strcmp(s.substr(0, 5).c_str(), "Hello") == 0, "从头抽取");
    check(std::strcmp(s.substr(6, 5).c_str(), "world") == 0, "从中间抽取");
    check(std::strcmp(s.substr(6, 999).c_str(), "world") == 0, "len 超出剩余长度时截断（原书的 n = left）");
    check(s.substr(11, 3).empty(), "pos 等于长度得到空串");
    check(s.size() == 11, "substr 不改变原串");

    bool threw = false;
    try {
        (void)s.substr(12, 1);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "勘误E16 算法4.5：pos 越界抛 out_of_range，而不是返回一个必然崩溃的对象");
}

// 缺陷 2（接口形状）：原书【代码4.1】把 append 声明成按值返回 `string append(const char c);`。
// 代码4.1 只有声明没有函数体，所以不能断言"原书会丢结果"——**能断言的是签名含混**：
// 调用方无从判断 s.append('x') 是改了 s 还是返回新串。这里钉死"就地修改"这一语义。
void test_append_mutates_in_place() {
    dsa::String s;
    s.append('a');
    s.append('b');
    check(std::strcmp(s.c_str(), "ab") == 0, "append 就地修改本串（而非返回新串、本串不变）");
    check(s.size() == 2, "长度随之更新");

    s += 'c';
    check(std::strcmp(s.c_str(), "abc") == 0, "operator+= 等价于 append");

    s.concatenate("def");
    check(std::strcmp(s.c_str(), "abcdef") == 0, "concatenate 接在串尾");
    check(s.size() == 6, "拼接后长度正确");

    dsa::String other = "ghi";
    s += other;
    check(std::strcmp(s.c_str(), "abcdefghi") == 0, "operator+= 接另一个 String");

    bool threw = false;
    try {
        s.concatenate(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "拼接空指针抛 invalid_argument");

    // 反复 append 触发多次重新分配，ASan/LSan 会盯住每一次 delete[]
    dsa::String grow;
    for (int i = 0; i < 500; ++i) {
        grow.append(static_cast<char>('a' + i % 26));
    }
    check(grow.size() == 500, "500 次 append 后长度正确");
    check(grow.at(499) == static_cast<char>('a' + 499 % 26), "末字符正确");
}

void test_at_and_find() {
    dsa::String s = "abcabc";
    check(s.at(0) == 'a' && s.at(5) == 'c', "at 按下标取字符");
    bool threw = false;
    try {
        (void)s.at(6);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "at 越界抛 out_of_range");

    check(s.find('b') == std::optional<dsa::String::size_type>(1), "find 返回首次出现的下标");
    check(s.find('b', 2) == std::optional<dsa::String::size_type>(4), "find 从 start 开始找");
    check(!s.find('z').has_value(), "找不到返回 nullopt");
    check(!s.find('a', 99).has_value(), "start 超出长度时返回 nullopt，不越界读");
    // 原书 `int find(...)` 用 -1 表示没找到，与"位置 0"只差一个符号。
    check(s.find('a') == std::optional<dsa::String::size_type>(0), "位置 0 与「没找到」不再混淆");
}

// 【算法4.3】原书自己实现的 strcmp 固定返回 -1/0/1，并在正文里说这与常规习惯不一致。
// 这里保持标准语义：只看符号。
void test_compare_and_relational() {
    dsa::String a = "abc";
    dsa::String b = "abd";
    dsa::String c = "abc";
    check(a.compare(c) == 0, "相等时 compare 返回 0");
    check(a.compare(b) < 0, "小于时 compare 为负");
    check(b.compare(a) > 0, "大于时 compare 为正");
    check(a == c && a != b, "相等/不等运算符");
    check(a < b && b > a && a <= c && a >= c, "关系运算符");

    dsa::String empty;
    check(empty < a, "空串小于任何非空串");
    check(empty == dsa::String(), "两个空串相等");

    // 前缀关系：短串小于以它为前缀的长串
    check(dsa::String("ab") < dsa::String("abc"), "前缀短串更小");
}

void test_clear() {
    dsa::String s = "something";
    s.clear();
    check(s.empty() && s.size() == 0, "clear 后为空");
    check(s.c_str() != nullptr && std::strcmp(s.c_str(), "") == 0, "clear 后 c_str 仍可用");
    s.append('x');
    check(std::strcmp(s.c_str(), "x") == 0, "clear 后仍可继续使用");
}

// 容器内不做 I/O。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    {
        dsa::String s = "abc";
        try { (void)s.substr(99, 1); } catch (const std::out_of_range&) {}
        try { (void)s.at(99); } catch (const std::out_of_range&) {}
        try { s.concatenate(nullptr); } catch (const std::invalid_argument&) {}
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "String 全程不向 cout/cerr 写任何东西");
}

}  // namespace

int main() {
    test_construction();
    test_rule_of_five();
    test_substr();
    test_append_mutates_in_place();
    test_at_and_find();
    test_compare_and_relational();
    test_clear();
    test_no_console_output();

    std::printf("String: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
