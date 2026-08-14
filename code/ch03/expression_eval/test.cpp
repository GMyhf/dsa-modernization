// 后缀表达式求值的自带断言测试。
//
// 关键判据：**原书那个"弹掉第一个才发现没有第二个"的 bug 必须被抓到**。
// 只测"能算对 3 4 +"的用例，在原书那份实现下同样全绿。
#include "modern.hpp"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", what.c_str()); }
}
bool close(double a, double b) { return std::fabs(a - b) < 1e-9; }

void test_basic_evaluation() {
    check(close(dsa::evaluate_postfix("3 4 +"), 7), "3 4 + = 7");
    check(close(dsa::evaluate_postfix("3 4 -"), -1), "3 4 - = -1（左右操作数顺序不能反）");
    check(close(dsa::evaluate_postfix("3 4 *"), 12), "3 4 * = 12");
    check(close(dsa::evaluate_postfix("8 2 /"), 4), "8 2 / = 4（除法顺序不能反）");
    check(close(dsa::evaluate_postfix("42"), 42), "单个操作数就是结果");
}

void test_nested_and_precedence_free() {
    // 后缀表达式不需要优先级：(3+4)*2 与 3+(4*2) 的后缀形式本就不同
    check(close(dsa::evaluate_postfix("3 4 + 2 *"), 14), "(3+4)*2 = 14");
    check(close(dsa::evaluate_postfix("3 4 2 * +"), 11), "3+(4*2) = 11");
    check(close(dsa::evaluate_postfix("1 2 + 3 4 + *"), 21), "(1+2)*(3+4) = 21");
    check(close(dsa::evaluate_postfix("5 1 2 + 4 * + 3 -"), 14), "经典例子 5+((1+2)*4)-3 = 14");
}

void test_decimals_and_negatives() {
    check(close(dsa::evaluate_postfix("1.5 2.5 +"), 4.0), "支持小数");
    check(close(dsa::evaluate_postfix("-3 4 +"), 1), "负号不被当成减号");
    check(close(dsa::evaluate_postfix("-3 -4 *"), 12), "两个负数相乘");
    check(close(dsa::evaluate_postfix("  3   4   +  "), 7), "多余空白不影响");
}

// 缺陷 1：原书 GetTwoOperands 先弹一个再检查有没有第二个——失败时栈已被破坏。
void test_missing_operand_is_rejected() {
    int thrown = 0;
    for (const char* expr : {"3 +", "+", "1 2 + *", "3 4 + +"}) {
        try { (void)dsa::evaluate_postfix(expr); }
        catch (const std::invalid_argument&) { ++thrown; }
    }
    check(thrown == 4, "操作数不足一律抛 invalid_argument（原书是 cerr 一行 + 清栈继续跑）");
}

void test_malformed_expressions() {
    int thrown = 0;
    for (const char* expr : {"", "   ", "3 4", "1 2 3 +", "3 x +"}) {
        try { (void)dsa::evaluate_postfix(expr); }
        catch (const std::invalid_argument&) { ++thrown; }
    }
    check(thrown == 5, "空表达式/操作数多余/无法识别的记号都抛 invalid_argument");
}

void test_divide_by_zero() {
    bool thrown = false;
    try { (void)dsa::evaluate_postfix("1 0 /"); }
    catch (const std::domain_error&) { thrown = true; }
    check(thrown, "勘误E19 算法3.5：除零抛 domain_error（原书是 cerr 一行 + 清栈，调用方拿不到信号）");

    // 除以极小值是**合法**的，不该被当成除零——这是本书与原书正文那条
    // "用阈值判断是否为 0" 建议的分歧点，理由写在 modern.hpp 的注释里。
    const double tiny = dsa::evaluate_postfix("1 1e-300 /");
    check(tiny > 1e290, "除以 1e-300 得到一个极大值，而不是被当成错误");
}

// 原书 bug 真正的要害是「出错之后继续跑」。这里钉住的是与之相反的性质：
// **每次求值彼此独立**——上一次失败不会给下一次留下任何残留。
// （注意：单看"操作数不足会抛异常"是抓不住原书那个 bug 的，
//   因为在抛异常即作废的设计里，栈被破坏与否根本观测不到。见 legacy.md 缺陷 1。）
void test_evaluations_are_independent() {
    for (const char* bad : {"3 +", "1 2 + *", "1 0 /", "x"}) {
        try { (void)dsa::evaluate_postfix(bad); } catch (const std::exception&) {}
        // 紧接着来一次正常求值，结果必须完全不受影响
        check(close(dsa::evaluate_postfix("3 4 +"), 7),
              std::string("在 \"") + bad + "\" 失败之后，下一次求值仍然正确");
    }
    // 反过来：成功之后再失败，也不该把成功的结果带出来
    check(close(dsa::evaluate_postfix("10 2 /"), 5), "先做一次成功的求值");
    bool threw = false;
    try { (void)dsa::evaluate_postfix("+"); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "紧接着的非法表达式照样抛异常，不会借用上次的残留");
}

// 缺陷 2：原书 Run() 直接读 cin 写 cout，算法与终端焊死。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    (void)dsa::evaluate_postfix("3 4 +");
    try { (void)dsa::evaluate_postfix("1 0 /"); } catch (const std::domain_error&) {}
    try { (void)dsa::evaluate_postfix("3 +"); } catch (const std::invalid_argument&) {}
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "求值全程不向 cout/cerr 写任何东西");
}

}  // namespace

int main() {
    test_basic_evaluation();
    test_nested_and_precedence_free();
    test_decimals_and_negatives();
    test_missing_operand_is_rejected();
    test_malformed_expressions();
    test_divide_by_zero();
    test_evaluations_are_independent();
    test_no_console_output();
    std::printf("ExpressionEval: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
