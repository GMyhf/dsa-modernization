// 教学版 String 的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

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

// 原书缺陷：`String(char* s)` 让书里自己的例子 `String s1 = "Hello";`
// 从 C++11 起编译不过——字面量是 const char[6]，绑不到 char*。
// 变异：把参数改回 `char*` → 下面这一行编译即红。
void test_construct_from_literal() {
    String s = "Hello";                 // 原书自己的例子，必须能编译
    check(s.size() == 5, "\"Hello\" 长度是 5");
    check(std::strcmp(s.c_str(), "Hello") == 0, "内容一致");
    check(!s.empty(), "非空串 empty 为假");
}

// 空串也持有一块 1 字节缓冲区，c_str() 永不返回空指针。
// 变异：默认构造改成 data_ = nullptr → strlen(c_str()) 当场 ASan 报错。
void test_default_is_usable_empty_string() {
    String s;
    check(s.empty(), "默认构造是空串");
    check(s.size() == 0, "空串长度 0");
    check(s.c_str() != nullptr, "空串的 c_str() 不是空指针");
    check(std::strlen(s.c_str()) == 0, "空串的 c_str() 指向一个合法的空 C 字符串");
}

void test_null_pointer_is_rejected() {
    bool thrown = false;
    try {
        const char* nothing = nullptr;
        String s(nothing);
        (void)s;
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "用空指针构造抛 invalid_argument，而不是 strlen(nullptr) 段错误");
}

// 变长存储：append 每次都要重新申请、拷贝、释放。内容和长度都要对。
void test_append_grows_the_buffer() {
    String s = "ab";
    s.append('c');
    check(s.size() == 3, "append 之后长度加一");
    check(std::strcmp(s.c_str(), "abc") == 0, "append 之后内容是 abc");
    check(s.c_str()[3] == '\0', "结尾的 '\\0' 还在");
    s.append('d').append('e');          // 返回自身引用，可以连着写
    check(std::strcmp(s.c_str(), "abcde") == 0, "链式 append 结果是 abcde");
    check(s.size() == 5, "链式 append 之后长度是 5");
}

void test_append_on_empty_string() {
    String s;
    s.append('x');
    check(s.size() == 1, "空串 append 之后长度 1");
    check(std::strcmp(s.c_str(), "x") == 0, "空串 append 之后内容是 x");
}

void test_concatenate() {
    String s = "foo";
    s.concatenate("bar");
    check(s.size() == 6, "拼接后长度 6");
    check(std::strcmp(s.c_str(), "foobar") == 0, "拼接后内容是 foobar");
    s.concatenate("");
    check(s.size() == 6, "拼接空串不改变长度");

    bool thrown = false;
    try {
        s.concatenate(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "拼接空指针抛 invalid_argument");
}

// 【算法4.5】原书越界时 `return NULL`，随后 strlen(nullptr) 段错误。
// 变异：把 substr 的越界检查删掉 → 这一条不再抛，红。
void test_substr() {
    String s = "abcdef";
    check(std::strcmp(s.substr(2, 3).c_str(), "cde") == 0, "substr(2,3) 是 cde");
    check(std::strcmp(s.substr(0, 100).c_str(), "abcdef") == 0, "len 超出剩余长度时截断，不越界");
    check(s.substr(6, 3).empty(), "pos == size() 合法，得到空串");
    check(s.substr(2, 0).empty(), "取 0 个字符得到空串");
    check(s.size() == 6, "substr 不改动原串");

    bool thrown = false;
    try {
        (void)s.substr(7, 1);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "起始位置越界抛 out_of_range，而不是返回 NULL");
}

// 【算法4.4】原书用 -1 表示没找到，与「位置 0」只差一个符号。
void test_find_returns_optional() {
    String s = "banana";
    auto first = s.find('a');
    check(first.has_value() && first.value() == 1, "第一个 a 在位置 1");
    auto second = s.find('a', 2);
    check(second.has_value() && second.value() == 3, "从位置 2 起找，下一个 a 在 3");
    check(!s.find('z').has_value(), "找不到返回 nullopt");

    String b = "banana";
    auto head = b.find('b');
    check(head.has_value() && head.value() == 0, "位置 0 也是一个真实的结果，不是「没找到」");
}

// 【算法4.3】比较只看符号，不看具体数值。
void test_compare_and_operators() {
    String a = "abc";
    String b = "abd";
    String c = "abc";
    check(a.compare(c) == 0, "相等时 compare 返回 0");
    check(a.compare(b) < 0, "abc < abd");
    check(b.compare(a) > 0, "abd > abc");
    check(a == c, "operator== 认得相等");
    check(a != b, "operator!= 认得不等");
    check(a < b, "operator< 认得小于");

    String empty_string;
    check(empty_string.compare(a) < 0, "空串小于任何非空串");
}

void test_at_and_out_of_range() {
    String s = "xyz";
    check(s.at(0) == 'x', "at(0) 是 x");
    check(s.at(2) == 'z', "at(2) 是 z");
    bool thrown = false;
    try {
        (void)s.at(3);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    check(thrown, "at 越界抛 out_of_range");
}

// 原书只有析构没有拷贝构造 → 二次释放。
// 变异实测：删掉拷贝构造，`String b = a;` 在 -Werror 下先撞 -Wdeprecated-copy
// 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    String a = "hello";
    String b = a;
    check(b.size() == 5, "副本长度一致");
    check(std::strcmp(b.c_str(), "hello") == 0, "副本内容一致");
    check(b.c_str() != a.c_str(), "副本持有自己的缓冲区，不是同一根指针");
    a.append('!');
    check(b.size() == 5, "改动原串不影响副本");
    check(a.size() == 6, "原串自己改到了");
}

void test_copy_assignment_is_deep() {
    String a = "abc";
    String b = "0123456789";           // 故意比 a 长，考验「先备好新的再释放旧的」
    b = a;
    check(b.size() == 3, "赋值后长度取自右边");
    check(std::strcmp(b.c_str(), "abc") == 0, "赋值后内容取自右边");
    check(b.c_str() != a.c_str(), "赋值后仍是各自的缓冲区");
    b.append('!');
    check(a.size() == 3, "改动左边不影响右边");
}

void test_self_assignment_is_safe() {
    String s = "keepme";
    s = s;
    check(s.size() == 6, "自赋值后长度不变");
    check(std::strcmp(s.c_str(), "keepme") == 0, "自赋值后内容不变");
}

void test_clear_then_reuse() {
    String s = "something";
    s.clear();
    check(s.empty(), "clear 之后是空串");
    check(std::strlen(s.c_str()) == 0, "clear 之后 c_str() 仍是合法空串");
    s.append('a');
    check(std::strcmp(s.c_str(), "a") == 0, "clear 之后还能继续用");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        String s = "abc";
        s.append('d');
        s.concatenate("ef");
        (void)s.find('z');
        try {
            (void)s.at(99);
        } catch (const std::out_of_range&) {
        }
        try {
            (void)s.substr(99, 1);
        } catch (const std::out_of_range&) {
        }
        String copy = s;
        copy = s;
        s.clear();
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "字符串类没有往 stdout 打任何东西");
    check(err.str().empty(), "字符串类没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_construct_from_literal();
    test_default_is_usable_empty_string();
    test_null_pointer_is_rejected();
    test_append_grows_the_buffer();
    test_append_on_empty_string();
    test_concatenate();
    test_substr();
    test_find_returns_optional();
    test_compare_and_operators();
    test_at_and_out_of_range();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_clear_then_reuse();
    test_no_console_output();

    std::printf("String(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
