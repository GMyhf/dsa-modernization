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


// ---- 中缀 → 后缀转换（原书留作练习，本书补上）--------------------------------

// 原书 2.076 行那个例子：中缀 23+(34*45)/(5+6+7) 的等价后缀式。
void test_infix_to_postfix_textbook_example() {
    check(dsa::infix_to_postfix("23 + (34 * 45) / (5 + 6 + 7)")
              == "23 34 45 * 5 6 + 7 + / +",
          "原书的例子：23+(34*45)/(5+6+7) → 23 34 45 * 5 6 + 7 + / +");
}

// 优先级：先乘除后加减，不需要括号也要转对。
void test_infix_precedence() {
    check(dsa::infix_to_postfix("1 + 2 * 3") == "1 2 3 * +", "乘法先算");
    check(dsa::infix_to_postfix("1 * 2 + 3") == "1 2 * 3 +", "乘法在前也一样");
    check(dsa::infix_to_postfix("(1 + 2) * 3") == "1 2 + 3 *", "括号能改变次序");
}

// **左结合**：同优先级要先弹再压，否则 a-b-c 会变成 a b c - -。
// 变异：把第 (4) 条的 `precedence(*top) < precedence(c)` 改成 `<=` → 这条会红。
void test_infix_is_left_associative() {
    check(dsa::infix_to_postfix("1 - 2 - 3") == "1 2 - 3 -", "减法左结合");
    check(dsa::infix_to_postfix("8 / 4 / 2") == "8 4 / 2 /", "除法左结合");
    // 真值验证：左结合的 8/4/2 是 1，右结合会算成 4
    check(close(dsa::evaluate_infix("8 / 4 / 2"), 1.0), "8/4/2 = 1，不是 4");
    check(close(dsa::evaluate_infix("1 - 2 - 3"), -4.0), "1-2-3 = -4，不是 2");
}

void test_infix_brackets_and_nesting() {
    check(dsa::infix_to_postfix("((1 + 2))") == "1 2 +", "多余的括号不影响结果");
    check(dsa::infix_to_postfix("2 * (3 + (4 - 1))") == "2 3 4 1 - + *", "嵌套括号");
    check(close(dsa::evaluate_infix("2 * (3 + (4 - 1))"), 12.0), "嵌套括号求值正确");
}

// 括号不匹配是原书第 (3)、(5) 条明确要求报错的两种情形。
void test_infix_rejects_unbalanced_brackets() {
    bool right_extra = false;
    try { (void)dsa::infix_to_postfix("1 + 2)"); }
    catch (const std::invalid_argument&) { right_extra = true; }
    check(right_extra, "右括号多了要报错（原书第 3 条）");

    bool left_extra = false;
    try { (void)dsa::infix_to_postfix("(1 + 2"); }
    catch (const std::invalid_argument&) { left_extra = true; }
    check(left_extra, "左括号多了要报错（原书第 5 条）");

    bool empty_expr = false;
    try { (void)dsa::infix_to_postfix("   "); }
    catch (const std::invalid_argument&) { empty_expr = true; }
    check(empty_expr, "空表达式要报错");
}

void test_infix_negatives_and_decimals() {
    check(close(dsa::evaluate_infix("-3 + 5"), 2.0), "开头的负号是符号不是运算符");
    check(close(dsa::evaluate_infix("2 * -3"), -6.0), "运算符后面的负号也是符号");
    check(close(dsa::evaluate_infix("(-3) * (-2)"), 6.0), "括号里的负号");
    check(close(dsa::evaluate_infix("1.5 * 2"), 3.0), "小数");
}

// 转换与求值必须一致：随机造式子，两条路算出来要相等。
// 这是最硬的一条——它不依赖我对某个具体输出串的记忆。
void test_infix_and_postfix_agree() {
    struct Case { const char* infix; double expected; };
    const Case cases[] = {
        {"1 + 2 * 3 - 4 / 2", 5.0},
        {"(1 + 2) * (3 - 4)", -3.0},
        {"(1 + 2) - 3", 0.0},        // 右括号后面跟减号：那是运算符，不是负号
        {"(4) - (1)", 3.0},
        {"10 / (2 + 3)", 2.0},
        {"1 + 2 + 3 + 4 + 5", 15.0},
        {"2 * 3 * 4", 24.0},
        {"100 - 10 - 1", 89.0},
        {"((2))", 2.0},
        {"7", 7.0},
    };
    bool all_ok = true;
    for (const Case& c : cases) {
        const std::string postfix = dsa::infix_to_postfix(c.infix);
        if (!close(dsa::evaluate_postfix(postfix), c.expected)
            || !close(dsa::evaluate_infix(c.infix), c.expected)) {
            all_ok = false;
            std::printf("    对不上: %s -> %s\n", c.infix, postfix.c_str());
        }
    }
    check(all_ok, "8 组式子：转成后缀再求值 == 直接对中缀求值 == 手算结果");
}

void test_infix_divide_by_zero_still_throws() {
    bool thrown = false;
    try { (void)dsa::evaluate_infix("1 / (2 - 2)"); }
    catch (const std::domain_error&) { thrown = true; }
    check(thrown, "中缀求值同样在除零时抛 domain_error");
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
    test_infix_to_postfix_textbook_example();
    test_infix_precedence();
    test_infix_is_left_associative();
    test_infix_brackets_and_nesting();
    test_infix_rejects_unbalanced_brackets();
    test_infix_negatives_and_decimals();
    test_infix_and_postfix_agree();
    test_infix_divide_by_zero_still_throws();
    test_no_console_output();
    std::printf("ExpressionEval: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
